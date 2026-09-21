#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ProductionProfileContract.h"

namespace mg::p4 {

// Company-response seam. HTTP transport is deliberately separate from the
// response schema so the real workplace payload can be adapted without
// touching identity validation, CableMap building or the START safety rule.
class WorkplaceProfileResponseAdapter {
public:
    virtual ~WorkplaceProfileResponseAdapter() = default;
    virtual bool parse(const char* body,
                       size_t bodyLength,
                       ProductionProfileRecord& record,
                       char* error,
                       size_t errorSize) const = 0;
};

// Canonical MG JSON accepted by the production lookup service.  The actual
// workplace adapter may either emit this shape server-side or translate the
// real server response into ProductionProfileRecord in another adapter.
//
// {
//   "productionPr":"PR123456",
//   "customerReference":"ASML-REF-...",
//   "revision":"R07",
//   "profileId":"...",
//   "signalPinCount":6,
//   "includePeA":true, "includePeB":true,
//   "includeDrainShieldA":false, "includeDrainShieldB":false,
//   "connectorA":{"kind":"HARTING","gender":"MALE","contacts":10,"name":"X1"},
//   "connectorB":{"kind":"HARTING","gender":"FEMALE","contacts":10,"name":"X2"},
//   "nets":[{"A":[0],"B":[0]}, {"A":[1],"B":[1,2]}, ...]
// }
//
// A/B arrays describe electrical connected-components, not point-to-point
// pairs. This preserves 1:N, N:1, N:N and same-side splice/common nets.
class CanonicalWorkplaceJsonAdapter final : public WorkplaceProfileResponseAdapter {
public:
    bool parse(const char* body,
               size_t bodyLength,
               ProductionProfileRecord& record,
               char* error,
               size_t errorSize) const override;
};

}  // namespace mg::p4
