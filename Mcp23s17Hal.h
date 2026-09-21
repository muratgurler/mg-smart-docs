#pragma once

#include <stddef.h>
#include <stdint.h>

#include "DigitalNodeMap.h"
#include "V33BoardConfig.h"

namespace mg::p4::digitalhw {

// Transport owns the electrical SPI transaction and the shared MCP_DIG_CS_N
// edge. The register/opcode protocol stays inside Mcp23s17Matrix so it can be
// host-tested without ESP32 hardware.
class Mcp23s17SpiTransport {
public:
    virtual ~Mcp23s17SpiTransport() = default;
    virtual bool transferFrame(const uint8_t* tx,
                               uint8_t* rx,
                               size_t length,
                               uint32_t spiHz) = 0;
    virtual void delayMicroseconds(uint32_t delayUs) = 0;
};

struct Mcp23s17MatrixStatus {
    bool initialized = false;
    bool healthy = false;
    uint8_t verifiedDeviceCount = 0U;
    uint8_t lastFailedAddress = 0xFFU;
};

class Mcp23s17Matrix {
public:
    explicit Mcp23s17Matrix(Mcp23s17SpiTransport& transport)
        : transport_(transport) {}

    bool begin();
    bool safeAllInputs();
    bool driveNodeLow(TestSide side, uint8_t nodeIndex);
    bool releaseAll();
    bool readSideLowMask(TestSide side, uint64_t& lowMask);
    bool readAllLowMasks(uint64_t& aLowMask, uint64_t& bLowMask);
    bool scanFromNode(TestSide sourceSide,
                      uint8_t sourceIndex,
                      uint64_t& senderLowMask,
                      uint64_t& receiverLowMask,
                      uint32_t settleUs = 40U);

    const Mcp23s17MatrixStatus& status() const { return status_; }
    bool ready() const { return status_.initialized && status_.healthy; }

    static constexpr uint8_t opcode(uint8_t hwAddress, bool read) {
        return static_cast<uint8_t>(0x40U | ((hwAddress & 0x07U) << 1U) |
                                    (read ? 1U : 0U));
    }

private:
    enum Register : uint8_t {
        IodirA = 0x00,
        IodirB = 0x01,
        Iocon = 0x0A,
        GppuA = 0x0C,
        GppuB = 0x0D,
        GpioA = 0x12,
        GpioB = 0x13,
        OlatA = 0x14,
        OlatB = 0x15,
    };

    static constexpr uint8_t kIoconHaen = 0x08U;

    bool writeRegister(uint8_t hwAddress, uint8_t reg, uint8_t value);
    bool readRegister(uint8_t hwAddress, uint8_t reg, uint8_t& value);
    bool write16(uint8_t hwAddress, uint8_t regA, uint16_t value);
    bool read16(uint8_t hwAddress, uint8_t regA, uint16_t& value);
    bool enableHardwareAddressing();
    bool verifyAddresses();
    bool fail(uint8_t address);

    Mcp23s17SpiTransport& transport_;
    Mcp23s17MatrixStatus status_{};
};

}  // namespace mg::p4::digitalhw
