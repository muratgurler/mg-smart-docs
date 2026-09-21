#include "MapWebEditor.h"

#include "CableNetGraph.h"

#include <ctype.h>
#include <LittleFS.h>

namespace mg::p4 {

namespace {
constexpr size_t kMaxDocumentUploadBytes = 256U * 1024U;
constexpr const char* kDocumentDir = "/mgdocs";
constexpr const char* kDocumentManifestPath = "/mgdocs/session_manifest.json";
}

MapWebEditor::MapWebEditor() : server_(80) {}

void MapWebEditor::begin(CableMap& map,
                         CableMapStore& store,
                         MgNetworkManager& network) {
    map_ = &map;
    store_ = &store;
    network_ = &network;
    if (started_) {
        return;
    }

    server_.on("/", HTTP_GET, [this]() {
        if (documentPortalActive_) {
            redirectToDocumentPortal();
        } else {
            redirectToMap();
        }
    });
    server_.on("/map", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/map/graph", HTTP_GET, [this]() { handleGraph(); });
    server_.on("/map/save", HTTP_POST, [this]() { handleSave(); });
    server_.on("/map/one", HTTP_POST, [this]() { handleOneToOne(); });
    server_.on("/map/clear", HTTP_POST, [this]() { handleClear(); });

    server_.on("/doc", HTTP_GET, [this]() { handleDocumentPortal(); });
    server_.on("/doc/upload", HTTP_POST,
               [this]() { handleDocumentUploadComplete(); },
               [this]() { handleDocumentUploadChunk(); });
    // Companion-app path: resumable multipart chunks remain supported, although
    // prepared profile JSON is normally small. This keeps transport robust.
    server_.on("/doc/chunk", HTTP_POST,
               [this]() { handleDocumentChunkUploadComplete(); },
               [this]() { handleDocumentChunkUploadChunk(); });
    // Stable companion-app API. Session discovery is available only while
    // the temporary AP is active; upload calls then use the returned token.
    server_.on("/api/profile/session", HTTP_GET, [this]() { handleProfileApiSession(); });
    // The token query parameter uses the same temporary-session authentication
    // as the captive portal.
    server_.on("/api/profile/upload", HTTP_POST,
               [this]() { handleDocumentUploadComplete(); },
               [this]() { handleDocumentUploadChunk(); });
    server_.on("/api/profile/chunk", HTTP_POST,
               [this]() { handleDocumentChunkUploadComplete(); },
               [this]() { handleDocumentChunkUploadChunk(); });
    server_.on("/doc/delete", HTTP_POST, [this]() { handleDocumentDelete(); });

    // Common Android/iOS/Windows captive-portal probes. During a document
    // session all of them land on the upload page instead of the Internet.
    server_.on("/generate_204", HTTP_GET, [this]() { handleCaptiveProbe(); });
    server_.on("/gen_204", HTTP_GET, [this]() { handleCaptiveProbe(); });
    server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handleCaptiveProbe(); });
    server_.on("/library/test/success.html", HTTP_GET, [this]() { handleCaptiveProbe(); });
    server_.on("/ncsi.txt", HTTP_GET, [this]() { handleCaptiveProbe(); });
    server_.on("/connecttest.txt", HTTP_GET, [this]() { handleCaptiveProbe(); });
    server_.on("/redirect", HTTP_GET, [this]() { handleCaptiveProbe(); });
    server_.on("/fwlink", HTTP_GET, [this]() { handleCaptiveProbe(); });
    server_.onNotFound([this]() {
        if (documentPortalActive_) {
            redirectToDocumentPortal();
        } else {
            server_.send(404, "text/plain; charset=utf-8", "MG Smart Test - page not found");
        }
    });
    server_.begin();
    started_ = true;
    Serial.println("[MAP-WEB] HTTP editor registered on /map");
    Serial.flush();
}

void MapWebEditor::tick() {
    if (started_) {
        server_.handleClient();
    }
}

bool MapWebEditor::available() const {
    return network_ != nullptr &&
           (network_->wifiConnected() || network_->ethernetHasIp());
}

String MapWebEditor::url() const {
    if (network_ == nullptr) {
        return String("-");
    }
    if (network_->wifiConnected()) {
        return String("http://") + network_->wifiIp() + "/map";
    }
    if (network_->ethernetHasIp()) {
        return String("http://") + network_->ethernetIp() + "/map";
    }
    return String("-");
}

bool MapWebEditor::consumeChanged() {
    const bool value = changed_;
    changed_ = false;
    return value;
}

bool MapWebEditor::ensureDocumentStorage() {
    if (!documentFsReady_) {
        documentFsReady_ = LittleFS.begin(true);
    }
    if (documentFsReady_ && !LittleFS.exists(kDocumentDir)) {
        LittleFS.mkdir(kDocumentDir);
    }
    return documentFsReady_;
}

void MapWebEditor::resetDocumentItems(bool deleteFiles) {
    resetDocumentChunkTransfer(deleteFiles);
    if (documentUploadFile_) {
        documentUploadFile_.close();
    }
    if (deleteFiles && ensureDocumentStorage()) {
        for (uint8_t index = 0; index < documentItemCount_; ++index) {
            if (!documentItems_[index].path.isEmpty() && LittleFS.exists(documentItems_[index].path)) {
                LittleFS.remove(documentItems_[index].path);
            }
        }
        // Remove any stale prepared-profile file left by an interrupted session.
        for (unsigned index = 1; index <= 128U; ++index) {
            char path[48] = {};
            snprintf(path, sizeof(path), "/mgdocs/profile_%03u.json", index);
            if (LittleFS.exists(path)) LittleFS.remove(path);
        }
        if (LittleFS.exists(kDocumentManifestPath)) {
            LittleFS.remove(kDocumentManifestPath);
        }
    }
    for (uint8_t index = 0; index < kMaxDocumentItems; ++index) {
        documentItems_[index] = DocumentItemInfo{};
    }
    documentUploadPath_ = String();
    pendingDocumentName_ = String();
    pendingDocumentKind_ = DocumentFileKind::Unknown;
    pendingDocumentSize_ = 0U;
    pendingDocumentCrc32_ = 0xFFFFFFFFUL;
    documentNextFileId_ = 1U;
    documentItemCount_ = 0U;
    documentLastCompletedTransferId_ = String();
    documentLastCompletedParts_ = 0U;
    lastDocumentName_ = String();
    lastDocumentSize_ = 0U;
}

bool MapWebEditor::startDocumentPortal() {
    if (network_ == nullptr) {
        lastDocumentError_ = "Network manager unavailable";
        return false;
    }
    if (!network_->startDocumentAp(600U)) {
        lastDocumentError_ = "Temporary Wi-Fi AP could not start";
        return false;
    }
    if (!ensureDocumentStorage()) {
        lastDocumentError_ = "LittleFS unavailable";
        network_->stopDocumentAp();
        return false;
    }

    // A fresh portal session intentionally starts with an empty document set.
    // Remove files from the previous session so temporary phone uploads cannot fill flash.
    resetDocumentItems(true);
    documentPortalActive_ = true;
    documentPortalAccepted_ = false;
    lastDocumentError_ = String();
    writeDocumentManifest();
    Serial.printf("[MOBILE-PROFILE] active url=%s\n", documentPortalUrl().c_str());
    Serial.flush();
    return true;
}

bool MapWebEditor::resumeDocumentPortal() {
    if (network_ == nullptr) {
        lastDocumentError_ = "Network manager unavailable";
        return false;
    }
    if (network_->documentApActive()) {
        documentPortalActive_ = true;
        network_->touchDocumentApSession();
        return true;
    }
    if (!ensureDocumentStorage()) {
        lastDocumentError_ = "LittleFS unavailable";
        return false;
    }
    if (!network_->startDocumentAp(600U)) {
        lastDocumentError_ = "Temporary Wi-Fi AP could not resume";
        return false;
    }
    documentPortalActive_ = true;
    documentPortalAccepted_ = false;
    lastDocumentError_ = String();
    Serial.printf("[MOBILE-PROFILE] resumed count=%u url=%s\n",
                  static_cast<unsigned>(documentItemCount_),
                  documentPortalUrl().c_str());
    Serial.flush();
    return true;
}

