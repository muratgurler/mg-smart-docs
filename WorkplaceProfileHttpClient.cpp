#include "WorkplaceProfileHttpClient.h"

#include <HTTPClient.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>

namespace mg::p4 {
namespace {

String base64Encode(const String& input) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String out;
    const size_t n = input.length();
    out.reserve(((n + 2U) / 3U) * 4U);
    for (size_t i = 0; i < n; i += 3U) {
        const uint32_t b0 = static_cast<uint8_t>(input[i]);
        const uint32_t b1 = (i + 1U < n) ? static_cast<uint8_t>(input[i + 1U]) : 0U;
        const uint32_t b2 = (i + 2U < n) ? static_cast<uint8_t>(input[i + 2U]) : 0U;
        const uint32_t v = (b0 << 16U) | (b1 << 8U) | b2;
        out += table[(v >> 18U) & 0x3FU];
        out += table[(v >> 12U) & 0x3FU];
        out += (i + 1U < n) ? table[(v >> 6U) & 0x3FU] : '=';
        out += (i + 2U < n) ? table[v & 0x3FU] : '=';
    }
    return out;
}

bool authConfigValid(const WorkplaceHttpConfig& config) {
    switch (config.authMode) {
        case WorkplaceAuthMode::None: return true;
        case WorkplaceAuthMode::Basic:
            return !config.authUser.isEmpty() && !config.authSecret.isEmpty() &&
                   config.authUser.indexOf(':') < 0;
        case WorkplaceAuthMode::Bearer:
            return !config.authSecret.isEmpty() &&
                   config.authSecret.indexOf('\r') < 0 && config.authSecret.indexOf('\n') < 0;
        case WorkplaceAuthMode::CustomHeader:
            return !config.customHeaderName.isEmpty() && !config.authSecret.isEmpty() &&
                   config.customHeaderName.indexOf(':') < 0 &&
                   config.customHeaderName.indexOf('\r') < 0 &&
                   config.customHeaderName.indexOf('\n') < 0 &&
                   config.authSecret.indexOf('\r') < 0 && config.authSecret.indexOf('\n') < 0;
    }
    return false;
}

}  // namespace


WorkplaceProfileHttpClient::WorkplaceProfileHttpClient(
    MgNetworkManager& network,
    const WorkplaceProfileResponseAdapter& adapter)
    : network_(network), adapter_(adapter) {}

bool WorkplaceProfileHttpClient::configuredForPr() const {
    return !config_.prEndpointTemplate.isEmpty() &&
           config_.prEndpointTemplate.indexOf("{PR}") >= 0 &&
           authConfigValid(config_) && config_.responseConfigurationValid;
}

String WorkplaceProfileHttpClient::percentEncode(const char* text) {
    String out;
    if (text == nullptr) return out;
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        const unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[(c >> 4U) & 0x0FU];
            out += hex[c & 0x0FU];
        }
    }
    return out;
}

bool WorkplaceProfileHttpClient::buildPrUrl(const String& endpointTemplate,
                                            const char* productionPr,
                                            String& url) {
    if (productionPr == nullptr || productionPr[0] == '\0') return false;
    const int pos = endpointTemplate.indexOf("{PR}");
    if (pos < 0) return false;
    url = endpointTemplate;
    url.replace("{PR}", percentEncode(productionPr));
    return url.startsWith("http://") || url.startsWith("https://");
}

WorkplaceLookupResult WorkplaceProfileHttpClient::fail(
    WorkplaceLookupStatus status,
    TestWorkflowController& workflow,
    const char* workflowReason,
    const char* message,
    int httpStatus) const {
    WorkplaceLookupResult out{};
    out.status = status;
    out.httpStatus = httpStatus;
    out.autoStartRequested = false;
    snprintf(out.message, sizeof(out.message), "%s", message != nullptr ? message : "lookup failed");
    workflow.markBlocked(workflowReason != nullptr ? workflowReason : out.message);
    return out;
}

