#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "WorkplaceServerConfig.h"
#include "ConfigurableWorkplaceJsonAdapter.h"

using namespace mg::p4;

int main() {
    WorkplaceServerConfig c = defaultWorkplaceServerConfig();
    auto v = validateWorkplaceServerConfig(c);
    assert(!v.ready);
    assert(!v.endpointValid);

    c.prEndpointTemplate = "http://workplace.local/api/profile/{PR}";
    v = validateWorkplaceServerConfig(c);
    assert(v.ready);

    c.prEndpointTemplate = "https://workplace.local/api/profile/{PR}";
    v = validateWorkplaceServerConfig(c);
    assert(!v.ready && !v.tlsValid);
    c.tlsCaPem = "-----BEGIN CERTIFICATE-----\nTEST\n-----END CERTIFICATE-----";
    v = validateWorkplaceServerConfig(c);
    assert(v.ready);

    c.authMode = WorkplaceAuthMode::Basic;
    c.authUser = "operator-api";
    c.authSecret = "";
    assert(!validateWorkplaceServerConfig(c).authValid);
    c.authSecret = "secret";
    assert(validateWorkplaceServerConfig(c).authValid);

    c.authMode = WorkplaceAuthMode::Bearer;
    c.authUser = "";
    assert(validateWorkplaceServerConfig(c).authValid);

    c.authMode = WorkplaceAuthMode::CustomHeader;
    c.customHeaderName = "X-API-Key";
    assert(validateWorkplaceServerConfig(c).authValid);
    c.customHeaderName = "Bad:Header";
    assert(!validateWorkplaceServerConfig(c).authValid);
    c.customHeaderName = "X-API-Key";

    WorkplaceJsonFieldMap mapped{};
    String error;
    const String spec =
        "pr=jobNo;customer=customerRef;rev=docRev;profile=profileCode;pins=pinCount;"
        "peA=peX1;peB=peX2;dsA=dsX1;dsB=dsX2;connA=x1Info;connB=x2Info;"
        "nets=connections;A=x1;B=x2;kind=type;gender=sex;contacts=count;name=label";
    assert(parseWorkplaceFieldMapSpec(spec, mapped, error));
    assert(mapped.productionPr == "jobNo");
    assert(mapped.nets == "connections");

    c.responseFormat = WorkplaceResponseFormat::MappedJsonV1;
    c.fieldMap = mapped;
    assert(validateWorkplaceServerConfig(c).responseValid);

    ConfigurableWorkplaceJsonAdapter adapter;
    adapter.configure(c.responseFormat, c.fieldMap);
    const char* json =
        "{"
        "\"jobNo\":\"PR123\","
        "\"customerRef\":\"ASML-88\","
        "\"docRev\":\"R07\","
        "\"profileCode\":\"P-88\","
        "\"pinCount\":6,"
        "\"peX1\":true,\"peX2\":true,\"dsX1\":false,\"dsX2\":false,"
        "\"x1Info\":{\"type\":\"HARTING\",\"sex\":\"MALE\",\"count\":10,\"label\":\"X1\"},"
        "\"x2Info\":{\"type\":\"HARTING\",\"sex\":\"FEMALE\",\"count\":10,\"label\":\"X2\"},"
        "\"connections\":[{\"x1\":[0],\"x2\":[0]},{\"x1\":[1],\"x2\":[1,2]}]"
        "}";
    ProductionProfileRecord record{};
    char parseError[160]{};
    assert(adapter.parse(json, strlen(json), record, parseError, sizeof(parseError)));
    assert(strcmp(record.productionPr, "PR123") == 0);
    assert(strcmp(record.customerReference, "ASML-88") == 0);
    assert(record.signalPinCount == 6);
    assert((record.aToB[1] & (1ULL << 1)) != 0ULL);
    assert((record.aToB[1] & (1ULL << 2)) != 0ULL);

    WorkplaceJsonFieldMap badMap = mapped;
    badMap.netB = badMap.netA;
    c.fieldMap = badMap;
    assert(!validateWorkplaceServerConfig(c).responseValid);

    printf("Workplace server config Fix1 tests passed.\n");
    return 0;
}