void MapWebEditor::resetDocumentChunkTransfer(bool removePartial) {
    if (documentUploadFile_) {
        documentUploadFile_.close();
    }
    if (removePartial && !documentUploadPath_.isEmpty() && documentFsReady_ &&
        LittleFS.exists(documentUploadPath_)) {
        LittleFS.remove(documentUploadPath_);
    }
    documentChunkTransferActive_ = false;
    documentChunkRequestAccepted_ = false;
    documentChunkRequestDuplicate_ = false;
    documentChunkFinalizedThisRequest_ = false;
    documentChunkTransferId_ = String();
    documentChunkExpectedPart_ = 0U;
    documentChunkTotalParts_ = 0U;
    documentChunkExpectedTotalBytes_ = 0U;
    documentUploadPath_ = String();
    pendingDocumentName_ = String();
    pendingDocumentKind_ = DocumentFileKind::Unknown;
    pendingDocumentSize_ = 0U;
    pendingDocumentCrc32_ = 0xFFFFFFFFUL;
}

void MapWebEditor::stopDocumentPortal() {
    resetDocumentChunkTransfer(true);
    if (documentUploadFile_) {
        documentUploadFile_.close();
    }
    documentUploadPath_ = String();
    pendingDocumentName_ = String();
    pendingDocumentSize_ = 0U;
    documentPortalActive_ = false;
    documentPortalAccepted_ = false;
    if (network_) {
        network_->stopDocumentAp();
    }
    Serial.println("[MOBILE-PROFILE] stopped");
    Serial.flush();
}

String MapWebEditor::documentPortalUrl() const {
    if (!network_ || !network_->documentApActive()) {
        return String("-");
    }
    return String("http://") + network_->documentApIp() + "/doc";
}

bool MapWebEditor::documentItem(uint8_t index, DocumentItemInfo& output) const {
    if (index >= documentItemCount_) {
        return false;
    }
    output = documentItems_[index];
    return true;
}

size_t MapWebEditor::documentTotalBytes() const {
    size_t total = 0U;
    for (uint8_t index = 0; index < documentItemCount_; ++index) {
        total += documentItems_[index].size;
    }
    return total;
}

bool MapWebEditor::recoverDocumentItemsFromStorage() {
    if (documentItemCount_ > 0U) return true;
    if (!ensureDocumentStorage()) {
        lastDocumentError_ = "Profile storage unavailable";
        return false;
    }

    for (uint16_t fileId = 1U; fileId <= 128U; ++fileId) {
        char path[48] = {};
        snprintf(path, sizeof(path), "/mgdocs/profile_%03u.json", static_cast<unsigned>(fileId));
        if (!LittleFS.exists(path)) continue;

        File file = LittleFS.open(path, FILE_READ);
        if (!file) continue;
        const size_t size = file.size();
        if (size == 0U || size > kMaxDocumentUploadBytes) {
            file.close();
            continue;
        }
        uint8_t buffer[1024];
        uint32_t crc = 0xFFFFFFFFUL;
        size_t bytesRead = 0U;
        while (file.available()) {
            const size_t got = file.read(buffer, sizeof(buffer));
            if (got == 0U) break;
            crc = crc32Update(crc, buffer, got);
            bytesRead += got;
        }
        file.close();
        if (bytesRead != size) continue;

        documentItems_[0].path = path;
        documentItems_[0].name = String("Recovered-MG-Profile-") + String(static_cast<unsigned>(fileId)) + ".json";
        documentItems_[0].kind = DocumentFileKind::Json;
        documentItems_[0].size = size;
        documentItems_[0].crc32 = ~crc;
        documentItemCount_ = 1U;
        documentNextFileId_ = static_cast<uint16_t>(fileId + 1U);
        lastDocumentName_ = documentItems_[0].name;
        lastDocumentSize_ = size;
        lastDocumentError_ = String();
        const bool manifestOk = writeDocumentManifest();
        Serial.printf("[PROFILE-RECOVER] recovered=1 id=%u manifest=%s\n",
                      static_cast<unsigned>(fileId), manifestOk ? "OK" : "FAILED");
        Serial.flush();
        return true;
    }

    Serial.println("[PROFILE-RECOVER] no committed mobile profile found");
    Serial.flush();
    return false;
}

bool MapWebEditor::removeDocumentItem(uint8_t index) {
    if (index >= documentItemCount_ || !ensureDocumentStorage()) {
        return false;
    }
    const String path = documentItems_[index].path;
    if (!path.isEmpty() && LittleFS.exists(path)) {
        LittleFS.remove(path);
    }
    for (uint8_t cursor = index; cursor + 1U < documentItemCount_; ++cursor) {
        documentItems_[cursor] = documentItems_[cursor + 1U];
    }
    if (documentItemCount_ > 0U) {
        --documentItemCount_;
        documentItems_[documentItemCount_] = DocumentItemInfo{};
    }
    writeDocumentManifest();
    Serial.printf("[DOC-SESSION] removed index=%u count=%u\n",
                  static_cast<unsigned>(index),
                  static_cast<unsigned>(documentItemCount_));
    Serial.flush();
    return true;
}

bool MapWebEditor::moveDocumentItem(uint8_t index, int8_t delta) {
    if (index >= documentItemCount_ || delta == 0) {
        return false;
    }
    const int target = static_cast<int>(index) + static_cast<int>(delta);
    if (target < 0 || target >= static_cast<int>(documentItemCount_)) {
        return false;
    }
    const DocumentItemInfo temporary = documentItems_[index];
    documentItems_[index] = documentItems_[target];
    documentItems_[target] = temporary;
    writeDocumentManifest();
    Serial.printf("[DOC-SESSION] moved %u -> %d\n",
                  static_cast<unsigned>(index), target);
    Serial.flush();
    return true;
}

bool MapWebEditor::clearDocumentItems() {
    if (!ensureDocumentStorage()) {
        return false;
    }
    for (uint8_t index = 0; index < documentItemCount_; ++index) {
        const String path = documentItems_[index].path;
        if (!path.isEmpty() && LittleFS.exists(path)) {
            LittleFS.remove(path);
        }
        documentItems_[index] = DocumentItemInfo{};
    }
    documentItemCount_ = 0U;
    lastDocumentName_ = String();
    lastDocumentSize_ = 0U;
    lastDocumentError_ = String();
    writeDocumentManifest();
    Serial.println("[DOC-SESSION] all items cleared");
    Serial.flush();
    return true;
}

bool MapWebEditor::prepareExternalProfileStorage(String& tempPath, String& reason) {
    reason = String();
    tempPath = String();
    if (!ensureDocumentStorage()) {
        reason = "Profile storage unavailable";
        lastDocumentError_ = reason;
        return false;
    }
    if (documentItemCount_ >= kMaxDocumentItems) {
        reason = "A prepared profile already exists in this session";
        lastDocumentError_ = reason;
        return false;
    }

    tempPath = "/mgdocs/ble_profile.partial";
    if (LittleFS.exists(tempPath)) {
        LittleFS.remove(tempPath);
    }
    lastDocumentError_ = String();
    return true;
}

