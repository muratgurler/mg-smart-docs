#pragma once

#include <stdint.h>

#include "V33BoardConfig.h"

namespace mg::p4::digitalhw {

enum class TestSide : uint8_t {
    A = 0,
    B = 1,
};

struct NodeAddress {
    TestSide side = TestSide::A;
    uint8_t nodeIndex = 0U;       // 0=PE, 1..62=signal, 63=dS
    uint8_t expanderIndex = 0U;   // U1..U8 => 0..7
    uint8_t hardwareAddress = 0U; // MCP23S17 A2:A0 => 0..7
    uint8_t localPin = 0U;        // 0..15 => GPA0..7, GPB0..7
    uint8_t port = 0U;            // 0=GPIOA, 1=GPIOB
    uint8_t portBit = 0U;         // 0..7
};

constexpr bool validNodeIndex(uint8_t nodeIndex) {
    return nodeIndex < v33::kTestNodeCountPerSide;
}

constexpr uint8_t sideBaseExpander(TestSide side) {
    return side == TestSide::A ? 0U : 4U;
}

constexpr NodeAddress nodeAddress(TestSide side, uint8_t nodeIndex) {
    const uint8_t safeNode = validNodeIndex(nodeIndex) ? nodeIndex : 0U;
    const uint8_t expander = static_cast<uint8_t>(sideBaseExpander(side) + safeNode / 16U);
    const uint8_t localPin = static_cast<uint8_t>(safeNode % 16U);
    return NodeAddress{
        side,
        safeNode,
        expander,
        expander,
        localPin,
        static_cast<uint8_t>(localPin / 8U),
        static_cast<uint8_t>(localPin % 8U),
    };
}

constexpr uint64_t nodeBit(uint8_t nodeIndex) {
    return validNodeIndex(nodeIndex) ? (1ULL << nodeIndex) : 0ULL;
}

static_assert(v33::kDigitalExpanderCount == 8U,
              "128-node map requires exactly 8 MCP23S17 expanders");
static_assert(v33::kTestNodeCountPerSide == 64U,
              "MG Smart Tester side map requires exactly 64 nodes");
static_assert(nodeAddress(TestSide::A, 0U).expanderIndex == 0U,
              "A_PE must map to U1");
static_assert(nodeAddress(TestSide::A, 63U).expanderIndex == 3U,
              "A_dS must map to U4");
static_assert(nodeAddress(TestSide::B, 0U).expanderIndex == 4U,
              "B_PE must map to U5");
static_assert(nodeAddress(TestSide::B, 63U).expanderIndex == 7U,
              "B_dS must map to U8");

}  // namespace mg::p4::digitalhw
