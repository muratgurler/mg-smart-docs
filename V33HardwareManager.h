#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "V33BoardConfig.h"
#include "Mcp23s17PhysicalBackend.h"

namespace mg::p4::v33 {

enum class HardwareState : uint8_t {
    NotConfigured,
    NotDetected,
    Ready,
    Fault,
};

struct ModuleStatus {
    HardwareState state = HardwareState::NotConfigured;
    const char* detail = "NOT CONFIGURED";
};

class V33HardwareManager {
public:
    void begin();
    void tick();

    bool hardwareEnabled() const { return kHardwareEnabled; }
    bool carrierPcbValidated() const { return kCarrierPcbValidated; }
    bool externalPinMapFrozen() const { return kExternalPinMapFrozen; }
    const ModuleStatus& digital() const { return digital_; }
    const ModuleStatus& kelvin() const { return kelvin_; }
    const ModuleStatus& flex() const { return flex_; }
    const ModuleStatus& tdr() const { return tdr_; }
    const ModuleStatus& pair() const { return pair_; }
    ScanSource* digitalScanSource() { return digitalBackend_.scanSource(); }

    bool allReady() const;
    const char* stateText(HardwareState state) const;

private:
    void setSafeNotConfigured();

    ModuleStatus digital_{};
    ModuleStatus kelvin_{};
    ModuleStatus flex_{};
    ModuleStatus tdr_{};
    ModuleStatus pair_{};
    Mcp23s17PhysicalBackend digitalBackend_{};
};

}  // namespace mg::p4::v33