bool MapWebEditor::commitExternalPreparedProfile(const String& tempPath,
                                                 const String& displayName,
                                                 size_t size,
                                                 uint32_t crc32,
                                                 String& reason) {
    reason = String();
    if (!ensureDocumentStorage()) {
        reason = "Profile storage unavailable";
        lastDocumentError_ = reason;
        return false;
    }
    if (tempPath.isEmpty() || !LittleFS.exists(tempPath)) {
        reason = "BLE temporary profile is missing";
        lastDocumentError_ = reason;
        return false;
    }
    if (size == 0U || size > kMaxDocumentUploadBytes) {
        reason = "Profile exceeds 256 KB limit";
        lastDocumentError_ = reason;
        return false;
    }
    if (documentItemCount_ >= kMaxDocumentItems) {
        reason = "Only one prepared profile is accepted per session";
        lastDocumentError_ = reason;
        return false;
    }

    File source = LittleFS.open(tempPath, FILE_READ);
    if (!source) {
        reason = "BLE temporary profile cannot be opened";
        lastDocumentError_ = reason;
        return false;
    }
    const size_t actualSize = source.size();
    source.close();
    if (actualSize != size) {
        reason = "BLE committed profile size mismatch";
        lastDocumentError_ = reason;
        return false;
    }

    char destinationBuffer[48] = {};
    snprintf(destinationBuffer, sizeof(destinationBuffer), "/mgdocs/profile_%03u.json",
             static_cast<unsigned>(documentNextFileId_++));
    const String destination = destinationBuffer;
    if (LittleFS.exists(destination)) LittleFS.remove(destination);
    if (!LittleFS.rename(tempPath, destination)) {
        reason = "BLE profile commit rename failed";
        lastDocumentError_ = reason;
        return false;
    }

    DocumentItemInfo& item = documentItems_[documentItemCount_];
    item.name = displayName.isEmpty() ? String("mg_profile_ble.json") : safeDocumentName(displayName);
    item.path = destination;
    item.kind = DocumentFileKind::Json;
    item.size = size;
    item.crc32 = crc32;
    ++documentItemCount_;
    lastDocumentName_ = item.name;
    lastDocumentSize_ = size;
    lastDocumentError_ = String();

    if (!writeDocumentManifest()) {
        reason = lastDocumentError_.isEmpty() ? String("Profile manifest write failed") : lastDocumentError_;
        // Do not report a half-committed profile as ready. Roll back the item
        // and file so the phone can retry the complete transfer cleanly.
        if (LittleFS.exists(destination)) LittleFS.remove(destination);
        documentItems_[0] = DocumentItemInfo{};
        documentItemCount_ = 0U;
        lastDocumentName_ = String();
        lastDocumentSize_ = 0U;
        return false;
    }

    Serial.printf("[BLE-PROFILE] committed name=%s bytes=%lu crc=%08lX path=%s\n",
                  item.name.c_str(),
                  static_cast<unsigned long>(item.size),
                  static_cast<unsigned long>(item.crc32),
                  item.path.c_str());
    Serial.flush();
    return true;
}

void MapWebEditor::abortExternalPreparedProfile(const String& tempPath) {
    if (tempPath.isEmpty()) return;
    if (ensureDocumentStorage() && LittleFS.exists(tempPath)) {
        LittleFS.remove(tempPath);
    }
}

void MapWebEditor::noteDocumentTransportError(const String& error) {
    lastDocumentError_ = error;
}

bool MapWebEditor::finalizeDocumentManifest() {
    if (documentItemCount_ == 0U) {
        lastDocumentError_ = "No profile to finalize";
        return false;
    }
    return writeDocumentManifest();
}

String MapWebEditor::documentManifestPath() const {
    return documentFsReady_ ? String(kDocumentManifestPath) : String();
}

bool MapWebEditor::documentManifestExists() const {
    return documentFsReady_ && LittleFS.exists(kDocumentManifestPath);
}

void MapWebEditor::redirectToMap() {
    server_.sendHeader("Location", "/map", true);
    server_.send(302, "text/plain", "");
}

void MapWebEditor::redirectToDocumentPortal() {
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server_.sendHeader("Pragma", "no-cache");
    server_.sendHeader("Connection", "close");
    String target = documentPortalUrl();
    if (target == "-") target = String("/doc");
    server_.sendHeader("Location", target, true);
    server_.send(302, "text/plain; charset=utf-8", "");
}

bool MapWebEditor::validDocumentSession() const {
    return documentPortalActive_ && network_ != nullptr &&
           network_->documentApActive() &&
           server_.hasArg("token") &&
           server_.arg("token") == network_->documentSessionToken();
}

String MapWebEditor::safeExtension(const String& filename) {
    String lower = filename;
    lower.toLowerCase();
    if (lower.endsWith(".json")) return String(".json");
    return String();
}

