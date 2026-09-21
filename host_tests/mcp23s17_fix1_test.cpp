#include <assert.h>
#include <stdint.h>

#include <array>
#include <queue>
#include <vector>

#include "DigitalNodeMap.h"
#include "Mcp23s17Hal.h"
#include "Mcp23s17ScanSource.h"
#include "ScanSource.h"

using mg::p4::DemoScanSource;
using mg::p4::Mcp23s17ScanSource;
using mg::p4::PhysicalScanObservation;
using mg::p4::RoutedScanSource;
using mg::p4::ScanDirection;
using mg::p4::digitalhw::Mcp23s17Matrix;
using mg::p4::digitalhw::Mcp23s17SpiTransport;
using mg::p4::digitalhw::TestSide;
using mg::p4::digitalhw::nodeAddress;
using mg::p4::digitalhw::nodeBit;

namespace {

constexpr uint8_t kIodirA = 0x00U;
constexpr uint8_t kIodirB = 0x01U;
constexpr uint8_t kIocon = 0x0AU;
constexpr uint8_t kGppuA = 0x0CU;
constexpr uint8_t kGppuB = 0x0DU;
constexpr uint8_t kGpioA = 0x12U;
constexpr uint8_t kGpioB = 0x13U;
constexpr uint8_t kOlatA = 0x14U;
constexpr uint8_t kOlatB = 0x15U;
constexpr uint8_t kHaen = 0x08U;

struct Endpoint {
    TestSide side;
    uint8_t node;
};

class FakeMcpTransport final : public Mcp23s17SpiTransport {
public:
    FakeMcpTransport() {
        for (auto& device : regs_) {
            device.fill(0U);
            device[kIodirA] = 0xFFU;
            device[kIodirB] = 0xFFU;
        }
    }

    void connect(TestSide aSide, uint8_t aNode,
                 TestSide bSide, uint8_t bNode) {
        const int a = endpointIndex(aSide, aNode);
        const int b = endpointIndex(bSide, bNode);
        adjacency_[a].push_back(b);
        adjacency_[b].push_back(a);
    }

    bool transferFrame(const uint8_t* tx,
                       uint8_t* rx,
                       size_t length,
                       uint32_t spiHz) override {
        assert(tx != nullptr);
        assert(length == 3U);
        assert(spiHz == mg::p4::v33::kMcp23s17TargetSpiHz);
        ++frameCount_;
        if (rx != nullptr) {
            rx[0] = 0U;
            rx[1] = 0U;
            rx[2] = 0U;
        }

        const bool read = (tx[0] & 0x01U) != 0U;
        const uint8_t address = static_cast<uint8_t>((tx[0] >> 1U) & 0x07U);
        const uint8_t reg = tx[1];

        if (read) {
            if (!haenEnabled()) return false;
            if (address >= regs_.size()) return false;
            uint8_t value = 0U;
            if (reg == kGpioA || reg == kGpioB) {
                value = gpioByte(address, reg == kGpioB ? 1U : 0U);
                ++gpioReadCount_[address];
            } else {
                value = regs_[address][reg];
            }
            if (rx != nullptr) rx[2] = value;
            return true;
        }

        if (reg == kIocon && !haenEnabled()) {
            for (auto& device : regs_) device[kIocon] = tx[2];
            return true;
        }
        if (!haenEnabled() || address >= regs_.size()) return false;
        regs_[address][reg] = tx[2];
        return true;
    }

    void delayMicroseconds(uint32_t delayUs) override {
        delayTotalUs_ += delayUs;
    }

    uint16_t read16Raw(uint8_t address, uint8_t regA) const {
        return static_cast<uint16_t>(regs_[address][regA]) |
               static_cast<uint16_t>(static_cast<uint16_t>(regs_[address][regA + 1U]) << 8U);
    }

    uint32_t gpioReadCount(uint8_t address) const { return gpioReadCount_[address]; }
    uint32_t frameCount() const { return frameCount_; }
    uint32_t delayTotalUs() const { return delayTotalUs_; }

private:
    static int endpointIndex(TestSide side, uint8_t node) {
        return (side == TestSide::A ? 0 : 64) + node;
    }

    bool haenEnabled() const {
        for (const auto& device : regs_) {
            if ((device[kIocon] & kHaen) == 0U) return false;
        }
        return true;
    }

    std::array<bool, 128> electricallyLow() const {
        std::array<bool, 128> low{};
        std::queue<int> pending;

        for (uint8_t address = 0U; address < 8U; ++address) {
            const uint16_t iodir = read16Raw(address, kIodirA);
            const uint16_t olat = read16Raw(address, kOlatA);
            for (uint8_t local = 0U; local < 16U; ++local) {
                const bool output = (iodir & static_cast<uint16_t>(1U << local)) == 0U;
                const bool outputLow = (olat & static_cast<uint16_t>(1U << local)) == 0U;
                if (!output || !outputLow) continue;
                const TestSide side = address < 4U ? TestSide::A : TestSide::B;
                const uint8_t bank = static_cast<uint8_t>(address % 4U);
                const uint8_t node = static_cast<uint8_t>(bank * 16U + local);
                const int index = endpointIndex(side, node);
                if (!low[index]) {
                    low[index] = true;
                    pending.push(index);
                }
            }
        }

        while (!pending.empty()) {
            const int current = pending.front();
            pending.pop();
            for (const int next : adjacency_[current]) {
                if (!low[next]) {
                    low[next] = true;
                    pending.push(next);
                }
            }
        }
        return low;
    }

