#include "TestWorkflowController.h"

#include "WorkflowArchiveCore.h"

#include <stdio.h>
#include <string.h>

namespace mg::p4 {

TestWorkflowController::TestWorkflowController() {
    resetAll();
}

void TestWorkflowController::attachArchive(WorkflowArchiveCore& archive) {
    archive_ = &archive;
    if (snapshot_.profileValid) {
        archive_->cacheProfile(snapshot_.source, snapshot_.identity, activeProfile_, activeMap_, snapshot_.customMap);
    }
}

void TestWorkflowController::copyText(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0U) return;
    snprintf(dst, dstSize, "%s", src != nullptr ? src : "");
}

void TestWorkflowController::deepCopyProfile(const CableProfile& profile) {
    activeProfile_ = profile;
    copyText(profileName_, sizeof(profileName_), profile.name);
    copyText(connectorAName_, sizeof(connectorAName_), profile.sideA.displayName);
    copyText(connectorBName_, sizeof(connectorBName_), profile.sideB.displayName);
    activeProfile_.name = profileName_;
    activeProfile_.sideA.displayName = connectorAName_;
    activeProfile_.sideB.displayName = connectorBName_;
}

void TestWorkflowController::resetAll() {
    activeMap_ = makeOneToOneCableMap();
    snapshot_ = {};
    snapshot_.operatorStartRequired = true;
    snapshot_.autoStartRequested = false;
    qualityWarning_ = false;
    blockReason_[0] = '\0';
    deepCopyProfile(makeNormalProfile());
}

void TestWorkflowController::clearForNewSource(TestProfileSource source, const char* sourceCode) {
    const uint32_t nextGeneration = snapshot_.generation + 1U;
    snapshot_ = {};
    snapshot_.source = source;
    snapshot_.state = TestWorkflowState::Loading;
    snapshot_.generation = nextGeneration;
    snapshot_.operatorStartRequired = true;
    snapshot_.autoStartRequested = false;
    copyText(snapshot_.identity.sourceCode, sizeof(snapshot_.identity.sourceCode), sourceCode);
    qualityWarning_ = false;
    blockReason_[0] = '\0';
    activeMap_ = makeOneToOneCableMap();
}

bool TestWorkflowController::prepareProfile(TestProfileSource source,
                                            const CableProfile& profile,
                                            const CableMap* map,
                                            const TestWorkflowIdentity* identity) {
    if (profile.signalPinCount > kMaximumSignalPins) {
        markBlocked("signal pin count exceeds runtime limit");
        return false;
    }

    const uint32_t generation = snapshot_.generation == 0U ? 1U : snapshot_.generation;
    const TestWorkflowIdentity priorIdentity = snapshot_.identity;
    snapshot_ = {};
    snapshot_.source = source;
    snapshot_.state = TestWorkflowState::Ready;
    snapshot_.verdict = TestWorkflowVerdict::None;
    snapshot_.profileValid = true;
    snapshot_.customMap = map != nullptr;
    snapshot_.operatorStartRequired = true;
    snapshot_.autoStartRequested = false;
    snapshot_.generation = generation;
    snapshot_.attempt = 1U;
    snapshot_.signalPinCount = profile.signalPinCount;
    snapshot_.identity = identity != nullptr ? *identity : priorIdentity;
    qualityWarning_ = false;
    blockReason_[0] = '\0';

    deepCopyProfile(profile);
    if (map != nullptr) {
        activeMap_ = *map;
    } else {
        activeMap_ = makeOneToOneCableMap();
    }
    if (archive_ != nullptr) {
        archive_->cacheProfile(source, snapshot_.identity, activeProfile_, activeMap_, snapshot_.customMap);
    }
    return true;
}

bool TestWorkflowController::prepareManual(const CableProfile& profile,
                                           bool customMap,
                                           const CableMap* map) {
    const TestProfileSource source = profile.kind == ScanProfileKind::SubD
        ? TestProfileSource::ManualSubD
        : (customMap ? TestProfileSource::CustomMap : TestProfileSource::ManualNormal);
    clearForNewSource(source, customMap ? "MANUAL-CUSTOM" : "MANUAL");
    return prepareProfile(source, profile, customMap ? map : nullptr, nullptr);
}

bool TestWorkflowController::prepareCableLearn(const CableProfile& profile,
                                               const CableMap& learnedMap) {
    clearForNewSource(TestProfileSource::CableLearn, "CABLE-LEARN");
    TestWorkflowIdentity id{};
    copyText(id.profileId, sizeof(id.profileId), "LEARNED-MAP");
    return prepareProfile(TestProfileSource::CableLearn, profile, &learnedMap, &id);
}