String MapWebEditor::safeDocumentName(const String& filename) {
    String output;
    output.reserve(64);
    for (size_t i = 0; i < filename.length() && output.length() < 64U; ++i) {
        const char c = filename[i];
        const bool safe = (c >= 'a' && c <= 'z') ||
                          (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') ||
                          c == '.' || c == '-' || c == '_' || c == ' ';
        output += safe ? c : '_';
    }
    output.trim();
    return output.isEmpty() ? String("mg_profile.json") : output;
}

DocumentFileKind MapWebEditor::documentKindFromExtension(const String& extension) {
    if (extension == ".json") return DocumentFileKind::Json;
    return DocumentFileKind::Unknown;
}

const char* MapWebEditor::documentKindText(DocumentFileKind kind) {
    switch (kind) {
        case DocumentFileKind::Json: return "MG-JSON";
        default: return "UNKNOWN";
    }
}

uint32_t MapWebEditor::crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
    if (data == nullptr) return crc;
    for (size_t index = 0; index < length; ++index) {
        crc ^= static_cast<uint32_t>(data[index]);
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return crc;
}

bool MapWebEditor::writeDocumentManifest() {
    if (!ensureDocumentStorage()) {
        return false;
    }
    File manifest = LittleFS.open(kDocumentManifestPath, FILE_WRITE);
    if (!manifest) {
        lastDocumentError_ = "Could not write document manifest";
        return false;
    }
    manifest.print("{\n  \"version\": 2,\n  \"count\": ");
    manifest.print(documentItemCount_);
    manifest.print(",\n  \"total_bytes\": ");
    manifest.print(static_cast<unsigned long>(documentTotalBytes()));
    manifest.print(",\n  \"files\": [\n");
    for (uint8_t index = 0; index < documentItemCount_; ++index) {
        const DocumentItemInfo& item = documentItems_[index];
        char crcText[16] = {};
        snprintf(crcText, sizeof(crcText), "%08lX", static_cast<unsigned long>(item.crc32));
        manifest.print("    {\"order\": ");
        manifest.print(static_cast<unsigned>(index + 1U));
        manifest.print(", \"name\": \"");
        manifest.print(item.name);
        manifest.print("\", \"path\": \"");
        manifest.print(item.path);
        manifest.print("\", \"kind\": \"");
        manifest.print(documentKindText(item.kind));
        manifest.print("\", \"size\": ");
        manifest.print(static_cast<unsigned long>(item.size));
        manifest.print(", \"crc32\": \"");
        manifest.print(crcText);
        manifest.print("\"}");
        if (index + 1U < documentItemCount_) manifest.print(',');
        manifest.print('\n');
    }
    manifest.print("  ]\n}\n");
    manifest.close();
    return true;
}

void MapWebEditor::handleCaptiveProbe() {
    if (!documentPortalActive_) {
        server_.send(404, "text/plain", "Not active");
        return;
    }
    if (network_) network_->touchDocumentApSession();
    // Keep the OS captive-portal mini browser open during mobile profile transfer.
    // We deliberately do not return Android's normal 204 success response here:
    // doing so makes Android close CaptivePortalLogin before the operator uploads.
    redirectToDocumentPortal();
}

void MapWebEditor::handleProfileApiSession() {
    if (!documentPortalActive_ || network_ == nullptr || !network_->documentApActive()) {
        server_.send(503, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"profile session inactive\"}");
        return;
    }
    network_->touchDocumentApSession();
    documentPortalAccepted_ = true;
    char response[384] = {};
    snprintf(response, sizeof(response),
             "{\"ok\":true,\"device\":\"MG-SMART-TESTER\",\"api\":1,\"ssid\":\"%s\",\"ip\":\"%s\",\"token\":\"%s\",\"format\":\"MG_PROFILE_JSON_V1\",\"maxBytes\":%lu}",
             network_->documentApSsid().c_str(),
             network_->documentApIp().c_str(),
             network_->documentSessionToken().c_str(),
             static_cast<unsigned long>(kMaxDocumentUploadBytes));
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json; charset=utf-8", response);
}

void MapWebEditor::handleDocumentPortal() {
    if (!documentPortalActive_ || network_ == nullptr) {
        server_.send(503, "text/plain; charset=utf-8", "Mobile profile portal inactive");
        return;
    }
    network_->touchDocumentApSession();
    documentPortalAccepted_ = true;
    const String token = network_->documentSessionToken();

    String html;
    html.reserve(5600);
    html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
    html += F("<title>MG Smart Tester - Mobile Profile</title><style>body{font-family:Arial;background:#081119;color:#eaf6ff;margin:0}header{background:#142b3a;padding:18px;text-align:center}main{max-width:760px;margin:auto;padding:18px}.card{background:#102431;border:1px solid #355565;border-radius:12px;padding:16px;margin:14px 0}.btn{display:block;width:100%;box-sizing:border-box;padding:14px;border:0;border-radius:9px;background:#176b8f;color:white;font-weight:bold;margin-top:12px}.green{background:#267047}.muted{color:#9cc6dc}.warn{color:#f1c96a}.good{color:#80e6a8}input{width:100%;padding:12px;box-sizing:border-box;background:#09161e;color:white;border:1px solid #4c7a91;border-radius:8px}</style></head><body>");
    html += F("<header><h2>MG SMART TESTER</h2><div>Mobil Profil Aktar / Mobile Profile Import</div></header><main>");
    html += F("<div class='card'><b>Belgeyi telefon uygulamasında hazırlayın.</b><br><span class='muted'>The Android/iOS companion app sends a prepared MG electrical profile JSON to the tester.</span></div>");
    html += F("<div class='card'><h3>Hazır Profil JSON / Prepared Profile JSON</h3><form method='post' enctype='multipart/form-data' action='/doc/upload?token=");
    html += token;
    html += F("'><input type='file' name='file' accept='application/json,.json' required><div class='muted'>Geliştirme/servis için tarayıcıdan manuel yükleme. Normal kullanımda companion app aynı profili doğrudan gönderir.</div><button class='btn green' type='submit'>PROFİLİ GÖNDER / SEND PROFILE</button></form></div>");
    html += F("<div class='card'><span class='good'>Alınan profil / Received profile: ");
    html += String(documentItemCount_);
    html += F("</span><br><span class='muted'>Toplam / Total: ");
    html += String(static_cast<unsigned long>(documentTotalBytes()));
    html += F(" bytes</span><br>");
    if (!lastDocumentError_.isEmpty()) {
        html += F("<span class='warn'>"); html += lastDocumentError_; html += F("</span><br>");
    }
    for (uint8_t index = 0; index < documentItemCount_; ++index) {
        const DocumentItemInfo& item = documentItems_[index];
        char crcText[16] = {};
        snprintf(crcText, sizeof(crcText), "%08lX", static_cast<unsigned long>(item.crc32));
        html += F("<div><b>"); html += item.name; html += F("</b><br><span class='muted'>MG-JSON · ");
        html += String(static_cast<unsigned long>(item.size)); html += F(" bytes · CRC32 "); html += crcText;
        html += F("</span><form method='post' action='/doc/delete?token="); html += token; html += F("&index=0'><button class='btn' type='submit'>SİL / DELETE</button></form></div>");
    }
    html += F("</div></main></body></html>");
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "text/html; charset=utf-8", html);
}

void MapWebEditor::handleDocumentDelete() {
    if (!validDocumentSession() || !server_.hasArg("index")) {
        server_.send(400, "text/plain; charset=utf-8", "Invalid profile delete request");
        return;
    }
    const int index = server_.arg("index").toInt();
    if (index < 0 || index >= static_cast<int>(documentItemCount_) ||
        !removeDocumentItem(static_cast<uint8_t>(index))) {
        server_.send(400, "text/plain; charset=utf-8", "Profile item could not be removed");
        return;
    }
    if (network_) network_->touchDocumentApSession();
    redirectToDocumentPortal();
}


