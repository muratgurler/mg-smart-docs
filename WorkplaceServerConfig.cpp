#include "WorkplaceServerConfig.h"

#include <stdio.h>

#include "ConfigP4.h"

namespace mg::p4 {
namespace {

bool validMapToken(const String& value) {
    if (value.isEmpty() || value.length() > 48U) return false;
    return value.indexOf('"') < 0 && value.indexOf('\\') < 0 &&
           value.indexOf(';') < 0 && value.indexOf('=') < 0;
}

void assignMapEntry(WorkplaceJsonFieldMap& map, const String& name, const String& value, bool& known) {
    known = true;
    if (name == "pr") map.productionPr = value;
    else if (name == "customer") map.customerReference = value;
    else if (name == "rev") map.revision = value;
    else if (name == "profile") map.profileId = value;
    else if (name == "pins") map.signalPinCount = value;
    else if (name == "peA") map.includePeA = value;
    else if (name == "peB") map.includePeB = value;
    else if (name == "dsA") map.includeDrainShieldA = value;
    else if (name == "dsB") map.includeDrainShieldB = value;
    else if (name == "connA") map.connectorA = value;
    else if (name == "connB") map.connectorB = value;
    else if (name == "nets") map.nets = value;
    else if (name == "A") map.netA = value;
    else if (name == "B") map.netB = value;
    else if (name == "kind") map.connectorKind = value;
    else if (name == "gender") map.connectorGender = value;
    else if (name == "contacts") map.connectorContacts = value;
    else if (name == "name") map.connectorName = value;
    else known = false;
}

bool validFieldMap(const WorkplaceJsonFieldMap& m) {
    const String* fields[] = {
        &m.productionPr, &m.customerReference, &m.revision, &m.profileId,
        &m.signalPinCount, &m.includePeA, &m.includePeB,
        &m.includeDrainShieldA, &m.includeDrainShieldB, &m.connectorA,
        &m.connectorB, &m.nets, &m.netA, &m.netB, &m.connectorKind,
        &m.connectorGender, &m.connectorContacts, &m.connectorName
    };
    const size_t count = sizeof(fields) / sizeof(fields[0]);
    for (size_t i = 0U; i < count; ++i) {
        if (!validMapToken(*fields[i])) return false;
        for (size_t j = i + 1U; j < count; ++j) {
            if (*fields[i] == *fields[j]) return false;
        }
    }
    return true;
}

}  // namespace

WorkplaceServerConfig defaultWorkplaceServerConfig() {
    WorkplaceServerConfig config{};
    config.prEndpointTemplate = kWorkplacePrEndpointTemplate;
    config.tlsCaPem = kWorkplaceTlsCaPem;
    config.timeoutMs = kWorkplaceHttpTimeoutMs;
    config.maxResponseBytes = kWorkplaceMaxResponseBytes;
    return config;
}

const char* workplaceAuthModeName(WorkplaceAuthMode mode) {
    switch (mode) {
        case WorkplaceAuthMode::Basic: return "BASIC";
        case WorkplaceAuthMode::Bearer: return "BEARER";
        case WorkplaceAuthMode::CustomHeader: return "HEADER";
        case WorkplaceAuthMode::None:
        default: return "NONE";
    }
}

const char* workplaceResponseFormatName(WorkplaceResponseFormat format) {
    return format == WorkplaceResponseFormat::MappedJsonV1
        ? "MAPPED_JSON_V1" : "CANONICAL_JSON_V1";
}

WorkplaceServerConfigValidation validateWorkplaceServerConfig(const WorkplaceServerConfig& config) {
    WorkplaceServerConfigValidation out{};
    out.endpointValid = !config.prEndpointTemplate.isEmpty() &&
        config.prEndpointTemplate.length() <= 512U &&
        config.prEndpointTemplate.indexOf("{PR}") >= 0 &&
        (config.prEndpointTemplate.startsWith("http://") ||
         config.prEndpointTemplate.startsWith("https://"));

    switch (config.authMode) {
        case WorkplaceAuthMode::None:
            out.authValid = true;
            break;
        case WorkplaceAuthMode::Basic:
            out.authValid = !config.authUser.isEmpty() && !config.authSecret.isEmpty() &&
                            config.authUser.length() <= 128U && config.authSecret.length() <= 512U &&
                            config.authUser.indexOf(':') < 0;
            break;
        case WorkplaceAuthMode::Bearer:
            out.authValid = !config.authSecret.isEmpty() && config.authSecret.length() <= 512U &&
                            config.authSecret.indexOf('\r') < 0 && config.authSecret.indexOf('\n') < 0;
            break;
        case WorkplaceAuthMode::CustomHeader:
            out.authValid = !config.customHeaderName.isEmpty() && !config.authSecret.isEmpty() &&
                            config.customHeaderName.length() <= 64U && config.authSecret.length() <= 512U &&
                            config.customHeaderName.indexOf(':') < 0 &&
                            config.customHeaderName.indexOf('\r') < 0 &&
                            config.customHeaderName.indexOf('\n') < 0 &&
                            config.authSecret.indexOf('\r') < 0 && config.authSecret.indexOf('\n') < 0;
            break;
    }

    out.tlsValid = !config.prEndpointTemplate.startsWith("https://") ||
                   (!config.tlsCaPem.isEmpty() && config.tlsCaPem.length() <= 3500U);
    out.responseValid = config.responseFormat == WorkplaceResponseFormat::CanonicalJsonV1 ||
                        (config.responseFormat == WorkplaceResponseFormat::MappedJsonV1 &&
                         validFieldMap(config.fieldMap));
    out.ready = out.endpointValid && out.authValid && out.tlsValid && out.responseValid;

    const char* message = "READY";
    if (!out.endpointValid) message = "URL must be http(s) and contain {PR}";
    else if (!out.authValid) message = "Authentication fields are incomplete";
    else if (!out.tlsValid) message = "HTTPS requires CA PEM";
    else if (!out.responseValid) message = "Response format / field map is invalid";
    snprintf(out.message, sizeof(out.message), "%s", message);
    return out;
}

String workplaceFieldMapSpec(const WorkplaceJsonFieldMap& m) {
    String out;
    out.reserve(420);
    out += "pr="; out += m.productionPr;
    out += ";customer="; out += m.customerReference;
    out += ";rev="; out += m.revision;
    out += ";profile="; out += m.profileId;
    out += ";pins="; out += m.signalPinCount;
    out += ";peA="; out += m.includePeA;
    out += ";peB="; out += m.includePeB;
    out += ";dsA="; out += m.includeDrainShieldA;
    out += ";dsB="; out += m.includeDrainShieldB;
    out += ";connA="; out += m.connectorA;
    out += ";connB="; out += m.connectorB;
    out += ";nets="; out += m.nets;
    out += ";A="; out += m.netA;
    out += ";B="; out += m.netB;
    out += ";kind="; out += m.connectorKind;
    out += ";gender="; out += m.connectorGender;
    out += ";contacts="; out += m.connectorContacts;
    out += ";name="; out += m.connectorName;
    return out;
}

bool parseWorkplaceFieldMapSpec(const String& spec, WorkplaceJsonFieldMap& map, String& error) {
    error = String();
    WorkplaceJsonFieldMap parsed{};
    if (spec.isEmpty()) {
        error = "field map is empty";
        return false;
    }
    int start = 0;
    while (start < static_cast<int>(spec.length())) {
        int end = spec.indexOf(';', start);
        if (end < 0) end = static_cast<int>(spec.length());
        String pair = spec.substring(start, end);
        pair.trim();
        if (!pair.isEmpty()) {
            const int eq = pair.indexOf('=');
            if (eq <= 0 || eq + 1 >= static_cast<int>(pair.length())) {
                error = String("bad field map entry: ") + pair;
                return false;
            }
            String name = pair.substring(0, eq);
            String value = pair.substring(eq + 1);
            name.trim(); value.trim();
            if (!validMapToken(value)) {
                error = String("invalid field name: ") + value;
                return false;
            }
            bool known = false;
            assignMapEntry(parsed, name, value, known);
            if (!known) {
                error = String("unknown field map key: ") + name;
                return false;
            }
        }
        start = end + 1;
    }
    if (!validFieldMap(parsed)) {
        error = "field map incomplete";
        return false;
    }
    map = parsed;
    return true;
}

}  // namespace mg::p4
