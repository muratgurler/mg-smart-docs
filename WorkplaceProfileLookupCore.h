#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ProductionWorkflowBridge.h"
#include "WorkplaceProfileJsonAdapter.h"

namespace mg::p4 {

enum class WorkplaceLookupStatus : uint8_t {
    Idle = 0,
    ConfigRequired,
    InvalidLookupCode,
    NetworkUnavailable,
    TlsConfigurationRequired,
    TransportError,
    HttpError,
    ResponseTooLarge,
    ResponseParseError,
    ProfileBlocked,
    Ready,
};

struct WorkplaceLookupResult {
    WorkplaceLookupStatus status = WorkplaceLookupStatus::Idle;
    int httpStatus = 0;
    bool profileReady = false;
    bool autoStartRequested = false;
    size_t responseBytes = 0U;
    ProductionProfileValidation validation{};
    char message[144] = {};
};

class WorkplaceProfileLookupCore {
public:
    static WorkplaceLookupResult applyResponse(
        TestWorkflowController& workflow,
        const WorkplaceProfileResponseAdapter& adapter,
        const char* responseBody,
        size_t responseLength,
        const char* expectedPr,
        const char* expectedCustomerReference = nullptr,
        int httpStatus = 200);

    static const char* statusText(WorkplaceLookupStatus status);
};

}  // namespace mg::p4