void MapWebEditor::handleDocumentChunkUploadChunk() {
    HTTPUpload& upload = server_.upload();

    if (upload.status == UPLOAD_FILE_START) {
        documentChunkRequestAccepted_ = false;
        documentChunkRequestDuplicate_ = false;
        documentChunkFinalizedThisRequest_ = false;
        lastDocumentError_ = String();

        if (!validDocumentSession()) {
            lastDocumentError_ = "Invalid or expired profile session";
            return;
        }
        if (!server_.hasArg("id") || !server_.hasArg("part") ||
            !server_.hasArg("parts") || !server_.hasArg("name") ||
            !server_.hasArg("size")) {
            lastDocumentError_ = "Incomplete profile chunk request";
            return;
        }
        if (!ensureDocumentStorage()) {
            lastDocumentError_ = "Profile storage unavailable";
            return;
        }

        const String transferId = server_.arg("id");
        const int partValue = server_.arg("part").toInt();
        const int partsValue = server_.arg("parts").toInt();
        const size_t declaredTotal = static_cast<size_t>(server_.arg("size").toInt());
        const String originalName = server_.arg("name");

        if (transferId.isEmpty() || partValue < 0 || partsValue <= 0 ||
            partValue >= partsValue || partsValue > 512) {
            lastDocumentError_ = "Invalid chunk sequence";
            return;
        }
        if (declaredTotal == 0U || declaredTotal > kMaxDocumentUploadBytes) {
            lastDocumentError_ = "Profile exceeds 256 KB limit";
            return;
        }

        const uint16_t part = static_cast<uint16_t>(partValue);
        const uint16_t parts = static_cast<uint16_t>(partsValue);

        // If the phone did not receive the ACK for the last request it may
        // retry once. Do not create a duplicate profile item in that case.
        if (!documentChunkTransferActive_ &&
            transferId == documentLastCompletedTransferId_ &&
            parts == documentLastCompletedParts_ && part + 1U == parts) {
            // A final retry is acknowledged only after the durable manifest can
            // also be written. This prevents the browser from showing "complete"
            // while P4 review has no committed profile metadata.
            if (!writeDocumentManifest()) {
                return;
            }
            documentChunkRequestDuplicate_ = true;
            documentChunkFinalizedThisRequest_ = true;
            if (network_) network_->touchDocumentApSession();
            return;
        }

        if (part == 0U) {
            // A new upload supersedes any interrupted partial transfer.
            if (documentChunkTransferActive_) {
                resetDocumentChunkTransfer(true);
            }
            if (documentItemCount_ >= kMaxDocumentItems) {
                lastDocumentError_ = "Only one prepared profile is accepted per session";
                return;
            }
            const String extension = safeExtension(originalName);
            if (extension.isEmpty()) {
                lastDocumentError_ = "Only prepared MG profile JSON is accepted";
                return;
            }

            const size_t fsTotal = LittleFS.totalBytes();
            const size_t fsUsed = LittleFS.usedBytes();
            constexpr size_t kStorageReserve = 64U * 1024U;
            const size_t fsFree = fsTotal > fsUsed ? fsTotal - fsUsed : 0U;
            if (fsTotal > 0U && declaredTotal + kStorageReserve > fsFree) {
                lastDocumentError_ = "Not enough LittleFS space for this document";
                Serial.printf("[PROFILE-UPLOAD] storage reject need=%lu free=%lu total=%lu used=%lu\n",
                              static_cast<unsigned long>(declaredTotal),
                              static_cast<unsigned long>(fsFree),
                              static_cast<unsigned long>(fsTotal),
                              static_cast<unsigned long>(fsUsed));
                Serial.flush();
                return;
            }

            pendingDocumentName_ = safeDocumentName(originalName);
            pendingDocumentKind_ = documentKindFromExtension(extension);
            pendingDocumentSize_ = 0U;
            pendingDocumentCrc32_ = 0xFFFFFFFFUL;
            documentChunkTransferId_ = transferId;
            documentChunkExpectedPart_ = 0U;
            documentChunkTotalParts_ = parts;
            documentChunkExpectedTotalBytes_ = declaredTotal;
            documentChunkTransferActive_ = true;

            char path[48] = {};
            snprintf(path, sizeof(path), "/mgdocs/profile_%03u%s",
                     static_cast<unsigned>(documentNextFileId_++),
                     extension.c_str());
            documentUploadPath_ = path;
            if (LittleFS.exists(documentUploadPath_)) {
                LittleFS.remove(documentUploadPath_);
            }
        } else {
            if (!documentChunkTransferActive_ ||
                transferId != documentChunkTransferId_ ||
                parts != documentChunkTotalParts_ ||
                declaredTotal != documentChunkExpectedTotalBytes_) {
                lastDocumentError_ = "Chunk upload session mismatch";
                return;
            }
            if (part < documentChunkExpectedPart_) {
                // ACK may have been lost; the already committed chunk is safe
                // to acknowledge again without appending it twice.
                documentChunkRequestDuplicate_ = true;
                if (network_) network_->touchDocumentApSession();
                return;
            }
            if (part != documentChunkExpectedPart_) {
                lastDocumentError_ = "Chunk arrived out of order";
                return;
            }
        }

        if (!documentChunkTransferActive_ || documentUploadPath_.isEmpty()) {
            lastDocumentError_ = "Chunk upload state unavailable";
            return;
        }

        const char* mode = (part == 0U) ? FILE_WRITE : FILE_APPEND;
        documentUploadFile_ = LittleFS.open(documentUploadPath_, mode);
        if (!documentUploadFile_) {
            lastDocumentError_ = "Could not open document chunk file";
            resetDocumentChunkTransfer(true);
            return;
        }
        documentChunkRequestAccepted_ = true;
        if (network_) network_->touchDocumentApSession();
        Serial.printf("[PROFILE-UPLOAD] chunk start id=%s part=%u/%u name=%s heap=%lu\n",
                      transferId.c_str(),
                      static_cast<unsigned>(part + 1U),
                      static_cast<unsigned>(parts),
                      pendingDocumentName_.c_str(),
                      static_cast<unsigned long>(ESP.getFreeHeap()));
        Serial.flush();
        return;
    }

    if (upload.status == UPLOAD_FILE_WRITE) {
        if (documentChunkRequestDuplicate_ || !documentChunkRequestAccepted_ ||
            !documentUploadFile_ || !lastDocumentError_.isEmpty()) {
            return;
        }
        if (pendingDocumentSize_ + upload.currentSize > kMaxDocumentUploadBytes ||
            pendingDocumentSize_ + upload.currentSize > documentChunkExpectedTotalBytes_) {
            lastDocumentError_ = "Chunk data exceeds declared file size";
            documentUploadFile_.close();
            resetDocumentChunkTransfer(true);
            return;
        }
        const size_t written = documentUploadFile_.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            lastDocumentError_ = "Flash write failed";
            documentUploadFile_.close();
            resetDocumentChunkTransfer(true);
            return;
        }
        pendingDocumentCrc32_ = crc32Update(pendingDocumentCrc32_, upload.buf, written);
        pendingDocumentSize_ += written;
        if (network_) network_->touchDocumentApSession();
        yield();
        return;
    }

    if (upload.status == UPLOAD_FILE_END) {
        if (documentChunkRequestDuplicate_) {
            return;
        }
        if (documentUploadFile_) {
            documentUploadFile_.flush();
            documentUploadFile_.close();
        }
        if (!documentChunkRequestAccepted_) {
            return;
        }
        if (!lastDocumentError_.isEmpty()) {
            resetDocumentChunkTransfer(true);
            return;
        }

        const uint16_t completedPart = documentChunkExpectedPart_;
        if (completedPart + 1U < documentChunkTotalParts_) {
            documentChunkExpectedPart_ = static_cast<uint16_t>(completedPart + 1U);
            Serial.printf("[PROFILE-UPLOAD] chunk committed next=%u/%u bytes=%lu\n",
                          static_cast<unsigned>(documentChunkExpectedPart_ + 1U),
                          static_cast<unsigned>(documentChunkTotalParts_),
                          static_cast<unsigned long>(pendingDocumentSize_));
            Serial.flush();
            return;
        }

        if (pendingDocumentSize_ != documentChunkExpectedTotalBytes_) {
            lastDocumentError_ = "Final profile size does not match phone upload";
            resetDocumentChunkTransfer(true);
            return;
        }

        DocumentItemInfo& item = documentItems_[documentItemCount_];
        item.name = pendingDocumentName_.isEmpty() ? String("mg_profile.json") : pendingDocumentName_;
        item.path = documentUploadPath_;
        item.kind = pendingDocumentKind_;
        item.size = pendingDocumentSize_;
        item.crc32 = ~pendingDocumentCrc32_;
        ++documentItemCount_;
        lastDocumentName_ = item.name;
        lastDocumentSize_ = item.size;
        documentLastCompletedTransferId_ = documentChunkTransferId_;
        documentLastCompletedParts_ = documentChunkTotalParts_;
        documentChunkFinalizedThisRequest_ = true;

        if (!writeDocumentManifest()) {
            // Keep the committed file/item and last-completed transfer ID so the
            // phone may retry the final chunk ACK.  handleDocumentChunkUploadComplete()
            // will return HTTP 400 while lastDocumentError_ is set; therefore the
            // browser can never report success before the manifest is durable.
            Serial.println("[PROFILE-UPLOAD] final file committed but manifest write failed; waiting for final ACK retry");
        }
        Serial.printf("[PROFILE-UPLOAD] chunked complete count=%u name=%s bytes=%lu crc=%08lX heap=%lu\n",
                      static_cast<unsigned>(documentItemCount_),
                      lastDocumentName_.c_str(),
                      static_cast<unsigned long>(lastDocumentSize_),
                      static_cast<unsigned long>(item.crc32),
                      static_cast<unsigned long>(ESP.getFreeHeap()));
        Serial.flush();

        // Keep last-completed ID for a possible final-ACK retry, but release
        // all active transfer state without deleting the completed file.
        documentChunkTransferActive_ = false;
        documentChunkRequestAccepted_ = false;
        documentChunkRequestDuplicate_ = false;
        documentChunkTransferId_ = String();
        documentChunkExpectedPart_ = 0U;
        documentChunkTotalParts_ = 0U;
        documentChunkExpectedTotalBytes_ = 0U;
        documentUploadPath_ = String();
        pendingDocumentName_ = String();
        pendingDocumentKind_ = DocumentFileKind::Unknown;
        pendingDocumentSize_ = 0U;
        pendingDocumentCrc32_ = 0xFFFFFFFFUL;
        if (network_) network_->touchDocumentApSession();
        return;
    }

    if (upload.status == UPLOAD_FILE_ABORTED) {
        if (documentUploadFile_) documentUploadFile_.close();
        lastDocumentError_ = "Chunk upload aborted";
        resetDocumentChunkTransfer(true);
        Serial.println("[PROFILE-UPLOAD] chunk aborted");
        Serial.flush();
    }
}