bool TestWorkflowController::prepareDocument(const CableProfile& profile,
                                             const CableMap& map,
                                             const char* profileId) {
    clearForNewSource(TestProfileSource::DocumentImport, "DOCUMENT");
    TestWorkflowIdentity id{};
    copyText(id.profileId, sizeof(id.profileId), profileId);
    copyText(id.sourceCode, sizeof(id.sourceCode), "DOCUMENT");
    return prepareProfile(TestProfileSource::DocumentImport, profile, &map, &id);
}

bool TestWorkflowController::prepareDocument(const CableProfile& profile,
                                             const CableMap& map,
                                             const TestWorkflowIdentity& identity) {
    clearForNewSource(TestProfileSource::DocumentImport,
                      identity.sourceCode[0] != '\0' ? identity.sourceCode : "DOCUMENT");
    TestWorkflowIdentity id = identity;
    if (id.sourceCode[0] == '\0') {
        copyText(id.sourceCode, sizeof(id.sourceCode), "DOCUMENT");
    }
    return prepareProfile(TestProfileSource::DocumentImport, profile, &map, &id);
}

bool TestWorkflowController::prepareCachedProfile(const CableProfile& profile,
                                                   const CableMap& map,
                                                   bool customMap,
                                                   const TestWorkflowIdentity& identity) {
    clearForNewSource(TestProfileSource::ProfileCache, "CACHE");
    TestWorkflowIdentity cached = identity;
    cached.productionPr[0] = '\0';
    copyText(cached.sourceCode, sizeof(cached.sourceCode), "CACHE");
    return prepareProfile(TestProfileSource::ProfileCache, profile, customMap ? &map : nullptr, &cached);
}

bool TestWorkflowController::prepareRetest() {
    if (!snapshot_.profileValid || snapshot_.state != TestWorkflowState::Complete) return false;
    snapshot_.state = TestWorkflowState::Ready;
    snapshot_.verdict = TestWorkflowVerdict::None;
    snapshot_.electricalErrors = 0U;
    snapshot_.progressPercent = 0U;
    snapshot_.autoStartRequested = false;
    snapshot_.operatorStartRequired = true;
    snapshot_.attempt = snapshot_.attempt < UINT16_MAX ? static_cast<uint16_t>(snapshot_.attempt + 1U) : 1U;
    qualityWarning_ = false;
    blockReason_[0] = '\0';
    return true;
}

NextCableDisposition TestWorkflowController::prepareNextCable() {
    if (!snapshot_.profileValid || snapshot_.state != TestWorkflowState::Complete) {
        return NextCableDisposition::NotAvailable;
    }
    snapshot_.generation += 1U;
    snapshot_.verdict = TestWorkflowVerdict::None;
    snapshot_.electricalErrors = 0U;
    snapshot_.progressPercent = 0U;
    snapshot_.autoStartRequested = false;
    snapshot_.operatorStartRequired = true;
    qualityWarning_ = false;
    blockReason_[0] = '\0';

    if (snapshot_.source == TestProfileSource::ProductionPr ||
        snapshot_.source == TestProfileSource::BarcodeCustomerReference) {
        snapshot_.state = TestWorkflowState::NeedsLookup;
        snapshot_.profileValid = false;
        snapshot_.attempt = 0U;
        snapshot_.identity.productionPr[0] = '\0';
        snapshot_.identity.sourceCode[0] = '\0';
        return NextCableDisposition::NeedsNewIdentifier;
    }

    snapshot_.state = TestWorkflowState::Ready;
    snapshot_.attempt = 1U;
    return NextCableDisposition::ReadySameProfile;
}

void TestWorkflowController::acceptBarcodeClassification(const char* code,
                                                         bool isProductionPr,
                                                         bool isCustomerReference,
                                                         bool isMgLocalProfile,
                                                         bool accepted,
                                                         const char* profileId,
                                                         const char* revision) {
    TestProfileSource source = TestProfileSource::None;
    if (isProductionPr) source = TestProfileSource::ProductionPr;
    else if (isCustomerReference) source = TestProfileSource::BarcodeCustomerReference;
    else if (isMgLocalProfile) source = TestProfileSource::BarcodeMgProfile;

    clearForNewSource(source, code);
    copyText(snapshot_.identity.profileId, sizeof(snapshot_.identity.profileId), profileId);
    copyText(snapshot_.identity.revision, sizeof(snapshot_.identity.revision), revision);
    if (isProductionPr) copyText(snapshot_.identity.productionPr, sizeof(snapshot_.identity.productionPr), code);
    if (isCustomerReference) copyText(snapshot_.identity.customerReference, sizeof(snapshot_.identity.customerReference), code);

    if (!accepted || source == TestProfileSource::None) {
        snapshot_.state = TestWorkflowState::Blocked;
        snapshot_.verdict = TestWorkflowVerdict::Warning;
        copyText(blockReason_, sizeof(blockReason_), "unknown/unassigned code");
        return;
    }

    if (isProductionPr || isCustomerReference) {
        snapshot_.state = TestWorkflowState::NeedsLookup;
        snapshot_.profileValid = false;
        snapshot_.verdict = TestWorkflowVerdict::None;
        return;
    }

    // Local MG profile demo: a resolved local profile can enter READY without
    // any company-network dependency, but still cannot auto-start.
    CableProfile local = makeNormalProfile(12U, true, true);
    TestWorkflowIdentity id = snapshot_.identity;
    prepareProfile(TestProfileSource::BarcodeMgProfile, local, nullptr, &id);
}

