#pragma once

#include <stdint.h>

namespace mg::p4::v33 {

// Real production digital I/O hardware is MCP23S17 over SPI. The carrier PCB
// and final GPIO/bus pin freeze are not ready yet, so external test hardware
// must remain electrically inactive. Demo/simulation stays available above
// this boundary without pretending to be a physical measurement.
constexpr bool kCarrierPcbValidated = false;
constexpr bool kExternalPinMapFrozen = false;
constexpr bool kHardwareEnabled = false;
static_assert(!kHardwareEnabled || (kCarrierPcbValidated && kExternalPinMapFrozen),
              "Physical hardware cannot be enabled before PCB validation and pin freeze");

// Frozen logical node architecture.
constexpr uint8_t kDigitalExpanderCount = 8;   // MCP23S17, HAEN=1, HW addr 0..7
constexpr uint8_t kKelvinMuxCount = 8;         // ADG725, 4 per side FORCE/SENSE
constexpr uint8_t kTdrRfMuxCount = 21;         // ADG904
constexpr uint8_t kPairMuxCount = 8;           // ADG732
constexpr uint8_t kPairCombineMuxCount = 4;    // TMUX1219
constexpr uint8_t kTestNodeCountPerSide = 64;  // node 0=PE, 1..62=signal, 63=dS

// SPI digital matrix addressing; all MCP23S17 parts share MCP_DIG_CS_N.
constexpr uint8_t kMcp23s17HwAddressFirst = 0;
constexpr uint8_t kMcp23s17HwAddressLast  = 7;
constexpr uint32_t kMcp23s17TargetSpiHz   = 10000000UL;  // prototype must verify 10/8/5 MHz

// TCA9548A channelized I2C architecture.
constexpr uint8_t kTca9548aAddress = 0x70;
constexpr uint8_t kAds122c04Address = 0x40; // CH0
constexpr uint8_t kTca6424aAddress = 0x22;  // CH1
constexpr uint8_t kTca9534aAddress = 0x38;  // CH1
constexpr uint8_t kFusb302Address = 0x22;   // CH2; isolated from TCA6424A by mux channel
constexpr uint8_t kSht40Address = 0x44;     // CH3 typical

// Candidate GPIO values retained for PCB design review. These are not an
// authorization to drive hardware while kHardwareEnabled=false. GPIO5 is the
// JC1060 LCD reset and must never be allocated to the external carrier.
constexpr int kGpioSpiSclk = 1;
constexpr int kGpioSpiMosi = 2;
constexpr int kGpioSpiMiso = 3;
constexpr int kGpioCtrlLatch = 4;
constexpr int kGpioReservedLcdReset = 5;
constexpr int kGpioI2cSda = 7;
constexpr int kGpioI2cScl = 8;
constexpr int kGpioTdrTrigger = 20;
constexpr int kGpioTdcIntN = 32;
constexpr int kGpioPairConvstCs = 33;
constexpr int kGpioMcpDigCsN = 45;
constexpr int kGpioGlitchEventQ = 46;
constexpr int kGpioFusb302IntN = 47;

}  // namespace mg::p4::v33
