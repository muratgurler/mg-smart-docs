#pragma once

#include <Arduino.h>

#include "MgNetworkManager.h"
#include "WorkplaceProfileLookupCore.h"
#include "WorkplaceServerConfig.h"

namespace mg::p4 {

struct WorkplaceHttpConfig {
    // Must contain literal {PR}; intentionally empty by default because the
    // company URL/path is site-specific and must never be guessed.
    String prEndpointTemplate;
    // Optional CA PEM for HTTPS. HTTPS without a CA is rejected; the firmware
    // never silently disables certificate verification.
    String tlsCaPem;
    uint32_t timeoutMs = 5000U;
    size_t maxResponseBytes = 32768U;
    WorkplaceAuthMode authMode = WorkplaceAuthMode::None;
    String authUser;
    String authSecret;
    String customHeaderName = "X-API-Key";
    bool responseConfigurationValid = true;
};

class WorkplaceProfileHttpClient {
public:
    explicit WorkplaceProfileHttpClient(MgNetworkManager& network,
                                        const WorkplaceProfileResponseAdapter& adapter);

    void configure(const WorkplaceHttpConfig& config) { config_ = config; }
    const WorkplaceHttpConfig& config() const { return config_; }
    bool configuredForPr() const;

    WorkplaceLookupResult lookupPr(const char* productionPr,
                                   TestWorkflowController& workflow);

    static bool buildPrUrl(const String& endpointTemplate,
                           const char* productionPr,
                           String& url);

private:
    static String percentEncode(const char* text);
    WorkplaceLookupResult fail(WorkplaceLookupStatus status,
                               TestWorkflowController& workflow,
                               const char* workflowReason,
                               const char* message,
                               int httpStatus = 0) const;

    MgNetworkManager& network_;
    const WorkplaceProfileResponseAdapter& adapter_;
    WorkplaceHttpConfig config_{};
};

}  // namespace mg::p4