bool TestWorkflowController::acceptNetworkResolvedProfile(const char* pr,
                                                          const char* customerReference,
                                                          const char* revision,
                                                          const char* profileId,
                                                          const CableProfile& profile,
                                                          const CableMap* map) {
    return acceptNetworkResolvedProfileForSource(TestProfileSource::ProductionPr,
                                                 pr, pr, customerReference, revision,
                                                 profileId, profile, map);
}

bool TestWorkflowController::acceptNetworkResolvedProfileForSource(
    TestProfileSource source,
    const char* sourceCode,
    const char* pr,
    const char* customerReference,
    const char* revision,
    const char* profileId,
    const CableProfile& profile,
    const CableMap* map) {
    if (source != TestProfileSource::ProductionPr &&
        source != TestProfileSource::BarcodeCustomerReference) {
        markBlocked("network profile source must be PR or customer code");
        return false;
    }
    if (pr == nullptr || pr[0] == '\0') {
        markBlocked("PR lookup returned no production key");
        return false;
    }
    if (customerReference == nullptr || customerReference[0] == '\0') {
        markBlocked("lookup returned no customer reference");
        return false;
    }
    if (revision == nullptr || revision[0] == '\0') {
        markBlocked("lookup returned no revision");
        return false;
    }
    if (profileId == nullptr || profileId[0] == '\0') {
        markBlocked("lookup returned no profile id");
        return false;
    }

    clearForNewSource(source, sourceCode);
    TestWorkflowIdentity id{};
    copyText(id.productionPr, sizeof(id.productionPr), pr);
    copyText(id.customerReference, sizeof(id.customerReference), customerReference);
    copyText(id.revision, sizeof(id.revision), revision);
    copyText(id.profileId, sizeof(id.profileId), profileId);
    copyText(id.sourceCode, sizeof(id.sourceCode), sourceCode);
    return prepareProfile(source, profile, map, &id);
}

bool TestWorkflowController::startAllowed() const {
    return snapshot_.profileValid &&
           (snapshot_.state == TestWorkflowState::Ready ||
            snapshot_.state == TestWorkflowState::Paused);
}

bool TestWorkflowController::markStartRequest() {
    if (!startAllowed()) return false;
    // Intentional: no state change to Running until ScanSession confirms it.
    snapshot_.autoStartRequested = false;
    return true;
}

void TestWorkflowController::refreshVerdict(uint16_t electricalErrors) {
    if (electricalErrors > 0U) snapshot_.verdict = TestWorkflowVerdict::Fail;
    else if (qualityWarning_) snapshot_.verdict = TestWorkflowVerdict::Warning;
    else snapshot_.verdict = TestWorkflowVerdict::Pass;
}

void TestWorkflowController::observeRunState(RunState state,
                                             uint16_t electricalErrors,
                                             uint8_t progressPercent) {
    if (!snapshot_.profileValid) return;
    snapshot_.electricalErrors = electricalErrors;
    snapshot_.progressPercent = progressPercent;
    snapshot_.autoStartRequested = false;
    switch (state) {
        case RunState::Running:
            snapshot_.state = TestWorkflowState::Running;
            break;
        case RunState::Paused:
            snapshot_.state = TestWorkflowState::Paused;
            break;
        case RunState::Complete:
            snapshot_.state = TestWorkflowState::Complete;
            refreshVerdict(electricalErrors);
            if (archive_ != nullptr) {
                archive_->upsertCompleted(snapshot_, activeProfile_, activeMap_, qualityWarning_);
            }
            break;
        case RunState::Idle:
        default:
            // A freshly prepared source remains READY; a reset after an
            // existing run also returns to READY instead of losing identity.
            snapshot_.state = TestWorkflowState::Ready;
            snapshot_.verdict = TestWorkflowVerdict::None;
            snapshot_.progressPercent = 0U;
            snapshot_.electricalErrors = 0U;
            break;
    }
}

