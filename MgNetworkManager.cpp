#include "MgNetworkManager.h"

// JC1060P470C_I_W_Y manufacturer Ethernet configuration:
// IP101, PHY address 1, MDC=31, MDIO=52, PHY reset/power=51, RMII.
#include <ETH.h>
#include <Network.h>
#include <Preferences.h>
#include <esp_system.h>

namespace {
constexpr int kEthPhyAddr = 1;
constexpr int kEthMdcPin = 31;
constexpr int kEthMdioPin = 52;
constexpr int kEthPowerPin = 51;
constexpr const char* kWifiPrefsNamespace = "mg_wifi";
constexpr const char* kWifiSsidKey = "ssid";
constexpr const char* kWifiPasswordKey = "pwd";
}

namespace mg::p4 {

MgNetworkManager* MgNetworkManager::instance_ = nullptr;

void MgNetworkManager::begin() {
    instance_ = this;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(100);

    Network.onEvent(networkEvent);
    loadSavedWifi();

    Serial.println("[NETWORK] Wi-Fi STA ready | Ethernet deferred");
    Serial.flush();

    if (hasSavedWifi()) {
        Serial.printf("[WIFI] saved network found: %s\n", savedSsid_.c_str());
        Serial.flush();
        connectSavedWifi();
    }
}

bool MgNetworkManager::ensureEthernetStarted() {
    if (ethernetStarted_) {
        return true;
    }
    const bool ok = ETH.begin(ETH_PHY_IP101, kEthPhyAddr, kEthMdcPin, kEthMdioPin, kEthPowerPin, EMAC_CLK_EXT_IN);
    ethernetStarted_ = ok;
    Serial.printf("[ETH] IP101 init=%s\n", ok ? "OK" : "FAILED");
    Serial.flush();
    return ok;
}

void MgNetworkManager::tick() {
    const wl_status_t status = WiFi.status();
    if (status != lastWifiStatus_) {
        lastWifiStatus_ = status;
        Serial.printf("[WIFI] status=%d\n", static_cast<int>(status));
        Serial.flush();
    }

    if (wifiConnecting_) {
        if (status == WL_CONNECTED) {
            wifiConnecting_ = false;
            wifiConnectStartMs_ = 0;
            Serial.printf("[WIFI] connected ssid=%s ip=%s\n",
                          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            Serial.flush();
            if (pendingRemember_) {
                saveWifiCredentials(pendingSsid_, pendingPassword_);
            }
            pendingSsid_ = String();
            pendingPassword_ = String();
            pendingRemember_ = false;
        } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL ||
                   (wifiConnectStartMs_ != 0 && millis() - wifiConnectStartMs_ >= 15000U)) {
            wifiConnecting_ = false;
            wifiConnectStartMs_ = 0;
            Serial.printf("[WIFI] connect failed/timeout status=%d\n", static_cast<int>(status));
            Serial.flush();
            pendingSsid_ = String();
            pendingPassword_ = String();
            pendingRemember_ = false;
        }
    }

    if (documentApActive_) {
        documentDns_.processNextRequest();
        const uint32_t now = millis();
        if (static_cast<int32_t>(now - documentApExpiresAtMs_) >= 0) {
            Serial.println("[DOC-AP] session timeout");
            Serial.flush();
            stopDocumentAp();
        }
    }
}

bool MgNetworkManager::startWifiScan() {
    WiFi.scanDelete();
    WiFi.mode(WIFI_STA);
    if (!wifiConnected() && !wifiConnecting_) {
        WiFi.disconnect(false, false);
        delay(100);
    }

    Serial.println("[WIFI] scan start");
    Serial.flush();
    const int result = WiFi.scanNetworks();
    Serial.printf("[WIFI] scan done result=%d\n", result);
    Serial.flush();
    return result >= 0;
}

bool MgNetworkManager::wifiScanRunning() const {
    return WiFi.scanComplete() == WIFI_SCAN_RUNNING;
}

int MgNetworkManager::wifiScanCount() const {
    const int result = WiFi.scanComplete();
    return result > 0 ? result : 0;
}

String MgNetworkManager::wifiSsid(int index) const {
    return (index >= 0 && index < wifiScanCount()) ? WiFi.SSID(index) : String();
}

int32_t MgNetworkManager::wifiRssi(int index) const {
    return (index >= 0 && index < wifiScanCount()) ? WiFi.RSSI(index) : 0;
}

bool MgNetworkManager::wifiEncrypted(int index) const {
    if (index < 0 || index >= wifiScanCount()) {
        return false;
    }
    return WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
}

void MgNetworkManager::clearWifiScan() {
    if (!wifiScanRunning()) {
        WiFi.scanDelete();
    }
}

bool MgNetworkManager::connectWifi(const String& ssid, const String& password, bool remember) {
    if (ssid.isEmpty()) {
        return false;
    }

    pendingSsid_ = ssid;
    pendingPassword_ = password;
    pendingRemember_ = remember;
    wifiConnecting_ = true;
    wifiConnectStartMs_ = millis();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(50);

    Serial.printf("[WIFI] connect start ssid=%s remember=%s\n",
                  ssid.c_str(), remember ? "yes" : "no");
    Serial.flush();

    if (password.isEmpty()) {
        WiFi.begin(ssid.c_str());
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
    }
    return true;
}

bool MgNetworkManager::connectSavedWifi() {
    if (!hasSavedWifi()) {
        return false;
    }
    return connectWifi(savedSsid_, savedPassword_, false);
}

void MgNetworkManager::disconnectWifi() {
    wifiConnecting_ = false;
    wifiConnectStartMs_ = 0;
    pendingSsid_ = String();
    pendingPassword_ = String();
    pendingRemember_ = false;
    WiFi.disconnect(false, false);
    Serial.println("[WIFI] disconnected by user");
    Serial.flush();
}

void MgNetworkManager::forgetWifi() {
    disconnectWifi();
    Preferences prefs;
    if (prefs.begin(kWifiPrefsNamespace, false)) {
        prefs.clear();
        prefs.end();
    }
    savedSsid_ = String();
    savedPassword_ = String();
    Serial.println("[WIFI] saved network forgotten");
    Serial.flush();
}

bool MgNetworkManager::hasSavedWifi() const {
    return !savedSsid_.isEmpty();
}

String MgNetworkManager::savedWifiSsid() const {
    return savedSsid_;
}

void MgNetworkManager::loadSavedWifi() {
    Preferences prefs;
    if (!prefs.begin(kWifiPrefsNamespace, true)) {
        return;
    }
    savedSsid_ = prefs.getString(kWifiSsidKey, "");
    savedPassword_ = prefs.getString(kWifiPasswordKey, "");
    prefs.end();
}

void MgNetworkManager::saveWifiCredentials(const String& ssid, const String& password) {
    Preferences prefs;
    if (prefs.begin(kWifiPrefsNamespace, false)) {
        prefs.putString(kWifiSsidKey, ssid);
        prefs.putString(kWifiPasswordKey, password);
        prefs.end();
        savedSsid_ = ssid;
        savedPassword_ = password;
        Serial.printf("[WIFI] credentials saved ssid=%s\n", ssid.c_str());
        Serial.flush();
    }
}

bool MgNetworkManager::wifiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String MgNetworkManager::wifiIp() const {
    return wifiConnected() ? WiFi.localIP().toString() : String("-");
}

String MgNetworkManager::wifiMac() const {
    return WiFi.macAddress();
}

String MgNetworkManager::wifiConnectedSsid() const {
    return wifiConnected() ? WiFi.SSID() : String("-");
}

String MgNetworkManager::ethernetIp() const {
    return ethernetHasIp_ ? ETH.localIP().toString() : String("-");
}

String MgNetworkManager::ethernetMac() const {
    return ethernetStarted_ ? ETH.macAddress() : String("-");
}

uint16_t MgNetworkManager::ethernetSpeedMbps() const {
    return ethernetLinkUp_ ? ETH.linkSpeed() : 0;
}

bool MgNetworkManager::ethernetFullDuplex() const {
    return ethernetLinkUp_ && ETH.fullDuplex();
}

void MgNetworkManager::networkEvent(arduino_event_id_t event) {
    if (!instance_) {
        return;
    }
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            instance_->ethernetStarted_ = true;
            ETH.setHostname("mg-smart-test");
            Serial.println("[ETH] started");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            instance_->ethernetLinkUp_ = true;
            Serial.println("[ETH] link connected");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            instance_->ethernetLinkUp_ = true;
            instance_->ethernetHasIp_ = true;
            Serial.printf("[ETH] DHCP IP=%s\n", ETH.localIP().toString().c_str());
            break;
        case ARDUINO_EVENT_ETH_LOST_IP:
            instance_->ethernetHasIp_ = false;
            Serial.println("[ETH] IP lost");
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            instance_->ethernetLinkUp_ = false;
            instance_->ethernetHasIp_ = false;
            Serial.println("[ETH] disconnected");
            break;
        case ARDUINO_EVENT_ETH_STOP:
            instance_->ethernetStarted_ = false;
            instance_->ethernetLinkUp_ = false;
            instance_->ethernetHasIp_ = false;
            Serial.println("[ETH] stopped");
            break;
        default:
            break;
    }
    Serial.flush();
}


