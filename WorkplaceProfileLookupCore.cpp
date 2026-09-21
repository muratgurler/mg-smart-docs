#include "WorkplaceProfileLookupCore.h"

#include <stdio.h>

namespace mg::p4 {

const char* WorkplaceProfileLookupCore::statusText(WorkplaceLookupStatus status) {
    switch (status) {
        case WorkplaceLookupStatus::ConfigRequired: return "CONFIG_REQUIRED";
        case WorkplaceLookupStatus::InvalidLookupCode: return "INVALID_CODE";
        case WorkplaceLookupStatus::NetworkUnavailable: return "NETWORK_UNAVAILABLE";
        case WorkplaceLookupStatus::TlsConfigurationRequired: return "TLS_CONFIG_REQUIRED";
        case WorkplaceLookupStatus::TransportError: return "TRANSPORT_ERROR";
        case WorkplaceLookupStatus::HttpError: return "HTTP_ERROR";
        case WorkplaceLookupStatus::ResponseTooLarge: return "RESPONSE_TOO_LARGE";
        case WorkplaceLookupStatus::ResponseParseError: return "PARSE_ERROR";
        case WorkplaceLookupStatus::ProfileBlocked: return "BLOCKED";
        case WorkplaceLookupStatus::Ready: return "READY";
        case WorkplaceLookupStatus::Idle: default: return "IDLE";
    }
}

WorkplaceLookupResult WorkplaceProfileLookupCore::applyResponse(
    TestWorkflowController& workflow,
    const WorkplaceProfileResponseAdapter& adapter,
    const char* responseBody,
    size_t responseLength,
    const char* expectedPr,
    const char* expectedCustomerReference,
    int httpStatus) {
    WorkplaceLookupResult out{};
    out.httpStatus = httpStatus;
    out.responseBytes = responseLength;
    out.autoStartRequested = false;

    if (httpStatus < 200 || httpStatus >= 300) {
        out.status = WorkplaceLookupStatus::HttpError;
        workflow.markBlocked("workplace HTTP response not successful");
        snprintf(out.message, sizeof(out.message), "HTTP %d", httpStatus);
        return out;
    }

    ProductionProfileRecord record{};
    char parseError[112]{};
    if (!adapter.parse(responseBody, responseLength, record, parseError, sizeof(parseError))) {
        out.status = WorkplaceLookupStatus::ResponseParseError;
        workflow.markBlocked(parseError[0] ? parseError : "workplace response parse failed");
        snprintf(out.message, sizeof(out.message), "PARSE: %.132s", parseError);
        return out;
    }

    const ProductionBridgeResult bridge = ProductionWorkflowBridge::apply(
        workflow, record, expectedPr, expectedCustomerReference);
    out.validation = bridge.validation;
    out.autoStartRequested = bridge.autoStartRequested;
    out.profileReady = bridge.ready;
    out.status = bridge.ready ? WorkplaceLookupStatus::Ready : WorkplaceLookupStatus::ProfileBlocked;
    snprintf(out.message, sizeof(out.message), "%.143s", bridge.message);
    return out;
}

}  // namespace mg::p4
