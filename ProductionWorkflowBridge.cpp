#include "ProductionWorkflowBridge.h"

#include <stdio.h>

namespace mg::p4 {

ProductionBridgeResult ProductionWorkflowBridge::apply(
    TestWorkflowController& workflow,
    const ProductionProfileRecord& record,
    const char* expectedPr,
    const char* expectedCustomerReference) {
    ProductionBridgeResult out{};
    out.autoStartRequested = false;
    out.validation = ProductionProfileContract::validate(
        record, expectedPr, expectedCustomerReference);

    if (!out.validation.valid) {
        workflow.markBlocked(out.validation.message);
        snprintf(out.message, sizeof(out.message), "BLOCKED: %s", out.validation.message);
        return out;
    }

    CableProfile profile{};
    CableMap map{};
    ProductionProfileValidation buildValidation{};
    if (!ProductionProfileContract::buildRuntimeProfile(record, profile, map, &buildValidation)) {
        out.validation = buildValidation;
        workflow.markBlocked(buildValidation.message);
        snprintf(out.message, sizeof(out.message), "BLOCKED: %s", buildValidation.message);
        return out;
    }

    TestProfileSource source = TestProfileSource::ProductionPr;
    const char* sourceCode = record.productionPr;
    if ((expectedPr == nullptr || expectedPr[0] == '\0') &&
        expectedCustomerReference != nullptr && expectedCustomerReference[0] != '\0') {
        source = TestProfileSource::BarcodeCustomerReference;
        sourceCode = expectedCustomerReference;
    }

    out.accepted = workflow.acceptNetworkResolvedProfileForSource(
        source,
        sourceCode,
        record.productionPr,
        record.customerReference,
        record.revision,
        record.profileId,
        profile,
        &map);
    out.ready = out.accepted &&
                workflow.snapshot().state == TestWorkflowState::Ready &&
                workflow.startAllowed();
    out.autoStartRequested = workflow.snapshot().autoStartRequested;

    snprintf(out.message, sizeof(out.message),
             out.ready ? "READY: operator START required" : "profile not ready");
    return out;
}

}  // namespace mg::p4
