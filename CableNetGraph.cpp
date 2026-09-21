#include "CableNetGraph.h"

namespace mg::p4 {
namespace {

constexpr uint8_t kNodeCount = static_cast<uint8_t>(kTestPointsPerSide * 2U);

uint64_t bitFor(uint8_t index) {
    return index < kTestPointsPerSide ? (1ULL << index) : 0ULL;
}

bool nodeVisible(uint8_t node, uint64_t visibleMaskA, uint64_t visibleMaskB) {
    if (node < kTestPointsPerSide) {
        return (visibleMaskA & bitFor(node)) != 0ULL;
    }
    const uint8_t index = static_cast<uint8_t>(node - kTestPointsPerSide);
    return (visibleMaskB & bitFor(index)) != 0ULL;
}

bool hasAnyVisibleEdge(const CableMap& map,
                       uint8_t node,
                       uint64_t visibleMaskA,
                       uint64_t visibleMaskB) {
    if (node < kTestPointsPerSide) {
        const uint8_t a = node;
        return (map.sameSideMaskA(a) & visibleMaskA) != 0ULL ||
               (map.receiverMaskAtoB(a) & visibleMaskB) != 0ULL;
    }
    const uint8_t b = static_cast<uint8_t>(node - kTestPointsPerSide);
    return (map.sameSideMaskB(b) & visibleMaskB) != 0ULL ||
           (map.receiverMaskBtoA(b) & visibleMaskA) != 0ULL;
}

void enqueueMask(uint64_t mask,
                 uint8_t base,
                 bool* visited,
                 uint8_t* queue,
                 uint8_t& tail) {
    for (uint8_t index = 0; index < kTestPointsPerSide; ++index) {
        if ((mask & bitFor(index)) == 0ULL) {
            continue;
        }
        const uint8_t node = static_cast<uint8_t>(base + index);
        if (!visited[node]) {
            visited[node] = true;
            queue[tail++] = node;
        }
    }
}

}  // namespace

uint8_t CableNetGraph::popcount(uint64_t value) {
    uint8_t count = 0U;
    while (value != 0ULL) {
        value &= (value - 1ULL);
        ++count;
    }
    return count;
}

size_t CableNetGraph::build(const CableMap& map,
                            uint64_t visibleMask,
                            CableNet* output,
                            size_t capacity) {
    return build(map, visibleMask, visibleMask, output, capacity);
}

size_t CableNetGraph::build(const CableMap& map,
                            uint64_t visibleMaskA,
                            uint64_t visibleMaskB,
                            CableNet* output,
                            size_t capacity) {
    if (output == nullptr || capacity == 0U ||
        (visibleMaskA | visibleMaskB) == 0ULL) {
        return 0U;
    }

    bool visited[kTestPointsPerSide * 2U]{};
    uint8_t queue[kTestPointsPerSide * 2U]{};
    size_t netCount = 0U;

    for (uint8_t seed = 0; seed < kNodeCount; ++seed) {
        if (visited[seed] ||
            !nodeVisible(seed, visibleMaskA, visibleMaskB) ||
            !hasAnyVisibleEdge(map, seed, visibleMaskA, visibleMaskB)) {
            continue;
        }

        CableNet net{};
        uint8_t head = 0U;
        uint8_t tail = 0U;
        visited[seed] = true;
        queue[tail++] = seed;

        while (head < tail) {
            const uint8_t node = queue[head++];
            if (node < kTestPointsPerSide) {
                const uint8_t a = node;
                net.aMask |= bitFor(a);
                enqueueMask(map.sameSideMaskA(a) & visibleMaskA,
                            0U, visited, queue, tail);
                enqueueMask(map.receiverMaskAtoB(a) & visibleMaskB,
                            kTestPointsPerSide, visited, queue, tail);
            } else {
                const uint8_t b = static_cast<uint8_t>(node - kTestPointsPerSide);
                net.bMask |= bitFor(b);
                enqueueMask(map.sameSideMaskB(b) & visibleMaskB,
                            kTestPointsPerSide, visited, queue, tail);
                enqueueMask(map.receiverMaskBtoA(b) & visibleMaskA,
                            0U, visited, queue, tail);
            }
        }

        if ((net.aMask | net.bMask) != 0ULL && netCount < capacity) {
            output[netCount++] = net;
        }
    }

    return netCount;
}

}  // namespace mg::p4
