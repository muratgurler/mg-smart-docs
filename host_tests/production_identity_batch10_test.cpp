#include "ProductionProfileContract.h"
#include "ProductionWorkflowBridge.h"
#include "TestWorkflowController.h"

#include <assert.h>
#include <cstdio>
#include <string.h>

using namespace mg::p4;

static ProductionProfileRecord makeRecord() {
    ProductionProfileRecord r{};
    snprintf(r.productionPr, sizeof(r.productionPr), "PR123456");
    snprintf(r.customerReference, sizeof(r.customerReference), "ASML-REF-98765");
    snprintf(r.revision, sizeof(r.revision), "R07");
    snprintf(r.profileId, sizeof(r.profileId), "ASML-HARTING-6-R07");
    r.signalPinCount = 6U;
    r.includePeA = true;
    r.includePeB = true;
    r.includeDrainShieldA = false;
    r.includeDrainShieldB = false;
    r.connectorKindA = ConnectorKind::Harting;
    r.connectorKindB = ConnectorKind::Harting;
    r.connectorGenderA = ConnectorGender::Male;
    r.connectorGenderB = ConnectorGender::Female;
    r.connectorContactsA = 10U;
    r.connectorContactsB = 10U;
    snprintf(r.connectorAName, sizeof(r.connectorAName), "X1");
    snprintf(r.connectorBName, sizeof(r.connectorBName), "X2");

    // PE one-to-one.
    r.aToB[0] = 1ULL << 0U;
    // One-to-many A1 -> B1+B2 and many-to-one A2 -> B2.
    r.aToB[1] = (1ULL << 1U) | (1ULL << 2U);
    r.aToB[2] = 1ULL << 2U;
    r.aToB[3] = 1ULL << 3U;
    r.aToB[4] = 1ULL << 4U;
    r.aToB[5] = 1ULL << 5U;
    // A6 is intentionally NC/SPARE: zero mask is valid.
    r.aToB[6] = 0ULL;

    // Legitimate same-side/common-net relation.
    r.sameSideA[3] = 1ULL << 4U;
    r.sameSideA[4] = 1ULL << 3U;
    return r;
}

