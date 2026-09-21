#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CableMap.h"

namespace mg::p4 {

// One electrical net after collapsing every A->B, A-A and B-B relation into
// connected components. A graph screen should draw one trunk per net, then
// short branches close to the A/B connector sides.
struct CableNet {
    uint64_t aMask = 0ULL;
    uint64_t bMask = 0ULL;
};

class CableNetGraph {
public:
    // Builds connected electrical nets, clipped to visibleMask. Isolated pins
    // are intentionally omitted because there is no electrical connection to
    // draw. Returns the number of nets written to output.
    static size_t build(const CableMap& map,
                        uint64_t visibleMask,
                        CableNet* output,
                        size_t capacity);

    // Side-specific visibility is used when PE/dS is enabled on only A or B.
    static size_t build(const CableMap& map,
                        uint64_t visibleMaskA,
                        uint64_t visibleMaskB,
                        CableNet* output,
                        size_t capacity);

    static uint8_t popcount(uint64_t value);
};

}  // namespace mg::p4
