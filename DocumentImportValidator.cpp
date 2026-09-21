#include "DocumentImportValidator.h"

#include <LittleFS.h>

#include "MapWebEditor.h"

namespace mg::p4 {

uint32_t DocumentImportValidator::crc32Update(uint32_t crc,
                                               const uint8_t* data,
                                               size_t length) {
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

bool DocumentImportValidator::validateJsonEnvelope(const String& path, String& reason) {
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
        reason = "Profile JSON cannot be opened";
        return false;
    }
    bool sawObjectStart = false;
    while (file.available()) {
        const int value = file.read();
        if (value < 0) break;
        const char c = static_cast<char>(value);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        sawObjectStart = c == '{';
        break;
    }
    file.close();
    if (!sawObjectStart) {
        reason = "Prepared profile is not a JSON object";
        return false;
    }
    return true;
}

DocumentValidationReport DocumentImportValidator::validate(MapWebEditor& portal) {
    DocumentValidationReport report;
    report.fileCount = portal.documentItemCount();
    report.totalBytes = portal.documentTotalBytes();
    if (report.fileCount == 0U) {
        report.message = "Prepared profile is missing";
        return report;
    }
    if (report.fileCount != 1U) {
        report.message = "Exactly one prepared profile is required";
        return report;
    }

    DocumentItemInfo item;
    if (!portal.documentItem(0U, item)) {
        report.failedIndex = 0;
        report.message = "Profile metadata is unavailable";
        return report;
    }
    if (item.kind != DocumentFileKind::Json) {
        report.failedIndex = 0;
        report.message = "Only prepared MG profile JSON is accepted";
        return report;
    }
    if (!LittleFS.exists(item.path)) {
        report.failedIndex = 0;
        report.message = "Profile file is missing";
        return report;
    }

    String jsonReason;
    if (!validateJsonEnvelope(item.path, jsonReason)) {
        report.failedIndex = 0;
        report.message = jsonReason;
        return report;
    }

    File file = LittleFS.open(item.path, FILE_READ);
    if (!file) {
        report.failedIndex = 0;
        report.message = "Profile JSON cannot be opened";
        return report;
    }
    const size_t actualSize = file.size();
    // Keep validation stack pressure low. BLE finalization runs
    // from Arduino loopTask, whose stack is intentionally modest on this P4
    // build. CRC throughput is irrelevant for a <=256 KiB profile, so a small
    // buffer is preferable to a 1 KiB automatic array.
    uint8_t buffer[256];
    uint32_t crc = 0xFFFFFFFFUL;
    size_t bytesRead = 0U;
    while (file.available()) {
        const size_t got = file.read(buffer, sizeof(buffer));
        if (got == 0U) break;
        crc = crc32Update(crc, buffer, got);
        bytesRead += got;
    }
    file.close();
    crc = ~crc;

    if (actualSize != item.size || bytesRead != item.size) {
        report.failedIndex = 0;
        report.message = "Profile size mismatch";
        return report;
    }
    if (crc != item.crc32) {
        report.failedIndex = 0;
        report.message = "Profile CRC32 mismatch";
        return report;
    }

    if (!portal.finalizeDocumentManifest()) {
        report.message = portal.lastDocumentError();
        return report;
    }

    report.ok = true;
    report.jsonCount = 1U;
    report.message = "Prepared mobile profile verified";
    return report;
}

}  // namespace mg::p4