void MapWebEditor::handleDocumentChunkUploadComplete() {
    if (!documentPortalActive_) {
        server_.send(503, "text/plain", "Mobile profile portal inactive");
        return;
    }
    if (!lastDocumentError_.isEmpty()) {
        server_.send(400, "text/plain; charset=utf-8", lastDocumentError_);
        return;
    }
    if (network_) network_->touchDocumentApSession();

    char response[192] = {};
    snprintf(response, sizeof(response),
             "{\"ok\":true,\"duplicate\":%s,\"done\":%s,\"files\":%u,\"bytes\":%lu}",
             documentChunkRequestDuplicate_ ? "true" : "false",
             documentChunkFinalizedThisRequest_ ? "true" : "false",
             static_cast<unsigned>(documentItemCount_),
             static_cast<unsigned long>(documentTotalBytes()));
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json; charset=utf-8", response);
}

void MapWebEditor::handleDocumentUploadChunk() {
    HTTPUpload& upload = server_.upload();

    if (upload.status == UPLOAD_FILE_START) {
        lastDocumentError_ = String();
        pendingDocumentSize_ = 0U;
        pendingDocumentCrc32_ = 0xFFFFFFFFUL;
        pendingDocumentName_ = safeDocumentName(upload.filename);
        pendingDocumentKind_ = DocumentFileKind::Unknown;
        documentUploadPath_ = String();

        if (!validDocumentSession()) {
            lastDocumentError_ = "Invalid or expired profile session";
            return;
        }
        if (!ensureDocumentStorage()) {
            lastDocumentError_ = "Profile storage unavailable";
            return;
        }
        const String extension = safeExtension(upload.filename);
        if (extension.isEmpty()) {
            lastDocumentError_ = "Only prepared MG profile JSON is accepted";
            return;
        }
        if (documentItemCount_ >= kMaxDocumentItems) {
            lastDocumentError_ = "Only one prepared profile is accepted per session";
            return;
        }
        pendingDocumentKind_ = documentKindFromExtension(extension);

        char path[48] = {};
        snprintf(path, sizeof(path), "/mgdocs/profile_%03u%s",
                 static_cast<unsigned>(documentNextFileId_++),
                 extension.c_str());
        documentUploadPath_ = path;
        if (LittleFS.exists(documentUploadPath_)) {
            LittleFS.remove(documentUploadPath_);
        }
        documentUploadFile_ = LittleFS.open(documentUploadPath_, FILE_WRITE);
        if (!documentUploadFile_) {
            lastDocumentError_ = "Could not create upload file";
            documentUploadPath_ = String();
            return;
        }
        if (network_) network_->touchDocumentApSession();
        Serial.printf("[PROFILE-UPLOAD] start name=%s path=%s\n",
                      pendingDocumentName_.c_str(), documentUploadPath_.c_str());
        Serial.flush();
        return;
    }

    if (upload.status == UPLOAD_FILE_WRITE) {
        if (!documentUploadFile_ || !lastDocumentError_.isEmpty()) {
            return;
        }
        if (pendingDocumentSize_ + upload.currentSize > kMaxDocumentUploadBytes) {
            lastDocumentError_ = "Profile exceeds 256 KB limit";
            documentUploadFile_.close();
            if (!documentUploadPath_.isEmpty()) LittleFS.remove(documentUploadPath_);
            documentUploadPath_ = String();
            return;
        }
        const size_t written = documentUploadFile_.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            lastDocumentError_ = "Flash write failed";
            documentUploadFile_.close();
            if (!documentUploadPath_.isEmpty()) LittleFS.remove(documentUploadPath_);
            documentUploadPath_ = String();
            return;
        }
        pendingDocumentCrc32_ = crc32Update(pendingDocumentCrc32_, upload.buf, written);
        pendingDocumentSize_ += written;
        if (network_) network_->touchDocumentApSession();
        return;
    }

    if (upload.status == UPLOAD_FILE_END) {
        if (documentUploadFile_) documentUploadFile_.close();
        if (!lastDocumentError_.isEmpty() || documentUploadPath_.isEmpty()) {
            if (!documentUploadPath_.isEmpty()) LittleFS.remove(documentUploadPath_);
            documentUploadPath_ = String();
            return;
        }
        DocumentItemInfo& item = documentItems_[documentItemCount_];
        item.name = pendingDocumentName_.isEmpty() ? String("mg_profile.json") : pendingDocumentName_;
        item.path = documentUploadPath_;
        item.kind = pendingDocumentKind_;
        item.size = pendingDocumentSize_;
        item.crc32 = ~pendingDocumentCrc32_;
        ++documentItemCount_;
        lastDocumentName_ = item.name;
        lastDocumentSize_ = item.size;
        if (!writeDocumentManifest()) {
            Serial.println("[PROFILE-UPLOAD] warning: manifest write failed");
        }
        Serial.printf("[PROFILE-UPLOAD] complete count=%u name=%s bytes=%lu crc=%08lX\n",
                      static_cast<unsigned>(documentItemCount_),
                      lastDocumentName_.c_str(),
                      static_cast<unsigned long>(lastDocumentSize_),
                      static_cast<unsigned long>(item.crc32));
        Serial.flush();
        documentUploadPath_ = String();
        if (network_) network_->touchDocumentApSession();
        return;
    }

    if (upload.status == UPLOAD_FILE_ABORTED) {
        if (documentUploadFile_) documentUploadFile_.close();
        if (!documentUploadPath_.isEmpty()) LittleFS.remove(documentUploadPath_);
        documentUploadPath_ = String();
        lastDocumentError_ = "Upload aborted";
        Serial.println("[PROFILE-UPLOAD] aborted");
        Serial.flush();
    }
}

void MapWebEditor::handleDocumentUploadComplete() {
    if (!documentPortalActive_) {
        server_.send(503, "text/plain", "Mobile profile portal inactive");
        return;
    }
    if (!lastDocumentError_.isEmpty()) {
        server_.send(400, "text/plain; charset=utf-8", lastDocumentError_);
        return;
    }
    redirectToDocumentPortal();
}

int MapWebEditor::parsePinToken(String token) {
    token.trim();
    token.toUpperCase();
    if (token == "PE") {
        return kPeTestIndex;
    }
    if (token == "DS" || token == "SH") {
        return kDrainShieldTestIndex;
    }
    if (token.length() > 0 && (token[0] == 'A' || token[0] == 'B')) {
        token.remove(0, 1);
        token.trim();
    }
    for (size_t i = 0; i < token.length(); ++i) {
        if (!isdigit(static_cast<unsigned char>(token[i]))) {
            return -1;
        }
    }
    const int value = token.toInt();
    return value >= 0 && value < kTestPointsPerSide ? value : -1;
}

uint64_t MapWebEditor::parseMask(const String& value) {
    uint64_t mask = 0ULL;
    int start = 0;
    while (start <= value.length()) {
        const int comma = value.indexOf(',', start);
        String token = comma < 0 ? value.substring(start) : value.substring(start, comma);
        token.trim();
        if (token.length() > 0 && token != "-") {
            const int pin = parsePinToken(token);
            if (pin >= 0) {
                mask |= (1ULL << pin);
            }
        }
        if (comma < 0) {
            break;
        }
        start = comma + 1;
    }
    return mask;
}

String MapWebEditor::formatPin(uint8_t index) {
    if (index == kPeTestIndex) {
        return String("PE");
    }
    if (index == kDrainShieldTestIndex) {
        return String("dS");
    }
    return String(index);
}

String MapWebEditor::formatMask(uint64_t mask, char prefix) {
    String output;
    for (uint8_t i = 0; i < kTestPointsPerSide; ++i) {
        if ((mask & (1ULL << i)) == 0ULL) {
            continue;
        }
        if (!output.isEmpty()) {
            output += ',';
        }
        if (prefix != '\0') {
            output += prefix;
        }
        output += formatPin(i);
    }
    return output.isEmpty() ? String("-") : output;
}