    uint8_t gpioByte(uint8_t address, uint8_t port) const {
        const auto low = electricallyLow();
        uint8_t value = 0xFFU;
        const TestSide side = address < 4U ? TestSide::A : TestSide::B;
        const uint8_t bank = static_cast<uint8_t>(address % 4U);
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            const uint8_t local = static_cast<uint8_t>(port * 8U + bit);
            const uint8_t node = static_cast<uint8_t>(bank * 16U + local);
            if (low[endpointIndex(side, node)]) {
                value = static_cast<uint8_t>(value & ~(1U << bit));
            }
        }
        return value;
    }

    std::array<std::array<uint8_t, 0x16U>, 8U> regs_{};
    std::array<std::vector<int>, 128U> adjacency_{};
    std::array<uint32_t, 8U> gpioReadCount_{};
    uint32_t frameCount_ = 0U;
    uint32_t delayTotalUs_ = 0U;
};

void testNodeMap() {
    const auto aPe = nodeAddress(TestSide::A, 0U);
    assert(aPe.expanderIndex == 0U && aPe.localPin == 0U);
    const auto a15 = nodeAddress(TestSide::A, 15U);
    assert(a15.expanderIndex == 0U && a15.localPin == 15U);
    const auto a16 = nodeAddress(TestSide::A, 16U);
    assert(a16.expanderIndex == 1U && a16.localPin == 0U);
    const auto aDs = nodeAddress(TestSide::A, 63U);
    assert(aDs.expanderIndex == 3U && aDs.localPin == 15U);
    const auto bPe = nodeAddress(TestSide::B, 0U);
    assert(bPe.expanderIndex == 4U && bPe.localPin == 0U);
    const auto bDs = nodeAddress(TestSide::B, 63U);
    assert(bDs.expanderIndex == 7U && bDs.localPin == 15U);
}

void testHaenAndSafeBoot() {
    FakeMcpTransport bus;
    Mcp23s17Matrix matrix(bus);
    assert(matrix.begin());
    assert(matrix.ready());
    assert(matrix.status().verifiedDeviceCount == 8U);
    for (uint8_t address = 0U; address < 8U; ++address) {
        assert(bus.read16Raw(address, kIodirA) == 0xFFFFU);
        assert(bus.read16Raw(address, kGppuA) == 0xFFFFU);
        assert(bus.read16Raw(address, kOlatA) == 0x0000U);
    }
}

void testFull128NodeObservationIncludesSameSide() {
    FakeMcpTransport bus;
    bus.connect(TestSide::A, 3U, TestSide::A, 4U);
    bus.connect(TestSide::A, 3U, TestSide::B, 3U);
    Mcp23s17Matrix matrix(bus);
    assert(matrix.begin());

    uint64_t sender = 0ULL;
    uint64_t receiver = 0ULL;
    assert(matrix.scanFromNode(TestSide::A, 3U, sender, receiver));
    assert(sender == (nodeBit(3U) | nodeBit(4U)));
    assert(receiver == nodeBit(3U));
    for (uint8_t address = 0U; address < 8U; ++address) {
        assert(bus.gpioReadCount(address) > 0U);
        assert(bus.read16Raw(address, kIodirA) == 0xFFFFU);
    }
    assert(bus.delayTotalUs() >= 45U);

    Mcp23s17ScanSource source(matrix);
    const PhysicalScanObservation reverse = source.observe(ScanDirection::BtoA, 3U);
    assert(reverse.senderGroupMask == nodeBit(3U));
    assert(reverse.receiverMask == (nodeBit(3U) | nodeBit(4U)));
}

void testRoutedSourceDefaultsToDemoAndCanSwitch() {
    DemoScanSource demo;
    demo.setFaultOverlayEnabled(false);

    FakeMcpTransport bus;
    bus.connect(TestSide::A, 7U, TestSide::B, 9U);
    Mcp23s17Matrix matrix(bus);
    assert(matrix.begin());
    Mcp23s17ScanSource physical(matrix);

    RoutedScanSource routed(demo);
    routed.attachPhysical(&physical);
    assert(routed.isDemo());
    assert(!routed.usingPhysical());
    assert(routed.observe(ScanDirection::AtoB, 7U).receiverMask == nodeBit(7U));

    routed.preferPhysical(true);
    assert(!routed.isDemo());
    assert(routed.usingPhysical());
    const PhysicalScanObservation observed = routed.observe(ScanDirection::AtoB, 7U);
    assert(observed.senderGroupMask == nodeBit(7U));
    assert(observed.receiverMask == nodeBit(9U));
    assert(!observed.resistanceValid);
}

}  // namespace

int main() {
    testNodeMap();
    testHaenAndSafeBoot();
    testFull128NodeObservationIncludesSameSide();
    testRoutedSourceDefaultsToDemoAndCanSwitch();
    return 0;
}
