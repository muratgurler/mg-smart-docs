#include "WorkflowArchivePersistence.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "WorkflowArchiveBinaryCodec.h"

namespace mg::p4 {
namespace {

constexpr uint32_t kDeferredSaveMs = 1500U;
constexpr const char* kCsvHeader =
    "record_id,generation,attempt,source,verdict,electrical_errors,signal_pins,"
    "pr,customer_ref,revision,profile_id,profile_name,map,quality_warning,map_fingerprint\n";

}  // namespace

void WorkflowArchivePersistence::setStatus(const char* text) {
    snprintf(statusText_, sizeof(statusText_), "%s", text != nullptr ? text : "");
}

bool WorkflowArchivePersistence::ensureDirectories() {
    if (!fsReady_) return false;
    if (!LittleFS.exists("/mgdata") && !LittleFS.mkdir("/mgdata")) return false;
    if (!LittleFS.exists("/mgreports") && !LittleFS.mkdir("/mgreports")) return false;
    return true;
}

bool WorkflowArchivePersistence::begin(WorkflowArchiveCore& archive) {
    archive_ = &archive;
    fsReady_ = LittleFS.begin(false);
    loadedFromFlash_ = false;
    lastExportPath_[0] = '\0';
    if (!fsReady_) {
        setStatus("LittleFS unavailable; archive remains RAM-only");
        return false;
    }
    if (!ensureDirectories()) {
        setStatus("LittleFS directories unavailable");
        return false;
    }
    observedSerial_ = archive_->changeSerial();
    savedSerial_ = observedSerial_;
    setStatus("LittleFS ready");
    return true;
}

bool WorkflowArchivePersistence::load() {
    if (!fsReady_ || archive_ == nullptr) return false;
    const char* loadPath = kArchivePath;
    if (!LittleFS.exists(kArchivePath) && LittleFS.exists(kArchiveBackupPath)) {
        loadPath = kArchiveBackupPath;
    }
    if (!LittleFS.exists(loadPath)) {
        observedSerial_ = archive_->changeSerial();
        savedSerial_ = observedSerial_;
        loadedFromFlash_ = false;
        setStatus("No saved archive yet");
        return true;
    }

    File file = LittleFS.open(loadPath, FILE_READ);
    if (!file) {
        setStatus("Archive open failed");
        return false;
    }
    const size_t size = static_cast<size_t>(file.size());
    if (size == 0U || size > kWorkflowArchiveBinaryMaxBytes) {
        file.close();
        setStatus("Archive size invalid");
        return false;
    }
    uint8_t* buffer = static_cast<uint8_t*>(malloc(size));
    if (buffer == nullptr) {
        file.close();
        setStatus("Archive load out of memory");
        return false;
    }
    const size_t readCount = file.read(buffer, size);
    file.close();
    if (readCount != size || !WorkflowArchiveBinaryCodec::decode(buffer, size, *archive_)) {
        free(buffer);
        setStatus("Archive CRC/schema invalid; ignored");
        return false;
    }
    free(buffer);
    observedSerial_ = archive_->changeSerial();
    savedSerial_ = observedSerial_;
    loadedFromFlash_ = true;
    dirtySinceMs_ = 0U;
    setStatus("Archive restored: /mgdata/workflow_archive.bin");
    return true;
}

bool WorkflowArchivePersistence::dirty() const {
    return archive_ != nullptr && archive_->changeSerial() != savedSerial_;
}

void WorkflowArchivePersistence::tick() {
    if (!fsReady_ || archive_ == nullptr) return;
    const uint32_t serial = archive_->changeSerial();
    if (serial != observedSerial_) {
        observedSerial_ = serial;
        dirtySinceMs_ = millis();
    }
    if (!dirty()) {
        dirtySinceMs_ = 0U;
        return;
    }
    if (dirtySinceMs_ == 0U) dirtySinceMs_ = millis();
    if (static_cast<uint32_t>(millis() - dirtySinceMs_) >= kDeferredSaveMs) {
        (void)saveNow();
    }
}

bool WorkflowArchivePersistence::saveNow() {
    if (!fsReady_ || archive_ == nullptr || !ensureDirectories()) return false;

    uint8_t* buffer = static_cast<uint8_t*>(malloc(kWorkflowArchiveBinaryMaxBytes));
    if (buffer == nullptr) {
        setStatus("Archive save out of memory");
        return false;
    }
    size_t encodedSize = 0U;
    if (!WorkflowArchiveBinaryCodec::encode(*archive_, buffer,
                                            kWorkflowArchiveBinaryMaxBytes,
                                            encodedSize)) {
        free(buffer);
        setStatus("Archive encode failed");
        return false;
    }

    if (LittleFS.exists(kArchiveTempPath)) LittleFS.remove(kArchiveTempPath);
    File file = LittleFS.open(kArchiveTempPath, FILE_WRITE);
    if (!file) {
        free(buffer);
        setStatus("Archive temp open failed");
        return false;
    }
    const size_t written = file.write(buffer, encodedSize);
    file.flush();
    file.close();
    free(buffer);
    if (written != encodedSize) {
        LittleFS.remove(kArchiveTempPath);
        setStatus("Archive write incomplete");
        return false;
    }

    // Two-phase replacement. Keep the previous valid archive as a backup
    // until the new CRC-protected temp file has been promoted successfully.
    if (LittleFS.exists(kArchiveBackupPath)) LittleFS.remove(kArchiveBackupPath);
    const bool hadPrevious = LittleFS.exists(kArchivePath);
    if (hadPrevious && !LittleFS.rename(kArchivePath, kArchiveBackupPath)) {
        LittleFS.remove(kArchiveTempPath);
        setStatus("Archive backup rename failed");
        return false;
    }
    if (!LittleFS.rename(kArchiveTempPath, kArchivePath)) {
        LittleFS.remove(kArchiveTempPath);
        if (hadPrevious && LittleFS.exists(kArchiveBackupPath)) {
            (void)LittleFS.rename(kArchiveBackupPath, kArchivePath);
        }
        setStatus("Archive promote rename failed");
        return false;
    }
    if (LittleFS.exists(kArchiveBackupPath)) LittleFS.remove(kArchiveBackupPath);

    savedSerial_ = archive_->changeSerial();
    observedSerial_ = savedSerial_;
    dirtySinceMs_ = 0U;
    ++successfulSaveCount_;
    setStatus("Archive saved: /mgdata/workflow_archive.bin");
    return true;
}

bool WorkflowArchivePersistence::exportAllCsv() {
    if (!fsReady_ || archive_ == nullptr || !ensureDirectories()) return false;
    File file = LittleFS.open(kAllResultsCsvPath, FILE_WRITE);
    if (!file) {
        setStatus("CSV export open failed");
        return false;
    }
    file.print(kCsvHeader);
    char row[640]{};
    for (size_t offset = archive_->resultCount(); offset > 0U; --offset) {
        const WorkflowTestRecord* record = archive_->resultNewest(offset - 1U);
        if (record == nullptr) continue;
        archive_->formatRecordCsv(*record, row, sizeof(row));
        file.print(row);
        file.print('\n');
    }
    file.flush();
    file.close();
    snprintf(lastExportPath_, sizeof(lastExportPath_), "%s", kAllResultsCsvPath);
    setStatus("CSV ready: /mgreports/results_all.csv");
    return true;
}

bool WorkflowArchivePersistence::exportRecordCsv(size_t newestOffset) {
    if (!fsReady_ || archive_ == nullptr || !ensureDirectories()) return false;
    const WorkflowTestRecord* record = archive_->resultNewest(newestOffset);
    if (record == nullptr) {
        setStatus("No selected result to export");
        return false;
    }
    File file = LittleFS.open(kSelectedResultCsvPath, FILE_WRITE);
    if (!file) {
        setStatus("Selected CSV open failed");
        return false;
    }
    file.print(kCsvHeader);
    char row[640]{};
    archive_->formatRecordCsv(*record, row, sizeof(row));
    file.print(row);
    file.print('\n');
    file.flush();
    file.close();
    snprintf(lastExportPath_, sizeof(lastExportPath_), "%s", kSelectedResultCsvPath);
    setStatus("CSV ready: /mgreports/result_selected.csv");
    return true;
}

}  // namespace mg::p4
