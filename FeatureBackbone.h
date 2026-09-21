#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "V33HardwareManager.h"

namespace mg::p4 {

enum class BackboneModuleId : uint8_t {
    Scan128 = 0,
    Kelvin,
    SelfTestCalibration,
    Tdr,
    PairIntegrity,
    UsbC,
    PeDs,
    FixtureSpc,
    SmartProbe,
    Network,
    Voice,
    EnvironmentAwg,
    BarcodeQr,
    ComponentTest,
    HardwareValidation,
    CableLearn,
    MultiConnector,
    FlexGlitch,
    Count
};

enum class BackboneRunState : uint8_t {
    Idle,
    Running,
    Paused,
    Complete,
    Fault,
};

struct BackboneSnapshot {
    BackboneModuleId module = BackboneModuleId::Scan128;
    BackboneRunState state = BackboneRunState::Idle;
    uint8_t progressPercent = 0;
    uint8_t phaseIndex = 0;
    uint8_t phaseCount = 1;
    bool usingDemo = true;
    bool hardwareAvailable = false;
    char metric1[64] = {};
    char metric2[64] = {};
    char metric3[64] = {};
};

class BackboneModuleDriver {
public:
    virtual ~BackboneModuleDriver() = default;
    virtual bool available() const = 0;
    virtual void reset() = 0;
    virtual void start() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void step() = 0;
    virtual void tick(uint32_t nowMs) = 0;
    virtual BackboneSnapshot snapshot() const = 0;
};

// Common execution seam for the entire MG Smart Tester backbone.
// Today every module falls back to the deterministic demo state machine.
// When the carrier PCB is ready, a real driver can be attached per module
// without changing the menus/screens that already call this class.
class FeatureBackbone {
public:
    void begin(v33::V33HardwareManager& hardware);
    void attachDriver(BackboneModuleId module, BackboneModuleDriver* driver);
    void preferHardware(bool enabled) { preferHardware_ = enabled; }

    void select(BackboneModuleId module, const char* workflowTitle = nullptr);
    void start();
    void pauseResume();
    void step();
    void reset();
    void tick(uint32_t nowMs);

    BackboneModuleId selectedModule() const { return selectedModule_; }
    const char* workflowTitle() const { return workflowTitle_; }
    BackboneSnapshot snapshot() const;

    static const char* moduleTitle(BackboneModuleId module, bool turkish);
    static const char* moduleDescription(BackboneModuleId module, bool turkish);
    static const char* phaseText(BackboneModuleId module, uint8_t phase, bool turkish);
    static const char* stateText(BackboneRunState state, bool turkish);

private:
    static constexpr uint8_t kModuleCount = static_cast<uint8_t>(BackboneModuleId::Count);

    BackboneModuleDriver* activeDriver() const;
    bool driverUsable(BackboneModuleId module) const;
    uint8_t demoPhaseCount(BackboneModuleId module) const;
    void resetDemo();
    void advanceDemo();
    void refreshDemoMetrics();

    v33::V33HardwareManager* hardware_ = nullptr;
    BackboneModuleDriver* drivers_[kModuleCount]{};
    BackboneModuleId selectedModule_ = BackboneModuleId::Scan128;
    BackboneRunState demoState_ = BackboneRunState::Idle;
    uint8_t demoPhaseIndex_ = 0;
    uint32_t lastDemoStepMs_ = 0;
    bool preferHardware_ = false;
    char workflowTitle_[48] = {};
    BackboneSnapshot demoSnapshot_{};
};

}  // namespace mg::p4
