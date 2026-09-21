#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CableMap.h"
#include "CableProfile.h"
#include "TestWorkflowController.h"

namespace mg::p4 {

constexpr size_t kWorkflowResultCapacity = 32U;
constexpr size_t kWorkflowProfileCacheCapacity = 8U;
constexpr size_t kWorkflowArchiveBinaryMaxBytes = 32768U;

struct WorkflowTestRecord {
    uint32_t recordId = 0U;
    uint32_t generation = 0U;
    uint16_t attempt = 0U;
    TestProfileSource source = TestProfileSource::None;
    TestWorkflowVerdict verdict = TestWorkflowVerdict::None;
    TestWorkflowIdentity identity{};
    uint16_t electricalErrors = 0U;
    uint8_t signalPinCount = 0U;
    bool customMap = false;
    bool qualityWarning = false;
    uint64_t mapFingerprint = 0ULL;
    char profileName[48] = {};
    char connectorA[40] = {};
    char connectorB[40] = {};
};

struct WorkflowCachedProfile {
    bool valid = false;
    uint32_t cacheId = 0U;
    uint32_t lastUseOrdinal = 0U;
    TestProfileSource source = TestProfileSource::None;
    TestWorkflowIdentity identity{};
    bool customMap = false;
    uint64_t mapFingerprint = 0ULL;
    CableProfile profile{};
    CableMap map{};
    char profileName[48] = {};
    char connectorA[40] = {};
    char connectorB[40] = {};
};

// Result/archive and profile-cache backbone.
//
// The core stays filesystem-independent. Extra23 attaches a versioned,
// CRC-protected LittleFS persistence driver and CSV exporter around this flat
// model, so future SD/server backends can reuse the same operator workflow.
class WorkflowArchiveCore {
public:
    void reset();

    // Upsert means a late quality update can correct the same completed run
    // instead of creating a duplicate result record.
    bool upsertCompleted(const TestWorkflowSnapshot& snapshot,
                         const CableProfile& profile,
                         const CableMap& map,
                         bool qualityWarning);

    bool cacheProfile(TestProfileSource source,
                      const TestWorkflowIdentity& identity,
                      const CableProfile& profile,
                      const CableMap& map,
                      bool customMap);

    size_t resultCount() const { return resultCount_; }
    size_t profileCacheCount() const { return profileCacheCount_; }

    // offset=0 is newest / most-recently-used.
    const WorkflowTestRecord* resultNewest(size_t offset) const;
    const WorkflowCachedProfile* cachedProfileNewest(size_t offset) const;

    bool loadCachedProfile(size_t newestOffset,
                           TestProfileSource& source,
                           TestWorkflowIdentity& identity,
                           const CableProfile*& profile,
                           const CableMap*& map,
                           bool& customMap);

    uint16_t passCount() const;
    uint16_t warningCount() const;
    uint16_t failCount() const;
    uint16_t firstAttemptCount() const;
    uint16_t firstPassCount() const;
    uint16_t retestCount() const;

    void formatRecord(const WorkflowTestRecord& record,
                      char* output,
                      size_t outputSize,
                      bool turkish) const;
    void formatRecordCsv(const WorkflowTestRecord& record,
                         char* output,
                         size_t outputSize) const;

    static uint64_t fingerprintMap(const CableMap& map);

    // Persistence seam. These restore helpers accept already-validated flat
    // records from the binary codec. They deliberately deep-bind profile
    // strings so no pointer into a temporary decode buffer survives.
    bool restoreRecord(const WorkflowTestRecord& record);
    bool restoreCachedProfile(const WorkflowCachedProfile& item);
    uint32_t changeSerial() const { return changeSerial_; }

private:
    static void copyText(char* dst, size_t dstSize, const char* src);
    static void bindProfileStrings(WorkflowCachedProfile& item);
    static bool cacheKeyMatches(const WorkflowCachedProfile& item,
                                TestProfileSource source,
                                const TestWorkflowIdentity& identity,
                                const CableProfile& profile,
                                bool customMap,
                                uint64_t mapFingerprint);
    int findResult(uint32_t generation, uint16_t attempt) const;
    int findCache(TestProfileSource source,
                  const TestWorkflowIdentity& identity,
                  const CableProfile& profile,
                  bool customMap,
                  uint64_t mapFingerprint) const;
    size_t cacheIndexNewest(size_t offset) const;
    void markChanged();

    WorkflowTestRecord results_[kWorkflowResultCapacity]{};
    WorkflowCachedProfile profiles_[kWorkflowProfileCacheCapacity]{};
    size_t resultCount_ = 0U;
    size_t resultWriteIndex_ = 0U;
    size_t profileCacheCount_ = 0U;
    uint32_t nextRecordId_ = 1U;
    uint32_t nextCacheId_ = 1U;
    uint32_t useOrdinal_ = 1U;
    uint32_t changeSerial_ = 0U;
};

}  // namespace mg::p4