void MapWebEditor::handleRoot() {
    if (documentPortalActive_) {
        redirectToDocumentPortal();
        return;
    }
    if (map_ == nullptr) {
        server_.send(500, "text/plain", "Map not initialized");
        return;
    }

    String html;
    html.reserve(30000);
    html += F("<!doctype html><html><head><meta charset='utf-8'>");
    html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
    html += F("<title>MG Smart Test - Custom Map</title><style>");
    html += F("body{font-family:Arial,sans-serif;background:#071018;color:#eef7fb;margin:0}header{background:#142b3a;padding:18px;text-align:center}main{max-width:1100px;margin:auto;padding:18px}.card{background:#10202a;border:1px solid #355565;border-radius:10px;padding:14px;margin-bottom:16px}table{width:100%;border-collapse:collapse}th,td{border-bottom:1px solid #29414f;padding:7px}th{background:#183342;position:sticky;top:0}input{width:96%;padding:8px;background:#0b1820;color:white;border:1px solid #4b6878;border-radius:6px}button{padding:11px 18px;border:0;border-radius:7px;color:white;font-weight:bold;margin:5px}.save{background:#267047}.one{background:#176b8f}.clear{background:#7a3d3d}.muted{color:#9fc0d0}.rev{color:#8eeaa5;font-family:monospace}</style></head><body>");
    html += F("<header><h2>MG SMART TEST - ÖZEL HARİTA / CUSTOM MAP</h2></header><main>");
    html += F("<div class='card'><b>MG Smart Tester dokunmatik editörü ile aynı aktif haritayı düzenler.</b><br><span class='muted'>A->B hedeflerini virgülle yazın: 3,5,12 veya PE,dS. B->A sütunu otomatik türetilir. A-A / B-B ortak bağlantılar şu sürümde MG Smart Tester dokunmatik editöründen düzenlenir.</span>");
    if (!editingEnabled_) html += F("<br><b style='color:#ffd06b'>Tarama veya dokunmatik editör açıkken web düzenleme kilitlidir.</b>");
    html += F("</div>");
    html += F("<form method='post' action='/map/save'><div class='card'><table><tr><th>A Kaynak</th><th>Beklenen B Hedefleri</th><th>Otomatik B->A Önizleme</th></tr>");

    for (uint8_t i = 0; i < kTestPointsPerSide; ++i) {
        html += F("<tr><td><b>A");
        html += formatPin(i);
        html += F("</b></td><td><input name='ab");
        html += String(i);
        html += F("' value='");
        html += formatMask(map_->receiverMaskAtoB(i), '\0');
        html += F("'></td><td class='rev'>B");
        html += formatPin(i);
        html += F(" -> ");
        html += formatMask(map_->receiverMaskBtoA(i), 'A');
        html += F("</td></tr>");
    }
    html += F("</table><button class='save' type='submit'>KAYDET / SAVE</button></div></form>");
    html += F("<div class='card'><a href='/map/graph?pins=25&pe=1&ds=1'><button class='one' type='button'>GRAFİKLE GÖSTER / SHOW GRAPH</button></a><form style='display:inline' method='post' action='/map/one'><button class='one' type='submit'>1:1 DOLDUR / FILL 1:1</button></form><form style='display:inline' method='post' action='/map/clear'><button class='clear' type='submit'>TEMİZLE / CLEAR</button></form></div>");
    html += F("</main></body></html>");
    server_.send(200, "text/html; charset=utf-8", html);
}

