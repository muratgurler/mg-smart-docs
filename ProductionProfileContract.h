#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CableMap.h"
#include "CableProfile.h"

namespace mg::p4 {

// Transport-neutral server/profile record. The company-specific HTTP adapter
// fills this structure; validation and workflow logic stay independent of the
// workplace API response format.
struct ProductionProfileRecord {
    char productionPr[24] = {};
    char customerReference[40] = {};
    char revision[20] = {};
    char profileId[40] = {};

    uint8_t signalPinCount = 0U;
    bool includePeA = false;
    bool includePeB = false;
    bool includeDrainShieldA = false;
    bool includeDrainShieldB = false;

    ConnectorKind connectorKindA = ConnectorKind::Custom;
    ConnectorKind connectorKindB = ConnectorKind::Custom;
    ConnectorGender connectorGenderA = ConnectorGender::Neutral;
    ConnectorGender connectorGenderB = ConnectorGender::Neutral;
    uint8_t connectorContactsA = 0U;
    uint8_t connectorContactsB = 0U;
    char connectorAName[40] = {}; // X1 -> A by default
    char connectorBName[40] = {}; // X2 -> B by default

    // Full 64-point electrical topology. Index 0=PE, 1..62=signals, 63=dS.
    // Zero A->B mask is valid and represents NC/SPARE on that point.
    uint64_t aToB[kTestPointsPerSide] = {};
    uint64_t sameSideA[kTestPointsPerSide] = {};
    uint64_t sameSideB[kTestPointsPerSide] = {};
};

enum class ProductionProfileError : uint8_t {
    None = 0,
    MissingProductionPr,
    InvalidProductionPr,
    ProductionPrMismatch,
    MissingCustomerReference,
    CustomerReferenceMismatch,
    MissingRevision,
    MissingProfileId,
    SignalPinCountOutOfRange,
    ConnectorContactCountOutOfRange,
    DisabledSourceHasConnections,
    DisabledTargetReferenced,
    SameSideSelfReference,
    SameSideAsymmetric,
};

struct ProductionProfileValidation {
    bool valid = false;
    ProductionProfileError error = ProductionProfileError::None;
    uint8_t point = 0U;
    uint8_t peer = 0U;
    char message[120] = {};
};

class ProductionProfileContract {
public:
    // expectedPr and expectedCustomerReference are optional lookup-origin
    // constraints. Supplying either makes the returned record cross-check that
    // identity. This prevents a scanned code from silently resolving to a
    // different production item.
    static ProductionProfileValidation validate(
        const ProductionProfileRecord& record,
        const char* expectedPr = nullptr,
        const char* expectedCustomerReference = nullptr);

    // Converts a validated record into the runtime CableProfile + CableMap.
    // profile.name and connector display-name pointers refer to record storage;
    // TestWorkflowController immediately deep-copies them on acceptance.
    static bool buildRuntimeProfile(const ProductionProfileRecord& record,
                                    CableProfile& profile,
                                    CableMap& map,
                                    ProductionProfileValidation* validation = nullptr);

    static bool pointEnabled(const ProductionProfileRecord& record,
                             bool sideA,
                             uint8_t index);
    static const char* errorText(ProductionProfileError error);
};

}  // namespace mg::p4
