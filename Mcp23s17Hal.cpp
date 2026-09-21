#include "Mcp23s17Hal.h"

namespace mg::p4::digitalhw {

bool Mcp23s17Matrix::fail(uint8_t address) {
    status_.healthy = false;
    status_.lastFailedAddress = address;
    return false;
}

bool Mcp23s17Matrix::writeRegister(uint8_t hwAddress,
                                   uint8_t reg,
                                   uint8_t value) {
    const uint8_t tx[3] = {opcode(hwAddress, false), reg, value};
    return transport_.transferFrame(tx, nullptr, sizeof(tx), v33::kMcp23s17TargetSpiHz);
}

bool Mcp23s17Matrix::readRegister(uint8_t hwAddress,
                                  uint8_t reg,
                                  uint8_t& value) {
    const uint8_t tx[3] = {opcode(hwAddress, true), reg, 0x00U};
    uint8_t rx[3] = {0U, 0U, 0U};
    if (!transport_.transferFrame(tx, rx, sizeof(tx), v33::kMcp23s17TargetSpiHz)) {
        return false;
    }
    value = rx[2];
    return true;
}

bool Mcp23s17Matrix::write16(uint8_t hwAddress,
                             uint8_t regA,
                             uint16_t value) {
    return writeRegister(hwAddress, regA, static_cast<uint8_t>(value & 0xFFU)) &&
           writeRegister(hwAddress, static_cast<uint8_t>(regA + 1U),
                         static_cast<uint8_t>((value >> 8U) & 0xFFU));
}

bool Mcp23s17Matrix::read16(uint8_t hwAddress,
                            uint8_t regA,
                            uint16_t& value) {
    uint8_t low = 0U;
    uint8_t high = 0U;
    if (!readRegister(hwAddress, regA, low) ||
        !readRegister(hwAddress, static_cast<uint8_t>(regA + 1U), high)) {
        return false;
    }
    value = static_cast<uint16_t>(low) |
            static_cast<uint16_t>(static_cast<uint16_t>(high) << 8U);
    return true;
}

bool Mcp23s17Matrix::enableHardwareAddressing() {
    // After POR HAEN=0, so all eight devices accept this same write even
    // though A2:A0 differ. One shared-CS broadcast enables address decoding.
    return writeRegister(0U, Iocon, kIoconHaen);
}

bool Mcp23s17Matrix::verifyAddresses() {
    status_.verifiedDeviceCount = 0U;
    for (uint8_t address = v33::kMcp23s17HwAddressFirst;
         address <= v33::kMcp23s17HwAddressLast;
         ++address) {
        uint8_t iocon = 0U;
        if (!readRegister(address, Iocon, iocon) || (iocon & kIoconHaen) == 0U) {
            return fail(address);
        }
        ++status_.verifiedDeviceCount;
    }
    return true;
}

bool Mcp23s17Matrix::safeAllInputs() {
    for (uint8_t address = v33::kMcp23s17HwAddressFirst;
         address <= v33::kMcp23s17HwAddressLast;
         ++address) {
        // Preload OLAT LOW before any pin is ever changed to output.
        if (!write16(address, OlatA, 0x0000U)) return fail(address);
        if (!write16(address, IodirA, 0xFFFFU)) return fail(address);
        // Input pull-ups make every undriven node HIGH; a selected sender LOW
        // then reveals every electrically connected node on both sides.
        if (!write16(address, GppuA, 0xFFFFU)) return fail(address);
    }
    return true;
}

bool Mcp23s17Matrix::begin() {
    status_ = Mcp23s17MatrixStatus{};
    if (!enableHardwareAddressing()) return fail(0U);
    if (!verifyAddresses()) return false;
    if (!safeAllInputs()) return false;
    status_.initialized = true;
    status_.healthy = true;
    return true;
}

bool Mcp23s17Matrix::releaseAll() {
    if (!safeAllInputs()) return false;
    transport_.delayMicroseconds(5U);
    return true;
}

bool Mcp23s17Matrix::driveNodeLow(TestSide side, uint8_t nodeIndex) {
    if (!ready() || !validNodeIndex(nodeIndex)) return false;
    if (!safeAllInputs()) return false;

    const NodeAddress node = nodeAddress(side, nodeIndex);
    const uint16_t outputMask = static_cast<uint16_t>(1U << node.localPin);
    const uint16_t direction = static_cast<uint16_t>(0xFFFFU & ~outputMask);

    // OLAT is already LOW from safeAllInputs(); direction is changed last so
    // the selected sender cannot briefly drive HIGH.
    if (!write16(node.hardwareAddress, IodirA, direction)) {
        return fail(node.hardwareAddress);
    }
    return true;
}

bool Mcp23s17Matrix::readSideLowMask(TestSide side, uint64_t& lowMask) {
    lowMask = 0ULL;
    const uint8_t firstExpander = sideBaseExpander(side);
    for (uint8_t bank = 0U; bank < 4U; ++bank) {
        const uint8_t address = static_cast<uint8_t>(firstExpander + bank);
        uint16_t gpio = 0xFFFFU;
        if (!read16(address, GpioA, gpio)) return fail(address);
        const uint16_t low = static_cast<uint16_t>(~gpio);
        lowMask |= static_cast<uint64_t>(low) << (bank * 16U);
    }
    return true;
}

bool Mcp23s17Matrix::readAllLowMasks(uint64_t& aLowMask,
                                     uint64_t& bLowMask) {
    return readSideLowMask(TestSide::A, aLowMask) &&
           readSideLowMask(TestSide::B, bLowMask);
}

bool Mcp23s17Matrix::scanFromNode(TestSide sourceSide,
                                  uint8_t sourceIndex,
                                  uint64_t& senderLowMask,
                                  uint64_t& receiverLowMask,
                                  uint32_t settleUs) {
    senderLowMask = 0ULL;
    receiverLowMask = 0ULL;
    if (!ready() || !validNodeIndex(sourceIndex)) return false;
    if (!driveNodeLow(sourceSide, sourceIndex)) return false;

    transport_.delayMicroseconds(settleUs);

    uint64_t aLow = 0ULL;
    uint64_t bLow = 0ULL;
    const bool readOk = readAllLowMasks(aLow, bLow);
    const bool releaseOk = releaseAll();
    if (!readOk || !releaseOk) return false;

    if (sourceSide == TestSide::A) {
        senderLowMask = aLow;
        receiverLowMask = bLow;
    } else {
        senderLowMask = bLow;
        receiverLowMask = aLow;
    }
    return true;
}

}  // namespace mg::p4::digitalhw
