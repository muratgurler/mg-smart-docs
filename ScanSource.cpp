#include "ScanSource.h"

#include "CableMap.h"
#include "CableNetGraph.h"
#include "ConfigP4.h"

namespace mg::p4 {

namespace {

constexpr uint64_t bitFor(uint8_t index) {
    return index < 64U ? (1ULL << index) : 0ULL;
}

uint8_t firstSetIndex(uint64_t mask) {
    if (mask == 0ULL) {
        return 0xFF;
    }
    for (uint8_t index = 0; index < 64U; ++index) {
        if ((mask & bitFor(index)) != 0ULL) {
            return index;
        }
    }
    return 0xFF;
}

uint8_t popcount64(uint64_t value) {
#if defined(__GNUC__)
    return static_cast<uint8_t>(__builtin_popcountll(value));
#else
    uint8_t count = 0;
    while (value != 0ULL) {
        value &= value - 1ULL;
        ++count;
    }
    return count;
#endif
}

// -------------------------------------------------------------------------
// DEMO PHYSICAL CABLE MODEL
// -------------------------------------------------------------------------
// A non-null CableMap is treated as an ELECTRICAL GRAPH, not as a list of
// independent A->B rows. Same-side joins therefore propagate across the whole
// connected component. That is essential for Cable Learn, Smart Probe and
// one-to-many/common-net demonstrations.
//
// The historic UI demo faults can still be overlaid for ScanScreen regression:
//   * A8/A9 receiver routes swapped
//   * A15 open
//   * A18 high resistance
//   * A22/A23 unexpected short group
// Learning/multi-connector demos disable that overlay and observe the exact
// passive graph.

CableNet physicalNetForSource(const CableMap* map,
                              ScanDirection direction,
                              uint8_t sourceIndex) {
    CableNet net{};
    if (sourceIndex >= 64U) return net;
    if (map == nullptr) {
        if (direction == ScanDirection::AtoB) {
            net.aMask = bitFor(sourceIndex);
            net.bMask = bitFor(sourceIndex);
        } else {
            net.bMask = bitFor(sourceIndex);
            net.aMask = bitFor(sourceIndex);
        }
        return net;
    }

    CableNet nets[kTestPointsPerSide]{};
    const size_t count = CableNetGraph::build(*map, ~0ULL, ~0ULL,
                                               nets, kTestPointsPerSide);
    const uint64_t sourceBit = bitFor(sourceIndex);
    for (size_t i = 0U; i < count; ++i) {
        const bool match = direction == ScanDirection::AtoB
                               ? (nets[i].aMask & sourceBit) != 0ULL
                               : (nets[i].bMask & sourceBit) != 0ULL;
        if (match) return nets[i];
    }

    // Isolated/NC source: driving node itself is still visible on sender side,
    // but there is no receiver-side electrical continuation.
    if (direction == ScanDirection::AtoB) net.aMask = sourceBit;
    else net.bMask = sourceBit;
    return net;
}

uint64_t baseReceiverMaskAtoB(const CableMap* map, uint8_t aIndex) {
    return physicalNetForSource(map, ScanDirection::AtoB, aIndex).bMask;
}

uint64_t receiverMaskAtoB(const CableMap* map,
                          uint8_t aIndex,
                          bool faultOverlayEnabled) {
    if (aIndex >= 64U) return 0ULL;
    if (!faultOverlayEnabled) return baseReceiverMaskAtoB(map, aIndex);

    switch (aIndex) {
        case 8U:
            return baseReceiverMaskAtoB(map, 9U);
        case 9U:
            return baseReceiverMaskAtoB(map, 8U);
        case 15U:
            return 0ULL;
        case 22U:
        case 23U:
            return baseReceiverMaskAtoB(map, 22U) |
                   baseReceiverMaskAtoB(map, 23U);
        default:
            return baseReceiverMaskAtoB(map, aIndex);
    }
}

uint64_t receiverMaskBtoA(const CableMap* map,
                          uint8_t bIndex,
                          bool faultOverlayEnabled) {
    if (!faultOverlayEnabled) {
        return physicalNetForSource(map, ScanDirection::BtoA, bIndex).aMask;
    }

    // Reverse the ACTUAL A->B physical demo observation so wrong mappings are
    // always direction-neutral and never independently scripted.
    uint64_t reverseMask = 0ULL;
    for (uint8_t aIndex = 0; aIndex < 64U; ++aIndex) {
        if ((receiverMaskAtoB(map, aIndex, true) & bitFor(bIndex)) != 0ULL) {
            reverseMask |= bitFor(aIndex);
        }
    }
    return reverseMask;
}

uint64_t senderGroupMask(const CableMap* map,
                         ScanDirection direction,
                         uint8_t sourceIndex,
                         bool faultOverlayEnabled) {
    const CableNet net = physicalNetForSource(map, direction, sourceIndex);
    uint64_t mask = direction == ScanDirection::AtoB ? net.aMask : net.bMask;
    if (mask == 0ULL) mask = bitFor(sourceIndex);

    if (faultOverlayEnabled) {
        if (sourceIndex == 22U) mask |= bitFor(23U);
        else if (sourceIndex == 23U) mask |= bitFor(22U);
    }
    return mask;
}

bool demoHighResistance(uint8_t sourceIndex, bool faultOverlayEnabled) {
    return faultOverlayEnabled && sourceIndex == 18U;
}

}  // namespace

Measurement classifyObservation(uint8_t sourceTestIndex,
                                const PhysicalScanObservation& observed,
                                uint64_t expectedReceiverMask,
                                uint64_t expectedSenderMask) {
    (void)sourceTestIndex;
    Measurement measurement{};
    measurement.expectedTestIndex = firstSetIndex(expectedReceiverMask);
    measurement.expectedReceiverMask = expectedReceiverMask;
    measurement.actualReceiverMask = observed.receiverMask;
    measurement.senderGroupMask = observed.senderGroupMask;
    measurement.actualTestIndex = firstSetIndex(observed.receiverMask);
    measurement.resistanceValid = observed.resistanceValid;
    measurement.resistanceMilliOhm = observed.resistanceMilliOhm;
    measurement.resistanceLimitMilliOhm = observed.resistanceLimitMilliOhm;

    const bool senderCorrect = observed.senderGroupMask == expectedSenderMask;
    const bool receiverCorrect = observed.receiverMask == expectedReceiverMask;
    const bool perfectMatch = senderCorrect && receiverCorrect;

    const uint64_t unexpectedSender =
        observed.senderGroupMask & ~expectedSenderMask;
    const uint64_t missingSender =
        expectedSenderMask & ~observed.senderGroupMask;

    // S3 priority: an A-A/B-B mismatch is evaluated before the opposite side.
    if (!senderCorrect) {
        if (unexpectedSender != 0ULL) {
            measurement.result = ElectricalResult::ShortCircuit;
        } else if (missingSender != 0ULL) {
            measurement.result = ElectricalResult::Open;
        } else {
            measurement.result = ElectricalResult::ShortCircuit;
        }
        return measurement;
    }

    if (perfectMatch) {
        measurement.result = observed.highResistance
                                 ? ElectricalResult::HighResistance
                                 : ElectricalResult::Ok;
        return measurement;
    }

    const uint64_t unexpectedReceiver =
        observed.receiverMask & ~expectedReceiverMask;
    const uint64_t missingReceiver =
        expectedReceiverMask & ~observed.receiverMask;

    if (unexpectedReceiver != 0ULL) {
        measurement.result = popcount64(observed.receiverMask) > 1U
                                 ? ElectricalResult::ShortCircuit
                                 : ElectricalResult::WrongConnection;
    } else if (missingReceiver != 0ULL) {
        measurement.result = ElectricalResult::Open;
    } else {
        measurement.result = ElectricalResult::WrongConnection;
    }

    return measurement;
}

Measurement classifyOneToOneObservation(
    uint8_t sourceTestIndex,
    const PhysicalScanObservation& observed) {
    const uint64_t selfMask = bitFor(sourceTestIndex);
    return classifyObservation(sourceTestIndex,
                               observed,
                               selfMask,
                               selfMask);
}

Measurement ScanSource::measure(ScanDirection direction, uint8_t testIndex) {
    return classifyOneToOneObservation(testIndex, observe(direction, testIndex));
}

ScanSource& RoutedScanSource::activeSource() const {
    if (preferPhysical_ && physicalSource_ != nullptr) return *physicalSource_;
    return demoSource_;
}

PhysicalScanObservation RoutedScanSource::observe(ScanDirection direction,
                                                   uint8_t testIndex) {
    return activeSource().observe(direction, testIndex);
}

bool RoutedScanSource::isDemo() const {
    return activeSource().isDemo();
}

PhysicalScanObservation DemoScanSource::observe(ScanDirection direction,
                                                uint8_t testIndex) {
    PhysicalScanObservation observed{};
    observed.senderGroupMask =
        senderGroupMask(physicalMap_, direction, testIndex, faultOverlayEnabled_);
    observed.receiverMask = direction == ScanDirection::AtoB
                                ? receiverMaskAtoB(physicalMap_, testIndex, faultOverlayEnabled_)
                                : receiverMaskBtoA(physicalMap_, testIndex, faultOverlayEnabled_);
    if (testIndex == kPeTestIndex && !peConnectedAcross_) {
        observed.receiverMask = 0ULL;
    } else if (testIndex == kDrainShieldTestIndex &&
               !drainShieldConnectedAcross_) {
        observed.receiverMask = 0ULL;
    }
    observed.highResistance = demoHighResistance(testIndex, faultOverlayEnabled_);

    // Demo-only deterministic Kelvin values. A physical MCP23S17/ADS122C04
    // source must replace these with measured values. Multi-branch/common nets
    // stay N/A until branch-selective Kelvin sampling is available.
    const bool singleReceiver = popcount64(observed.receiverMask) == 1U;
    const bool singleSender = popcount64(observed.senderGroupMask) == 1U;
    if (singleReceiver && singleSender) {
        observed.resistanceValid = true;
        observed.resistanceLimitMilliOhm = 12.0f;
        observed.resistanceMilliOhm = observed.highResistance
            ? 18.40f
            : (7.60f + static_cast<float>(testIndex % 9U) * 0.09f);
    }
    return observed;
}

}  // namespace mg::p4
