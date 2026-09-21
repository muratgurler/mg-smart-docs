#pragma once

#include <stddef.h>
#include <stdint.h>

#include "WorkflowArchiveCore.h"

namespace mg::p4 {

class WorkflowArchivePersistence {
public:
    static constexpr const char* kArchivePath = "/mgdata/workflow_archive.bin";
    static constexpr const char* kArchiveTempPath = "/mgdata/workflow_archive.tmp";
    static constexpr const char* kArchiveBackupPath = "/mgdata/workflow_archive.bak";
    static constexpr const char* kAllResultsCsvPath = "/mgreports/results_all.csv";
    static constexpr const char* kSelectedResultCsvPath = "/mgreports/result_selected.csv";

    bool begin(WorkflowArchiveCore& archive);
    bool load();
    void tick();
    bool saveNow();

    bool exportAllCsv();
    bool exportRecordCsv(size_t newestOffset);

    bool fsReady() const { return fsReady_; }
    bool loadedFromFlash() const { return loadedFromFlash_; }
    bool dirty() const;
    uint32_t successfulSaveCount() const { return successfulSaveCount_; }
    const char* statusText() const { return statusText_; }
    const char* lastExportPath() const { return lastExportPath_; }

private:
    void setStatus(const char* text);
    bool ensureDirectories();

    WorkflowArchiveCore* archive_ = nullptr;
    bool fsReady_ = false;
    bool loadedFromFlash_ = false;
    uint32_t savedSerial_ = 0U;
    uint32_t observedSerial_ = 0U;
    uint32_t dirtySinceMs_ = 0U;
    uint32_t successfulSaveCount_ = 0U;
    char statusText_[96] = {};
    char lastExportPath_[64] = {};
};

}  // namespace mg::p4