bool MgNetworkManager::startDocumentAp(uint32_t timeoutSeconds) {
    if (documentApActive_) {
        touchDocumentApSession();
        return true;
    }

    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String suffix = mac.length() >= 4 ? mac.substring(mac.length() - 4) : String("P4");
    suffix.toUpperCase();
    documentApSsid_ = String("MG-DOC-") + suffix;

    const uint32_t randomA = esp_random();
    const uint32_t randomB = esp_random();
    char password[16] = {};
    snprintf(password, sizeof(password), "MG%08lX", static_cast<unsigned long>(randomA));
    documentApPassword_ = password;
    char token[20] = {};
    snprintf(token, sizeof(token), "%08lX%08lX",
             static_cast<unsigned long>(randomA),
             static_cast<unsigned long>(randomB));
    documentSessionToken_ = token;

    // Document Import is deliberately isolated from the company WLAN. A
    // simultaneous STA connection/scanning can force channel/coexistence work
    // on the C6 and was found to be a poor fit for sustained phone uploads.
    // Suspend STA for the short portal session; Ethernet, if present, is not
    // affected. Restore a previously connected saved WLAN after the portal.
    documentRestoreStaAfterAp_ = wifiConnected();
    wifiConnecting_ = false;
    wifiConnectStartMs_ = 0U;
    pendingSsid_ = String();
    pendingPassword_ = String();
    pendingRemember_ = false;
    WiFi.disconnect(false, false);
    delay(50);
    WiFi.mode(WIFI_AP);
    delay(50);
    const IPAddress apIp(192, 168, 4, 1);
    const IPAddress gateway(192, 168, 4, 1);
    const IPAddress netmask(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, gateway, netmask);
    const String requestedDocumentSsid = documentApSsid_;
    const bool ok = WiFi.softAP(documentApSsid_.c_str(),
                                documentApPassword_.c_str(),
                                1,
                                false,
                                1);
    if (!ok) {
        Serial.println("[DOC-AP] softAP start FAILED");
        Serial.flush();
        documentApSsid_ = String();
        documentApPassword_ = String();
        documentSessionToken_ = String();
        return false;
    }

    // ESP32-P4 uses the hosted Wi-Fi device. Read back the AP SSID instead of
    // assuming the requested name was applied by the C6. The P4 screen and QR
    // must advertise the network the phone can actually see.
    const String runtimeDocumentSsid = WiFi.softAPSSID();
    if (!runtimeDocumentSsid.isEmpty()) {
        if (runtimeDocumentSsid != requestedDocumentSsid) {
            Serial.printf("[DOC-AP] SSID readback differs requested=%s runtime=%s\n",
                          requestedDocumentSsid.c_str(), runtimeDocumentSsid.c_str());
        }
        documentApSsid_ = runtimeDocumentSsid;
    }

    documentDns_.setErrorReplyCode(DNSReplyCode::NoError);
    documentDns_.start(53, "*", WiFi.softAPIP());
    documentApTimeoutMs_ = timeoutSeconds < 60U ? 60000U : timeoutSeconds * 1000U;
    documentApActive_ = true;
    touchDocumentApSession();

    Serial.printf("[DOC-AP] started ssid=%s ip=%s timeout=%lus clients=1 max\n",
                  documentApSsid_.c_str(),
                  WiFi.softAPIP().toString().c_str(),
                  static_cast<unsigned long>(documentApTimeoutMs_ / 1000U));
    Serial.flush();
    return true;
}