void MapWebEditor::handleGraph() {
    if (documentPortalActive_) {
        redirectToDocumentPortal();
        return;
    }
    if (map_ == nullptr) {
        server_.send(500, "text/plain", "Map not initialized");
        return;
    }

    int pins = server_.hasArg("pins") ? server_.arg("pins").toInt() : 25;
    if (pins < 1) pins = 1;
    if (pins > kMaximumSignalPins) pins = kMaximumSignalPins;
    const bool includePe = !server_.hasArg("pe") || server_.arg("pe") == "1";
    const bool includeDs = !server_.hasArg("ds") || server_.arg("ds") == "1";

    uint8_t visible[kTestPointsPerSide]{};
    uint8_t visibleCount = 0U;
    if (includePe) visible[visibleCount++] = kPeTestIndex;
    for (int pin = 1; pin <= pins && visibleCount < kTestPointsPerSide; ++pin) {
        visible[visibleCount++] = static_cast<uint8_t>(pin);
    }
    if (includeDs && visibleCount < kTestPointsPerSide) {
        visible[visibleCount++] = kDrainShieldTestIndex;
    }

    uint64_t visibleMask = 0ULL;
    for (uint8_t i = 0; i < visibleCount; ++i) {
        visibleMask |= (1ULL << visible[i]);
    }

    String html;
    html.reserve(26000);
    html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
    html += F("<title>MG Smart Test - Map Graph</title><style>body{font-family:Arial,sans-serif;background:#071018;color:#eef7fb;margin:0}header{background:#142b3a;padding:14px;text-align:center}main{max-width:1150px;margin:auto;padding:14px}.card{background:#10202a;border:1px solid #355565;border-radius:10px;padding:12px;margin-bottom:14px}a,button{color:white}button{padding:9px 14px;border:0;border-radius:7px;background:#355f78;font-weight:bold;margin:4px}input,select{padding:7px;background:#0b1820;color:white;border:1px solid #4b6878;border-radius:5px}input{width:70px}.legend{font-size:13px}.w{color:#f0f4f6}.g{color:#59d98a}.y{color:#ffd05a}svg{width:100%;height:auto;background:#071018;border-radius:8px}</style></head><body>");
    html += F("<header><h2>KABLO BAĞLANTI HARİTASI / CABLE CONNECTION MAP</h2></header><main>");
    html += F("<div class='card'><form method='get' action='/map/graph'>Maksimum pin / Max pins: <input type='number' min='1' max='62' name='pins' value='");
    html += String(pins);
    html += F("'> PE: <select name='pe'><option value='1'");
    if (includePe) html += F(" selected");
    html += F(">ON</option><option value='0'");
    if (!includePe) html += F(" selected");
    html += F(">OFF</option></select> dS: <select name='ds'><option value='1'");
    if (includeDs) html += F(" selected");
    html += F(">ON</option><option value='0'");
    if (!includeDs) html += F(" selected");
    html += F(">OFF</option></select><button type='submit'>GÜNCELLE / UPDATE</button><a href='/map'><button type='button'>EDİTÖRE DÖN / BACK</button></a></form>");
    html += F("<div class='legend'><span class='w'>Beyaz: 1:1</span> &nbsp; <span class='g'>Yeşil: özel bağlantı</span> &nbsp; <span class='y'>Sarı: ortak / çoklu net</span></div></div>");

    const int svgH = 760;
    const int top = 34;
    const int bottom = 730;
    auto yFor = [&](uint8_t slot) -> int {
        if (visibleCount <= 1U) return (top + bottom) / 2;
        return top + (static_cast<int>(slot) * (bottom - top)) / (visibleCount - 1U);
    };
    auto findSlot = [&](uint8_t testIndex) -> int {
        for (uint8_t i = 0; i < visibleCount; ++i) if (visible[i] == testIndex) return i;
        return -1;
    };

    html += F("<div class='card'><svg viewBox='0 0 1000 760' role='img' aria-label='Cable map graph'><text x='55' y='20' fill='#6edbff' font-size='16'>A</text><text x='945' y='20' fill='#8ef0a7' font-size='16'>B</text>");
    // Guides and pin labels.
    for (uint8_t slot = 0; slot < visibleCount; ++slot) {
        const int y = yFor(slot);
        html += F("<line x1='80' y1='"); html += String(y); html += F("' x2='920' y2='"); html += String(y); html += F("' stroke='#16303d' stroke-width='1'/>");
        html += F("<text x='68' y='"); html += String(y + 4); html += F("' fill='#dcecf4' font-size='11' text-anchor='end'>"); html += formatPin(visible[slot]); html += F("</text>");
        html += F("<text x='932' y='"); html += String(y + 4); html += F("' fill='#dcecf4' font-size='11'>"); html += formatPin(visible[slot]); html += F("</text>");
    }

    // Draw one trunk per ELECTRICAL NET, not one full-width line per
    // A->B pair. Multi-pin joins are fanned out beside the connector, matching
    // the P4 touch graph and conventional hand-drawn wiring diagrams.
    CableNet nets[kTestPointsPerSide]{};
    const size_t netCount = CableNetGraph::build(*map_, visibleMask,
                                                  nets, kTestPointsPerSide);
    const int branchInset = visibleCount <= 25U ? 78 : (visibleCount <= 40U ? 62 : 48);
    const int leftNodeX = 80;
    const int rightNodeX = 920;
    const int leftJoinX = leftNodeX + branchInset;
    const int rightJoinX = rightNodeX - branchInset;

    auto appendLine = [&](int x1, int y1, int x2, int y2,
                          const char* color, int lineWidth) {
        html += F("<line x1='"); html += String(x1);
        html += F("' y1='"); html += String(y1);
        html += F("' x2='"); html += String(x2);
        html += F("' y2='"); html += String(y2);
        html += F("' stroke='"); html += color;
        html += F("' stroke-width='"); html += String(lineWidth);
        html += F("' stroke-linecap='round'/>");
    };
    auto appendJunction = [&](int x, int y, const char* color) {
        html += F("<circle cx='"); html += String(x);
        html += F("' cy='"); html += String(y);
        html += F("' r='"); html += visibleCount <= 25U ? "2.4" : "1.7";
        html += F("' fill='"); html += color; html += F("'/>");
    };

    for (size_t netIndex = 0; netIndex < netCount; ++netIndex) {
        const CableNet& net = nets[netIndex];
        const uint8_t aCount = CableNetGraph::popcount(net.aMask);
        const uint8_t bCount = CableNetGraph::popcount(net.bMask);
        uint8_t aSlots[kTestPointsPerSide]{};
        uint8_t bSlots[kTestPointsPerSide]{};
        uint8_t aSlotCount = 0U;
        uint8_t bSlotCount = 0U;
        uint8_t onlyAIndex = 0xFFU;
        uint8_t onlyBIndex = 0xFFU;

        for (uint8_t index = 0; index < kTestPointsPerSide; ++index) {
            if ((net.aMask & (1ULL << index)) != 0ULL) {
                const int slot = findSlot(index);
                if (slot >= 0) {
                    aSlots[aSlotCount++] = static_cast<uint8_t>(slot);
                    onlyAIndex = index;
                }
            }
            if ((net.bMask & (1ULL << index)) != 0ULL) {
                const int slot = findSlot(index);
                if (slot >= 0) {
                    bSlots[bSlotCount++] = static_cast<uint8_t>(slot);
                    onlyBIndex = index;
                }
            }
        }

        const bool multiNet = aCount > 1U || bCount > 1U;
        const bool oneToOne = aCount == 1U && bCount == 1U && onlyAIndex == onlyBIndex;
        const char* color = multiNet ? "#ffd05a" : (oneToOne ? "#f0f4f6" : "#59d98a");
        const int lineWidth = visibleCount <= 25U ? 2 : 1;

        bool haveA = aSlotCount > 0U;
        bool haveB = bSlotCount > 0U;
        int rootAY = haveA ? yFor(aSlots[0]) : 0;
        int rootBY = haveB ? yFor(bSlots[0]) : 0;
        if (haveA && haveB) {
            int bestDistance = 0x7FFFFFFF;
            for (uint8_t ai = 0; ai < aSlotCount; ++ai) {
                const int ay = yFor(aSlots[ai]);
                for (uint8_t bi = 0; bi < bSlotCount; ++bi) {
                    const int by = yFor(bSlots[bi]);
                    const int distance = ay > by ? ay - by : by - ay;
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        rootAY = ay;
                        rootBY = by;
                    }
                }
            }
        }

        if (aSlotCount > 1U) {
            int minY = yFor(aSlots[0]);
            int maxY = minY;
            for (uint8_t i = 0; i < aSlotCount; ++i) {
                const int y = yFor(aSlots[i]);
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                appendLine(leftNodeX, y, leftJoinX, y, color, lineWidth);
                appendJunction(leftJoinX, y, color);
            }
            appendLine(leftJoinX, minY, leftJoinX, maxY, color, lineWidth);
        }

        if (bSlotCount > 1U) {
            int minY = yFor(bSlots[0]);
            int maxY = minY;
            for (uint8_t i = 0; i < bSlotCount; ++i) {
                const int y = yFor(bSlots[i]);
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                appendLine(rightJoinX, y, rightNodeX, y, color, lineWidth);
                appendJunction(rightJoinX, y, color);
            }
            appendLine(rightJoinX, minY, rightJoinX, maxY, color, lineWidth);
        }

        if (haveA && haveB) {
            appendLine(aSlotCount > 1U ? leftJoinX : leftNodeX,
                       rootAY,
                       bSlotCount > 1U ? rightJoinX : rightNodeX,
                       rootBY,
                       color,
                       lineWidth);
            if (aSlotCount > 1U) appendJunction(leftJoinX, rootAY, color);
            if (bSlotCount > 1U) appendJunction(rightJoinX, rootBY, color);
        }
    }
    for (uint8_t slot = 0; slot < visibleCount; ++slot) {
        const int y = yFor(slot);
        html += F("<circle cx='80' cy='"); html += String(y); html += F("' r='3' fill='#a8dff3'/><circle cx='920' cy='"); html += String(y); html += F("' r='3' fill='#a8dff3'/>");
    }
    html += F("</svg></div></main></body></html>");
    server_.send(200, "text/html; charset=utf-8", html);
}

void MapWebEditor::handleSave() {
    if (documentPortalActive_) {
        redirectToDocumentPortal();
        return;
    }
    if (!editingEnabled_) {
        server_.send(423, "text/plain; charset=utf-8", "Map editing is locked while scan/touch editor is active");
        return;
    }
    if (map_ == nullptr || store_ == nullptr) {
        server_.send(500, "text/plain", "Map not initialized");
        return;
    }
    CableMap updated = *map_;  // preserve A-A/B-B groups from touch editor
    for (uint8_t i = 0; i < kTestPointsPerSide; ++i) {
        const String arg = String("ab") + String(i);
        if (server_.hasArg(arg)) {
            updated.setAtoBMask(i, parseMask(server_.arg(arg)));
        }
    }
    updated.setName("CUSTOM MAP");
    *map_ = updated;
    store_->save(*map_);
    changed_ = true;
    Serial.println("[MAP-WEB] custom map saved from browser");
    Serial.flush();
    redirectToMap();
}

void MapWebEditor::handleOneToOne() {
    if (documentPortalActive_) {
        redirectToDocumentPortal();
        return;
    }
    if (!editingEnabled_) {
        server_.send(423, "text/plain; charset=utf-8", "Map editing is locked while scan/touch editor is active");
        return;
    }
    if (map_ != nullptr && store_ != nullptr) {
        map_->setOneToOne();
        map_->setName("CUSTOM MAP");
        store_->save(*map_);
        changed_ = true;
        Serial.println("[MAP-WEB] 1:1 map saved");
        Serial.flush();
    }
    redirectToMap();
}

void MapWebEditor::handleClear() {
    if (documentPortalActive_) {
        redirectToDocumentPortal();
        return;
    }
    if (!editingEnabled_) {
        server_.send(423, "text/plain; charset=utf-8", "Map editing is locked while scan/touch editor is active");
        return;
    }
    if (map_ != nullptr && store_ != nullptr) {
        map_->clear();
        map_->setName("CUSTOM MAP");
        store_->save(*map_);
        changed_ = true;
        Serial.println("[MAP-WEB] map cleared and saved");
        Serial.flush();
    }
    redirectToMap();
}

}  // namespace mg::p4