int main() {
    ProductionProfileRecord record = makeRecord();

    auto v = ProductionProfileContract::validate(record, "PR123456", "ASML-REF-98765");
    assert(v.valid);

    // Workplace PR identifiers may carry a dotted order/item suffix, e.g.
    // PR1100054053.015. The mobile extractor already preserves this exact
    // format, so the device-side contract must accept the dot as well.
    ProductionProfileRecord dottedPr = makeRecord();
    snprintf(dottedPr.productionPr, sizeof(dottedPr.productionPr), "PR1100054053.015");
    v = ProductionProfileContract::validate(dottedPr, "PR1100054053.015", nullptr);
    assert(v.valid);

    CableProfile profile{};
    CableMap map{};
    assert(ProductionProfileContract::buildRuntimeProfile(record, profile, map, &v));
    assert(v.valid);
    assert(profile.signalPinCount == 6U);
    assert(profile.includePeA && profile.includePeB);
    assert(!profile.includeDrainShieldA && !profile.includeDrainShieldB);
    assert(strcmp(profile.sideA.displayName, "X1") == 0);
    assert(strcmp(profile.sideB.displayName, "X2") == 0);
    assert(map.receiverMaskAtoB(1U) == ((1ULL << 1U) | (1ULL << 2U)));
    assert(map.receiverMaskBtoA(2U) == ((1ULL << 1U) | (1ULL << 2U)));
    assert(map.receiverMaskAtoB(6U) == 0ULL); // SPARE/NC preserved
    assert(map.sameSideMaskA(3U) == (1ULL << 4U));

    // Customer barcode starts the lookup; the returned PR is stored separately
    // and the workflow source remains customer-code based for traceability.
    TestWorkflowController workflow;
    workflow.acceptBarcodeClassification("ASML-REF-98765", false, true, false,
                                         true, "", "");
    assert(workflow.snapshot().state == TestWorkflowState::NeedsLookup);
    auto bridge = ProductionWorkflowBridge::apply(workflow, record, nullptr, "ASML-REF-98765");
    assert(bridge.accepted && bridge.ready);
    assert(!bridge.autoStartRequested);
    assert(workflow.snapshot().source == TestProfileSource::BarcodeCustomerReference);
    assert(strcmp(workflow.snapshot().identity.productionPr, "PR123456") == 0);
    assert(strcmp(workflow.snapshot().identity.customerReference, "ASML-REF-98765") == 0);
    assert(strcmp(workflow.snapshot().identity.revision, "R07") == 0);
    assert(workflow.startAllowed());
    assert(!workflow.snapshot().autoStartRequested);

    // PR-origin lookup also reaches READY without auto-start.
    TestWorkflowController prWorkflow;
    prWorkflow.acceptBarcodeClassification("PR123456", true, false, false,
                                           true, "", "");
    auto prBridge = ProductionWorkflowBridge::apply(prWorkflow, record, "PR123456", nullptr);
    assert(prBridge.ready);
    assert(prWorkflow.snapshot().source == TestProfileSource::ProductionPr);
    assert(!prWorkflow.snapshot().autoStartRequested);

    // Stage03 Fix7: mobile/document profile acceptance must preserve the
    // production identity instead of replacing it with MOBILE-PROFILE.
    TestWorkflowController mobileWorkflow;
    TestWorkflowIdentity mobileIdentity{};
    snprintf(mobileIdentity.productionPr, sizeof(mobileIdentity.productionPr), "PR1100054053.015");
    snprintf(mobileIdentity.customerReference, sizeof(mobileIdentity.customerReference), "4022.677.77401");
    snprintf(mobileIdentity.revision, sizeof(mobileIdentity.revision), "14.10.18");
    snprintf(mobileIdentity.profileId, sizeof(mobileIdentity.profileId), "AUTO-SIMPLE_SUBD-15P");
    snprintf(mobileIdentity.sourceCode, sizeof(mobileIdentity.sourceCode), "MOBILE-BLE-EDITED");
    assert(mobileWorkflow.prepareDocument(profile, map, mobileIdentity));
    assert(strcmp(mobileWorkflow.snapshot().identity.productionPr, "PR1100054053.015") == 0);
    assert(strcmp(mobileWorkflow.snapshot().identity.customerReference, "4022.677.77401") == 0);
    assert(strcmp(mobileWorkflow.snapshot().identity.revision, "14.10.18") == 0);
    assert(strcmp(mobileWorkflow.snapshot().identity.profileId, "AUTO-SIMPLE_SUBD-15P") == 0);
    assert(strcmp(mobileWorkflow.snapshot().identity.sourceCode, "MOBILE-BLE-EDITED") == 0);

    // Cross-identifier mismatch must block instead of reusing a stale profile.
    TestWorkflowController mismatchWorkflow;
    mismatchWorkflow.acceptBarcodeClassification("ASML-WRONG", false, true, false,
                                                 true, "", "");
    auto mismatch = ProductionWorkflowBridge::apply(mismatchWorkflow, record, nullptr, "ASML-WRONG");
    assert(!mismatch.accepted && !mismatch.ready);
    assert(mismatch.validation.error == ProductionProfileError::CustomerReferenceMismatch);
    assert(mismatchWorkflow.snapshot().state == TestWorkflowState::Blocked);
    assert(!mismatchWorkflow.hasPreparedProfile());

    ProductionProfileRecord badTarget = makeRecord();
    badTarget.aToB[1] |= (1ULL << 20U); // outside active B1..B6/PE
    v = ProductionProfileContract::validate(badTarget, "PR123456", nullptr);
    assert(!v.valid);
    assert(v.error == ProductionProfileError::DisabledTargetReferenced);

    ProductionProfileRecord asymmetric = makeRecord();
    asymmetric.sameSideA[4] = 0ULL;
    v = ProductionProfileContract::validate(asymmetric, nullptr, nullptr);
    assert(!v.valid);
    assert(v.error == ProductionProfileError::SameSideAsymmetric);

    ProductionProfileRecord badPr = makeRecord();
    snprintf(badPr.productionPr, sizeof(badPr.productionPr), "WO123456");
    v = ProductionProfileContract::validate(badPr, nullptr, nullptr);
    assert(!v.valid);
    assert(v.error == ProductionProfileError::InvalidProductionPr);

    return 0;
}
