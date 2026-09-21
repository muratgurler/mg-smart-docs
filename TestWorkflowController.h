#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CableMap.h"
#include "CableProfile.h"
#include "ScanSession.h"

namespace mg::p4 {

class WorkflowArchiveCore;

enum class TestProfileSource : uint8_t {
    None = 0,
    ManualNormal,
    ManualSubD,
    CustomMap,
    CableLearn,
    BarcodeMgProfile,
    BarcodeCustomerReference,
    ProductionPr,
    DocumentImport,
    ProfileCache,
};

enum class TestWorkflowState : uint8_t {
    Empty = 0,
    Loading,
    NeedsLookup,
    Ready,
    Running,
    Paused,
    Complete,
    Blocked,
};

enum class NextCableDisposition : uint8_t {
    NotAvailable = 0,
    ReadySameProfile,
    NeedsNewIdentifier,
};

enum class TestWorkflowVerdict : uint8_t {
    None = 0,
    Pass,
    Warning,
    Fail,
};

struct TestWorkflowIdentity {
    char productionPr[24] = {};
    char customerReference[40] = {};
    char revision[20] = {};
    char profileId[40] = {};
    char sourceCode[48] = {};
};

struct TestWorkflowSnapshot {
    TestProfileSource source = TestProfileSource::None;
    TestWorkflowState state = TestWorkflowState::Empty;
    TestWorkflowVerdict verdict = TestWorkflowVerdict::None;
    bool profileValid = false;
    bool customMap = false;
    bool autoStartRequested = false; // frozen invariant: must remain false
    bool operatorStartRequired = true;
    uint32_t generation = 0U;
    uint16_t attempt = 0U;
    uint16_t electricalErrors = 0U;
    uint8_t progressPercent = 0U;
    uint8_t signalPinCount = 0U;
    TestWorkflowIdentity identity{};
};

// Top-level profile/test/result coordinator.
//
// Barcode, PR/server, document import, Cable Learn and manual setup all enter
// through this controller. The controller owns a stable copy of the selected
// profile/map and enforces the single READY -> operator START -> TEST -> RESULT
// lifecycle. It performs no hardware I/O; the existing ScanSession and later
// hardware drivers remain the execution layer.
class TestWorkflowController {
public:
    TestWorkflowController();

    void attachArchive(WorkflowArchiveCore& archive);

    void resetAll();
    void clearForNewSource(TestProfileSource source, const char* sourceCode = nullptr);

    bool prepareProfile(TestProfileSource source,
                        const CableProfile& profile,
                        const CableMap* map = nullptr,
                        const TestWorkflowIdentity* identity = nullptr);

    bool prepareManual(const CableProfile& profile, bool customMap, const CableMap* map = nullptr);
    bool prepareCableLearn(const CableProfile& profile, const CableMap& learnedMap);
    bool prepareDocument(const CableProfile& profile,
                         const CableMap& map,
                         const char* profileId = "DOC-DRAFT");
    bool prepareDocument(const CableProfile& profile,
                         const CableMap& map,
                         const TestWorkflowIdentity& identity);
    bool prepareCachedProfile(const CableProfile& profile,
                              const CableMap& map,
                              bool customMap,
                              const TestWorkflowIdentity& identity);

    bool prepareRetest();
    NextCableDisposition prepareNextCable();

    // Barcode workflow bridge. PR/customer codes are intentionally not made
    // READY by the barcode alone; they remain NEEDS_LOOKUP until the profile is
    // resolved by the network/server layer. Local MG profile codes can become
    // READY immediately.
    void acceptBarcodeClassification(const char* code,
                                     bool isProductionPr,
                                     bool isCustomerReference,
                                     bool isMgLocalProfile,
                                     bool accepted,
                                     const char* profileId,
                                     const char* revision);

    bool acceptNetworkResolvedProfile(const char* pr,
                                      const char* customerReference,
                                      const char* revision,
                                      const char* profileId,
                                      const CableProfile& profile,
                                      const CableMap* map = nullptr);

    // Same resolved-profile bridge, but preserves which identifier initiated
    // the lookup. PR and customer-reference workflows remain distinct in the
    // archive while both identifiers are stored in the same DUT identity.
    bool acceptNetworkResolvedProfileForSource(TestProfileSource source,
                                               const char* sourceCode,
                                               const char* pr,
                                               const char* customerReference,
                                               const char* revision,
                                               const char* profileId,
                                               const CableProfile& profile,
                                               const CableMap* map = nullptr);

    // Scan runtime bridge. markStartRequest never starts hardware by itself;
    // it only verifies that START is legal. The caller then invokes the real
    // ScanSession/driver action and reports the resulting run state here.
    bool markStartRequest();
    void observeRunState(RunState state, uint16_t electricalErrors, uint8_t progressPercent);
    void markQualityWarning(bool warning);
    void markBlocked(const char* reason);

    bool startAllowed() const;
    bool hasPreparedProfile() const { return snapshot_.profileValid; }
    const CableProfile& profile() const { return activeProfile_; }
    const CableMap& map() const { return activeMap_; }
    bool usesCustomMap() const { return snapshot_.customMap; }
    bool qualityWarning() const { return qualityWarning_; }
    const TestWorkflowSnapshot& snapshot() const { return snapshot_; }
    const char* blockReason() const { return blockReason_; }

    void formatSummary(char* output, size_t outputSize, bool turkish) const;
    void formatIdentity(char* output, size_t outputSize) const;

    static const char* sourceText(TestProfileSource source, bool turkish);
    static const char* stateText(TestWorkflowState state, bool turkish);
    static const char* verdictText(TestWorkflowVerdict verdict);

private:
    static void copyText(char* dst, size_t dstSize, const char* src);
    void deepCopyProfile(const CableProfile& profile);
    void refreshVerdict(uint16_t electricalErrors);

    CableProfile activeProfile_{};
    CableMap activeMap_{};
    TestWorkflowSnapshot snapshot_{};
    bool qualityWarning_ = false;
    char profileName_[48] = {};
    char connectorAName_[40] = {};
    char connectorBName_[40] = {};
    char blockReason_[96] = {};
    WorkflowArchiveCore* archive_ = nullptr;
};

}  // namespace mg::p4