WorkplaceLookupResult WorkplaceProfileHttpClient::lookupPr(
    const char* productionPr,
    TestWorkflowController& workflow) {
    if (productionPr == nullptr || productionPr[0] == '\0') {
        return fail(WorkplaceLookupStatus::InvalidLookupCode, workflow,
                    "empty workplace PR", "empty PR");
    }
    if (!configuredForPr()) {
        return fail(WorkplaceLookupStatus::ConfigRequired, workflow,
                    "workplace PR endpoint is not configured", "CONFIG_REQUIRED: PR endpoint");
    }
    if (network_.documentApActive() ||
        (!network_.ethernetHasIp() && !network_.wifiConnected())) {
        return fail(WorkplaceLookupStatus::NetworkUnavailable, workflow,
                    "company network unavailable", "NETWORK_UNAVAILABLE");
    }

    String url;
    if (!buildPrUrl(config_.prEndpointTemplate, productionPr, url)) {
        return fail(WorkplaceLookupStatus::ConfigRequired, workflow,
                    "invalid workplace PR endpoint template", "CONFIG_REQUIRED: invalid endpoint");
    }

    HTTPClient http;
    http.setConnectTimeout(config_.timeoutMs);
    http.setTimeout(config_.timeoutMs);
    http.setUserAgent("MG-Smart-Tester/3.3");

    bool begun = false;
    NetworkClient plainClient;
    NetworkClientSecure secureClient;
    if (url.startsWith("https://")) {
        if (config_.tlsCaPem.isEmpty()) {
            return fail(WorkplaceLookupStatus::TlsConfigurationRequired, workflow,
                        "HTTPS workplace endpoint has no CA certificate",
                        "TLS_CONFIG_REQUIRED: CA certificate");
        }
        secureClient.setCACert(config_.tlsCaPem.c_str());
        begun = http.begin(secureClient, url);
    } else {
        begun = http.begin(plainClient, url);
    }
    if (!begun) {
        return fail(WorkplaceLookupStatus::TransportError, workflow,
                    "workplace HTTP client could not start", "TRANSPORT_ERROR: begin");
    }
    http.addHeader("Accept", "application/json");
    if (config_.authMode == WorkplaceAuthMode::Basic) {
        const String encoded = base64Encode(config_.authUser + ":" + config_.authSecret);
        http.addHeader("Authorization", String("Basic ") + encoded);
    } else if (config_.authMode == WorkplaceAuthMode::Bearer) {
        http.addHeader("Authorization", String("Bearer ") + config_.authSecret);
    } else if (config_.authMode == WorkplaceAuthMode::CustomHeader) {
        http.addHeader(config_.customHeaderName, config_.authSecret);
    }

    const int httpStatus = http.GET();
    if (httpStatus <= 0) {
        const String reason = HTTPClient::errorToString(httpStatus);
        http.end();
        return fail(WorkplaceLookupStatus::TransportError, workflow,
                    "workplace HTTP GET failed", reason.c_str(), httpStatus);
    }
    if (httpStatus < 200 || httpStatus >= 300) {
        char message[64]{};
        snprintf(message, sizeof(message), "HTTP %d", httpStatus);
        http.end();
        return fail(WorkplaceLookupStatus::HttpError, workflow,
                    "workplace HTTP response not successful", message, httpStatus);
    }

    const int declaredLength = http.getSize();
    if (declaredLength > 0 && static_cast<size_t>(declaredLength) > config_.maxResponseBytes) {
        http.end();
        return fail(WorkplaceLookupStatus::ResponseTooLarge, workflow,
                    "workplace response exceeds configured size limit",
                    "RESPONSE_TOO_LARGE", httpStatus);
    }

    const String body = http.getString();
    http.end();
    if (body.length() > config_.maxResponseBytes) {
        return fail(WorkplaceLookupStatus::ResponseTooLarge, workflow,
                    "workplace response exceeds configured size limit",
                    "RESPONSE_TOO_LARGE", httpStatus);
    }

    WorkplaceLookupResult result = WorkplaceProfileLookupCore::applyResponse(
        workflow, adapter_, body.c_str(), body.length(), productionPr, nullptr, httpStatus);
    result.responseBytes = body.length();
    result.autoStartRequested = false;
    return result;
}

}  // namespace mg::p4
