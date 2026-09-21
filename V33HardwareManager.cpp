#include "V33HardwareManager.h"

namespace mg::p4::v33 {

void V33HardwareManager::setSafeNotConfigured() {
    digital_ = {HardwareState::NotConfigured, "8x MCP23S17 HAL ready / physical backend gated until PCB+pin freeze"};
    kelvin_  = {HardwareState::NotConfigured, "8x ADG725 + ADS122C04 / CH0 I2C; PCB validation pending"};
    flex_    = {HardwareState::NotConfigured, "TLV3601 + latch / pin map pending"};
    tdr_     = {HardwareState::NotConfigured, "21x ADG904 + TDC7200 / pin map pending"};
    pair_    = {HardwareState::NotConfigured, "8x ADG732 + 4x TMUX1219 / pin map pending"};
}

void V33HardwareManager::begin() {
    setSafeNotConfigured();
    Serial.println("[HW] hardware abstraction initialized");
    if (!kHardwareEnabled) {
        Serial.println("[HW] physical carrier access DISABLED: PCB validation/pin freeze pending");
        Serial.printf("[HW] target backbone: MCP23S17=%u ADG725=%u ADG904=%u ADG732=%u TMUX1219=%u\n",
                      kDigitalExpanderCount,
                      kKelvinMuxCount,
                      kTdrRfMuxCount,
                      kPairMuxCount,
                      kPairCombineMuxCount);
        Serial.flush();
        return;
    }

    if (digitalBackend_.begin()) {
        digital_ = {HardwareState::Ready, "8x MCP23S17 SPI backend READY; HAEN/address read-back passed"};
    } else {
        digital_ = {HardwareState::Fault, "MCP23S17 backend failed initialization/read-back"};
    }

    // Other analog/high-speed modules remain separately gated. Fix1 only
    // prepares the digital 128-node physical backend.
}

void V33HardwareManager::tick() {
    // Reserved for asynchronous V3.3 module state machines.
}

bool V33HardwareManager::allReady() const {
    return digital_.state == HardwareState::Ready &&
           kelvin_.state == HardwareState::Ready &&
           flex_.state == HardwareState::Ready &&
           tdr_.state == HardwareState::Ready &&
           pair_.state == HardwareState::Ready;
}

const char* V33HardwareManager::stateText(HardwareState state) const {
    switch (state) {
        case HardwareState::NotConfigured: return "NOT CONFIGURED";
        case HardwareState::NotDetected:   return "NOT DETECTED";
        case HardwareState::Ready:         return "READY";
        case HardwareState::Fault:         return "FAULT";
    }
    return "UNKNOWN";
}

}  // namespace mg::p4::v33
