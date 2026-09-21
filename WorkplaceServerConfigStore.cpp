#include "WorkplaceServerConfigStore.h"

#include <Preferences.h>
#include <stdio.h>

namespace mg::p4 {
namespace {
constexpr const char* kNs = "mg-workplace";
}

void WorkplaceServerConfigStore::setStatus(const char* text) {
    snprintf(statusText_, sizeof(statusText_), "%s", text != nullptr ? text : "");
}

bool WorkplaceServerConfigStore::load(WorkplaceServerConfig& config) {
    Preferences prefs;
    if (!prefs.begin(kNs, true)) {
        setStatus("NVS open failed");
        return false;
    }
    hasSavedConfig_ = prefs.getBool("valid", false);
    if (!hasSavedConfig_) {
        prefs.end();
        setStatus("No saved workplace config");
        return true;
    }

    config.prEndpointTemplate = prefs.getString("pr_url", config.prEndpointTemplate);
    config.authMode = static_cast<WorkplaceAuthMode>(prefs.getUChar("auth_m", static_cast<uint8_t>(config.authMode)));
    config.authUser = prefs.getString("auth_u", "");
    config.authSecret = prefs.getString("auth_s", "");
    config.customHeaderName = prefs.getString("auth_h", "X-API-Key");
    config.tlsCaPem = prefs.getString("tls_ca", config.tlsCaPem);
    config.responseFormat = static_cast<WorkplaceResponseFormat>(prefs.getUChar("resp_f", static_cast<uint8_t>(config.responseFormat)));
    const String mapSpec = prefs.getString("fieldmap", workplaceFieldMapSpec(config.fieldMap));
    config.timeoutMs = prefs.getUInt("timeout", config.timeoutMs);
    config.maxResponseBytes = static_cast<size_t>(prefs.getUInt("maxresp", static_cast<uint32_t>(config.maxResponseBytes)));
    prefs.end();

    String error;
    if (!parseWorkplaceFieldMapSpec(mapSpec, config.fieldMap, error)) {
        setStatus("Saved field map invalid; canonical defaults used");
        config.fieldMap = WorkplaceJsonFieldMap{};
        return false;
    }
    setStatus("Workplace config loaded from NVS");
    return true;
}

bool WorkplaceServerConfigStore::save(const WorkplaceServerConfig& config) {
    Preferences prefs;
    if (!prefs.begin(kNs, false)) {
        setStatus("NVS open failed");
        return false;
    }
    bool ok = true;
    ok &= prefs.putString("pr_url", config.prEndpointTemplate) > 0U || config.prEndpointTemplate.isEmpty();
    ok &= prefs.putUChar("auth_m", static_cast<uint8_t>(config.authMode)) == 1U;
    ok &= prefs.putString("auth_u", config.authUser) > 0U || config.authUser.isEmpty();
    ok &= prefs.putString("auth_s", config.authSecret) > 0U || config.authSecret.isEmpty();
    ok &= prefs.putString("auth_h", config.customHeaderName) > 0U || config.customHeaderName.isEmpty();
    ok &= prefs.putString("tls_ca", config.tlsCaPem) > 0U || config.tlsCaPem.isEmpty();
    ok &= prefs.putUChar("resp_f", static_cast<uint8_t>(config.responseFormat)) == 1U;
    const String fieldMap = workplaceFieldMapSpec(config.fieldMap);
    ok &= prefs.putString("fieldmap", fieldMap) > 0U;
    ok &= prefs.putUInt("timeout", config.timeoutMs) == 4U;
    ok &= prefs.putUInt("maxresp", static_cast<uint32_t>(config.maxResponseBytes)) == 4U;
    if (ok) ok &= prefs.putBool("valid", true) == 1U;
    prefs.end();
    hasSavedConfig_ = ok;
    setStatus(ok ? "Workplace config saved" : "Workplace config save failed");
    return ok;
}

bool WorkplaceServerConfigStore::clear() {
    Preferences prefs;
    if (!prefs.begin(kNs, false)) {
        setStatus("NVS open failed");
        return false;
    }
    const bool ok = prefs.clear();
    prefs.end();
    hasSavedConfig_ = false;
    setStatus(ok ? "Workplace config cleared" : "Workplace config clear failed");
    return ok;
}

}  // namespace mg::p4
