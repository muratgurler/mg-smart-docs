#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace mg::p4 {

enum class WorkplaceAuthMode : uint8_t {
    None = 0,
    Basic = 1,
    Bearer = 2,
    CustomHeader = 3
};

enum class WorkplaceResponseFormat : uint8_t {
    CanonicalJsonV1 = 0,
    MappedJsonV1 = 1
};

struct WorkplaceJsonFieldMap {
    String productionPr = "productionPr";
    String customerReference = "customerReference";
    String revision = "revision";
    String profileId = "profileId";
    String signalPinCount = "signalPinCount";
    String includePeA = "includePeA";
    String includePeB = "includePeB";
    String includeDrainShieldA = "includeDrainShieldA";
    String includeDrainShieldB = "includeDrainShieldB";
    String connectorA = "connectorA";
    String connectorB = "connectorB";
    String nets = "nets";
    String netA = "A";
    String netB = "B";
    String connectorKind = "kind";
    String connectorGender = "gender";
    String connectorContacts = "contacts";
    String connectorName = "name";
};

struct WorkplaceServerConfig {
    String prEndpointTemplate;
    WorkplaceAuthMode authMode = WorkplaceAuthMode::None;
    String authUser;
    String authSecret;
    String customHeaderName = "X-API-Key";
    String tlsCaPem;
    WorkplaceResponseFormat responseFormat = WorkplaceResponseFormat::CanonicalJsonV1;
    WorkplaceJsonFieldMap fieldMap{};
    uint32_t timeoutMs = 5000U;
    size_t maxResponseBytes = 32768U;
};

struct WorkplaceServerConfigValidation {
    bool ready = false;
    bool endpointValid = false;
    bool authValid = false;
    bool tlsValid = false;
    bool responseValid = false;
    char message[128] = {};
};

WorkplaceServerConfig defaultWorkplaceServerConfig();
WorkplaceServerConfigValidation validateWorkplaceServerConfig(const WorkplaceServerConfig& config);
const char* workplaceAuthModeName(WorkplaceAuthMode mode);
const char* workplaceResponseFormatName(WorkplaceResponseFormat format);
String workplaceFieldMapSpec(const WorkplaceJsonFieldMap& map);
bool parseWorkplaceFieldMapSpec(const String& spec, WorkplaceJsonFieldMap& map, String& error);

}  // namespace mg::p4
