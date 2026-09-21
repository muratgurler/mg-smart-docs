#include <cassert>
#include <cstring>
#include <iostream>

#include "../TestWorkflowController.h"

using namespace mg::p4;

int main() {
    TestWorkflowController workflow;
    assert(workflow.snapshot().state == TestWorkflowState::Empty);
    assert(!workflow.snapshot().autoStartRequested);
    assert(!workflow.startAllowed());

    CableProfile normal = makeNormalProfile(25U, true, true);
    assert(workflow.prepareManual(normal, false, nullptr));
    assert(workflow.snapshot().state == TestWorkflowState::Ready);
    assert(workflow.snapshot().source == TestProfileSource::ManualNormal);
    assert(workflow.startAllowed());
    assert(workflow.markStartRequest());
    assert(!workflow.snapshot().autoStartRequested);

    workflow.observeRunState(RunState::Running, 0U, 12U);
    assert(workflow.snapshot().state == TestWorkflowState::Running);
    workflow.observeRunState(RunState::Paused, 0U, 35U);
    assert(workflow.snapshot().state == TestWorkflowState::Paused);
    workflow.observeRunState(RunState::Complete, 0U, 100U);
    assert(workflow.snapshot().verdict == TestWorkflowVerdict::Pass);

    const uint32_t beforeBarcode = workflow.snapshot().generation;
    workflow.acceptBarcodeClassification("PR123456", true, false, false, true,
                                         "DEMO-DB25", "R07");
    assert(workflow.snapshot().generation > beforeBarcode);
    assert(workflow.snapshot().state == TestWorkflowState::NeedsLookup);
    assert(!workflow.snapshot().profileValid);
    assert(!workflow.startAllowed());
    assert(std::strcmp(workflow.snapshot().identity.productionPr, "PR123456") == 0);

    CableMap prMap = makeOneToOneCableMap();
    assert(workflow.acceptNetworkResolvedProfile("PR123456", "ASML-REF-DEMO-01", "R07",
                                                 "DEMO-DB25", normal, &prMap));
    assert(workflow.snapshot().state == TestWorkflowState::Ready);
    assert(workflow.snapshot().source == TestProfileSource::ProductionPr);
    assert(std::strcmp(workflow.snapshot().identity.customerReference, "ASML-REF-DEMO-01") == 0);
    assert(!workflow.snapshot().autoStartRequested);

    workflow.observeRunState(RunState::Complete, 2U, 100U);
    assert(workflow.snapshot().verdict == TestWorkflowVerdict::Fail);
    assert(workflow.snapshot().electricalErrors == 2U);

    workflow.acceptBarcodeClassification("MG:PROFILE:FREE-12", false, false, true, true,
                                         "MG-FREE-12", "LOCAL");
    assert(workflow.snapshot().state == TestWorkflowState::Ready);
    assert(workflow.snapshot().source == TestProfileSource::BarcodeMgProfile);
    assert(workflow.profile().signalPinCount == 12U);
    assert(workflow.startAllowed());

    CableMap learned = makeOneToOneCableMap();
    learned.setSameSideConnection(CableMapSide::A, 7U, 9U, true);
    learned.connectAtoB(7U, 12U, true);
    CableProfile learnedProfile = makeNormalProfile(kMaximumSignalPins, true, true);
    assert(workflow.prepareCableLearn(learnedProfile, learned));
    assert(workflow.snapshot().source == TestProfileSource::CableLearn);
    assert(workflow.snapshot().customMap);
    assert((workflow.map().sameSideMaskA(7U) & (1ULL << 9U)) != 0ULL);
    assert((workflow.map().receiverMaskAtoB(7U) & (1ULL << 12U)) != 0ULL);

    CableMap documentMap = makeOneToOneCableMap();
    documentMap.setAtoBMask(2U, (1ULL << 3U) | (1ULL << 4U));
    CableProfile docProfile = makeNormalProfile(30U, true, true);
    docProfile.name = "DOCUMENT PROFILE TEMP";
    assert(workflow.prepareDocument(docProfile, documentMap, "DOC-123"));
    assert(workflow.snapshot().source == TestProfileSource::DocumentImport);
    assert(std::strcmp(workflow.profile().name, "DOCUMENT PROFILE TEMP") == 0);
    assert(std::strcmp(workflow.snapshot().identity.profileId, "DOC-123") == 0);
    assert(workflow.map().receiverMaskAtoB(2U) == ((1ULL << 3U) | (1ULL << 4U)));

    workflow.acceptBarcodeClassification("UNKNOWN-4711", false, false, false, false,
                                         "", "");
    assert(workflow.snapshot().state == TestWorkflowState::Blocked);
    assert(!workflow.startAllowed());
    assert(!workflow.snapshot().autoStartRequested);

    char summary[256]{};
    workflow.formatSummary(summary, sizeof(summary), false);
    assert(std::strstr(summary, "AUTO=NO") != nullptr);

    std::cout << "workflow_controller_batch7_test PASS\n";
    return 0;
}