void TestWorkflowController::markQualityWarning(bool warning) {
    qualityWarning_ = warning;
    if (snapshot_.state == TestWorkflowState::Complete) {
        refreshVerdict(snapshot_.electricalErrors);
        if (archive_ != nullptr) {
            archive_->upsertCompleted(snapshot_, activeProfile_, activeMap_, qualityWarning_);
        }
    }
}

void TestWorkflowController::markBlocked(const char* reason) {
    snapshot_.state = TestWorkflowState::Blocked;
    snapshot_.profileValid = false;
    snapshot_.autoStartRequested = false;
    snapshot_.verdict = TestWorkflowVerdict::Fail;
    copyText(blockReason_, sizeof(blockReason_), reason);
}

const char* TestWorkflowController::sourceText(TestProfileSource source, bool tr) {
    switch (source) {
        case TestProfileSource::ManualNormal: return tr ? "SERBEST / NORMAL" : "FREE / NORMAL";
        case TestProfileSource::ManualSubD: return "SUB-D";
        case TestProfileSource::CustomMap: return tr ? "OZEL HARITA" : "CUSTOM MAP";
        case TestProfileSource::CableLearn: return tr ? "KABLO OGRENME" : "CABLE LEARN";
        case TestProfileSource::BarcodeMgProfile: return "MG BARCODE / QR";
        case TestProfileSource::BarcodeCustomerReference: return tr ? "MUSTERI KODU" : "CUSTOMER CODE";
        case TestProfileSource::ProductionPr: return "PR / SERVER";
        case TestProfileSource::DocumentImport: return tr ? "BELGE AKTARIM" : "DOCUMENT IMPORT";
        case TestProfileSource::ProfileCache: return tr ? "PROFIL CACHE" : "PROFILE CACHE";
        case TestProfileSource::None: default: return tr ? "YOK" : "NONE";
    }
}

const char* TestWorkflowController::stateText(TestWorkflowState state, bool tr) {
    switch (state) {
        case TestWorkflowState::Loading: return tr ? "YUKLENIYOR" : "LOADING";
        case TestWorkflowState::NeedsLookup: return tr ? "SUNUCU COZUMLEME BEKLIYOR" : "WAITING SERVER LOOKUP";
        case TestWorkflowState::Ready: return "READY";
        case TestWorkflowState::Running: return tr ? "TEST EDILIYOR" : "TESTING";
        case TestWorkflowState::Paused: return tr ? "DURAKLATILDI" : "PAUSED";
        case TestWorkflowState::Complete: return tr ? "TAMAMLANDI" : "COMPLETE";
        case TestWorkflowState::Blocked: return tr ? "BLOKE" : "BLOCKED";
        case TestWorkflowState::Empty: default: return tr ? "PROFIL YOK" : "NO PROFILE";
    }
}

const char* TestWorkflowController::verdictText(TestWorkflowVerdict verdict) {
    switch (verdict) {
        case TestWorkflowVerdict::Pass: return "PASS";
        case TestWorkflowVerdict::Warning: return "WARNING";
        case TestWorkflowVerdict::Fail: return "FAIL";
        case TestWorkflowVerdict::None: default: return "-";
    }
}

void TestWorkflowController::formatIdentity(char* output, size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) return;
    snprintf(output, outputSize, "PR=%s | REF=%s | REV=%s | PROFILE=%s",
             snapshot_.identity.productionPr[0] ? snapshot_.identity.productionPr : "-",
             snapshot_.identity.customerReference[0] ? snapshot_.identity.customerReference : "-",
             snapshot_.identity.revision[0] ? snapshot_.identity.revision : "-",
             snapshot_.identity.profileId[0] ? snapshot_.identity.profileId : "-");
}

void TestWorkflowController::formatSummary(char* output, size_t outputSize, bool turkish) const {
    if (output == nullptr || outputSize == 0U) return;
    snprintf(output, outputSize,
             "%s | %s | pins=%u | map=%s | result=%s | run=%lu/%u | START=%s | AUTO=NO",
             sourceText(snapshot_.source, turkish),
             stateText(snapshot_.state, turkish),
             static_cast<unsigned>(snapshot_.signalPinCount),
             snapshot_.customMap ? "NET" : "1:1",
             verdictText(snapshot_.verdict),
             static_cast<unsigned long>(snapshot_.generation),
             static_cast<unsigned>(snapshot_.attempt),
             startAllowed() ? "READY" : "WAIT");
}

}  // namespace mg::p4