void MgNetworkManager::stopDocumentAp() {
    if (!documentApActive_) {
        return;
    }
    const bool restoreSta = documentRestoreStaAfterAp_;
    documentDns_.stop();
    // Keep the ESP-Hosted transport alive when the temporary document
    // AP ends. softAPdisconnect(true) turns Wi-Fi fully off on P4 hosted Wi-Fi,
    // which deinitializes the SDIO transport; the immediate STA restart then
    // needs a fresh contiguous DMA mempool and can assert after LVGL and heavy UI activity have
    // fragmented internal RAM. Stop only the AP interface and retain Wi-Fi.
    WiFi.softAPdisconnect(false);
    delay(30);
    WiFi.mode(WIFI_STA);
    documentApActive_ = false;
    documentApExpiresAtMs_ = 0U;
    documentApSsid_ = String();
    documentApPassword_ = String();
    documentSessionToken_ = String();
    documentRestoreStaAfterAp_ = false;
    Serial.printf("[DOC-AP][F10] stopped without Hosted deinit; restore STA=%s\n", restoreSta ? "yes" : "no");
    Serial.flush();
    if (restoreSta && hasSavedWifi()) {
        connectSavedWifi();
    }
}

void MgNetworkManager::touchDocumentApSession() {
    if (!documentApActive_) {
        return;
    }
    documentApExpiresAtMs_ = millis() + documentApTimeoutMs_;
}

String MgNetworkManager::documentApIp() const {
    return documentApActive_ ? WiFi.softAPIP().toString() : String("-");
}

uint32_t MgNetworkManager::documentSessionRemainingSeconds() const {
    if (!documentApActive_) {
        return 0U;
    }
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - documentApExpiresAtMs_) >= 0) {
        return 0U;
    }
    return (documentApExpiresAtMs_ - now + 999U) / 1000U;
}

} // namespace mg::p4
