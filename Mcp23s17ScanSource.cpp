#include "Mcp23s17ScanSource.h"

namespace mg::p4 {

PhysicalScanObservation Mcp23s17ScanSource::observe(ScanDirection direction,
                                                     uint8_t testIndex) {
    PhysicalScanObservation observation{};
    const digitalhw::TestSide sourceSide =
        direction == ScanDirection::AtoB
            ? digitalhw::TestSide::A
            : digitalhw::TestSide::B;

    uint64_t senderLow = 0ULL;
    uint64_t receiverLow = 0ULL;
    if (!matrix_.scanFromNode(sourceSide,
                              testIndex,
                              senderLow,
                              receiverLow)) {
        return observation;
    }

    // The matrix reads all four MCP23S17 devices on the sender side and all
    // four on the receiver side while exactly one sender is driven LOW. This
    // preserves the MG regression rule: the other 127 nodes are observed,
    // including same-side shorts; only the classifier decides whether a
    // multi-node group is expected or faulty.
    observation.senderGroupMask = senderLow;
    observation.receiverMask = receiverLow;
    return observation;
}

}  // namespace mg::p4
