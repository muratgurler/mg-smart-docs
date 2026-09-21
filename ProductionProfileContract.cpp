#include "ProductionProfileContract.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace mg::p4 {
namespace {

bool emptyText(const char* text) {
    return text == nullptr || text[0] == '\0';
}

bool sameText(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    while (*a != '\0' && *b != '\0') {
        if (toupper(static_cast<unsigned char>(*a)) !=
            toupper(static_cast<unsigned char>(*b))) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

bool validPr(const char* pr) {
    if (pr == nullptr || pr[0] == '\0') return false;
    if (toupper(static_cast<unsigned char>(pr[0])) != 'P' ||
        toupper(static_cast<unsigned char>(pr[1])) != 'R') {
        return false;
    }
    if (pr[2] == '\0') return false;
    for (size_t i = 2U; pr[i] != '\0'; ++i) {
        const unsigned char c = static_cast<unsigned char>(pr[i]);
        if (!(isalnum(c) || c == '-' || c == '_' || c == '/' || c == '.')) return false;
    }
    return true;
}

ProductionProfileValidation fail(ProductionProfileError error,
                                 const char* message,
                                 uint8_t point = 0U,
                                 uint8_t peer = 0U) {
    ProductionProfileValidation out{};
    out.valid = false;
    out.error = error;
    out.point = point;
    out.peer = peer;
    snprintf(out.message, sizeof(out.message), "%s", message != nullptr ? message : "invalid production profile");
    return out;
}

uint64_t bitFor(uint8_t index) {
    return index < kTestPointsPerSide ? (1ULL << index) : 0ULL;
}

}  // namespace

const char* ProductionProfileContract::errorText(ProductionProfileError error) {
    switch (error) {
        case ProductionProfileError::MissingProductionPr: return "missing production PR";
        case ProductionProfileError::InvalidProductionPr: return "invalid production PR";
        case ProductionProfileError::ProductionPrMismatch: return "production PR mismatch";
        case ProductionProfileError::MissingCustomerReference: return "missing customer reference";
        case ProductionProfileError::CustomerReferenceMismatch: return "customer reference mismatch";
        case ProductionProfileError::MissingRevision: return "missing document revision";
        case ProductionProfileError::MissingProfileId: return "missing profile id";
        case ProductionProfileError::SignalPinCountOutOfRange: return "signal pin count out of range";
        case ProductionProfileError::ConnectorContactCountOutOfRange: return "connector contact count out of range";
        case ProductionProfileError::DisabledSourceHasConnections: return "disabled source point has connections";
        case ProductionProfileError::DisabledTargetReferenced: return "disabled target point referenced";
        case ProductionProfileError::SameSideSelfReference: return "same-side self reference";
        case ProductionProfileError::SameSideAsymmetric: return "same-side map is asymmetric";
        case ProductionProfileError::None: default: return "ok";
    }
}

bool ProductionProfileContract::pointEnabled(const ProductionProfileRecord& record,
                                             bool sideA,
                                             uint8_t index) {
    if (index >= kTestPointsPerSide) return false;
    if (index == kPeTestIndex) return sideA ? record.includePeA : record.includePeB;
    if (index == kDrainShieldTestIndex) {
        return sideA ? record.includeDrainShieldA : record.includeDrainShieldB;
    }
    return index >= kFirstSignalTestIndex && index <= record.signalPinCount;
}

ProductionProfileValidation ProductionProfileContract::validate(
    const ProductionProfileRecord& record,
    const char* expectedPr,
    const char* expectedCustomerReference) {
    if (emptyText(record.productionPr)) {
        return fail(ProductionProfileError::MissingProductionPr, errorText(ProductionProfileError::MissingProductionPr));
    }
    if (!validPr(record.productionPr)) {
        return fail(ProductionProfileError::InvalidProductionPr, errorText(ProductionProfileError::InvalidProductionPr));
    }
    if (!emptyText(expectedPr) && !sameText(record.productionPr, expectedPr)) {
        return fail(ProductionProfileError::ProductionPrMismatch, errorText(ProductionProfileError::ProductionPrMismatch));
    }
    if (emptyText(record.customerReference)) {
        return fail(ProductionProfileError::MissingCustomerReference, errorText(ProductionProfileError::MissingCustomerReference));
    }
    if (!emptyText(expectedCustomerReference) &&
        !sameText(record.customerReference, expectedCustomerReference)) {
        return fail(ProductionProfileError::CustomerReferenceMismatch,
                    errorText(ProductionProfileError::CustomerReferenceMismatch));
    }
    if (emptyText(record.revision)) {
        return fail(ProductionProfileError::MissingRevision, errorText(ProductionProfileError::MissingRevision));
    }
    if (emptyText(record.profileId)) {
        return fail(ProductionProfileError::MissingProfileId, errorText(ProductionProfileError::MissingProfileId));
    }
    if (record.signalPinCount < 1U || record.signalPinCount > kMaximumSignalPins) {
        return fail(ProductionProfileError::SignalPinCountOutOfRange,
                    errorText(ProductionProfileError::SignalPinCountOutOfRange));
    }
    if (record.connectorContactsA > kTestPointsPerSide ||
        record.connectorContactsB > kTestPointsPerSide) {
        return fail(ProductionProfileError::ConnectorContactCountOutOfRange,
                    errorText(ProductionProfileError::ConnectorContactCountOutOfRange));
    }

    for (uint8_t a = 0U; a < kTestPointsPerSide; ++a) {
        const uint64_t targets = record.aToB[a];
        if (!pointEnabled(record, true, a) && targets != 0ULL) {
            return fail(ProductionProfileError::DisabledSourceHasConnections,
                        errorText(ProductionProfileError::DisabledSourceHasConnections), a, 0U);
        }
        for (uint8_t b = 0U; b < kTestPointsPerSide; ++b) {
            if ((targets & bitFor(b)) != 0ULL && !pointEnabled(record, false, b)) {
                return fail(ProductionProfileError::DisabledTargetReferenced,
                            errorText(ProductionProfileError::DisabledTargetReferenced), a, b);
            }
        }
    }

    for (uint8_t side = 0U; side < 2U; ++side) {
        const uint64_t* same = side == 0U ? record.sameSideA : record.sameSideB;
        const bool isA = side == 0U;
        for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
            const uint64_t peers = same[i];
            if ((peers & bitFor(i)) != 0ULL) {
                return fail(ProductionProfileError::SameSideSelfReference,
                            errorText(ProductionProfileError::SameSideSelfReference), i, i);
            }
            if (!pointEnabled(record, isA, i) && peers != 0ULL) {
                return fail(ProductionProfileError::DisabledSourceHasConnections,
                            errorText(ProductionProfileError::DisabledSourceHasConnections), i, 0U);
            }
            for (uint8_t j = 0U; j < kTestPointsPerSide; ++j) {
                if ((peers & bitFor(j)) == 0ULL) continue;
                if (!pointEnabled(record, isA, j)) {
                    return fail(ProductionProfileError::DisabledTargetReferenced,
                                errorText(ProductionProfileError::DisabledTargetReferenced), i, j);
                }
                if ((same[j] & bitFor(i)) == 0ULL) {
                    return fail(ProductionProfileError::SameSideAsymmetric,
                                errorText(ProductionProfileError::SameSideAsymmetric), i, j);
                }
            }
        }
    }

    ProductionProfileValidation out{};
    out.valid = true;
    out.error = ProductionProfileError::None;
    snprintf(out.message, sizeof(out.message), "ok");
    return out;
}

bool ProductionProfileContract::buildRuntimeProfile(const ProductionProfileRecord& record,
                                                    CableProfile& profile,
                                                    CableMap& map,
                                                    ProductionProfileValidation* validation) {
    ProductionProfileValidation result = validate(record, nullptr, nullptr);
    if (validation != nullptr) *validation = result;
    if (!result.valid) return false;

    profile = makeNormalProfile(record.signalPinCount, false, false);
    profile.name = record.profileId;
    profile.kind = (record.connectorKindA == ConnectorKind::DSub &&
                    record.connectorKindB == ConnectorKind::DSub)
                       ? ScanProfileKind::SubD
                       : ScanProfileKind::Normal;
    profile.includePeA = record.includePeA;
    profile.includePeB = record.includePeB;
    profile.includeDrainShieldA = record.includeDrainShieldA;
    profile.includeDrainShieldB = record.includeDrainShieldB;
    profile.syncSpecialPointUnion();

    profile.sideA.kind = record.connectorKindA;
    profile.sideA.gender = record.connectorGenderA;
    profile.sideA.contactCount = record.connectorContactsA;
    profile.sideA.displayName = record.connectorAName[0] != '\0' ? record.connectorAName : "X1";
    profile.sideA.frontImage = nullptr;

    profile.sideB.kind = record.connectorKindB;
    profile.sideB.gender = record.connectorGenderB;
    profile.sideB.contactCount = record.connectorContactsB;
    profile.sideB.displayName = record.connectorBName[0] != '\0' ? record.connectorBName : "X2";
    profile.sideB.frontImage = nullptr;

    map.loadRaw(record.aToB, record.sameSideA, record.sameSideB, record.profileId);
    return true;
}

}  // namespace mg::p4
