#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CableMap.h"
#include "CableProfile.h"
#include "ScanSource.h"

namespace mg::p4 {

enum class RunState : uint8_t {
    Idle,
    Running,
    Paused,
    Complete,
};

enum class PointVisualStatus : uint8_t {
    Untested,
    Scanning,
    Ok,
    Open,
    ShortCircuit,
    WrongConnection,
    HighResistance,
};

class ScanSession {
public:
    ScanSession(const CableProfile& profile, ScanSource& source);

    void setProfile(const CableProfile& profile);
    void setOneToOneMap();
    void setCustomMap(const CableMap& map);
    void startOrPause();
    void start();
    void pause();
    void step();
    void reset();
    void toggleDirection();
    void setStopOnError(bool enabled);
    void setContinuousMode(bool enabled);
    void tick(uint32_t nowMs);
    void setDemoStepIntervalMs(uint32_t intervalMs) {
        scanStepIntervalMs_ = intervalMs;
    }
    void setScanSpeedPercent(uint8_t percent);

    const CableProfile& profile() const { return *profile_; }
    RunState runState() const { return runState_; }
    ScanDirection direction() const { return direction_; }
    ScanDirection displayDirection() const { return displayDirection_; }
    uint8_t currentSlot() const { return currentSlot_; }
    uint8_t currentTestIndex() const;
    uint8_t displayTestIndexA() const { return displayTestIndexA_; }
    uint8_t displayTestIndexB() const { return displayTestIndexB_; }
    PointVisualStatus activeStatusA() const { return activeStatusA_; }
    PointVisualStatus activeStatusB() const { return activeStatusB_; }
    // Coherent live connection snapshot used by the scan UI. During an
    // in-progress point these masks contain the expected sender/receiver
    // group; after classification they contain the actually observed group.
    uint64_t displaySenderMask() const { return displaySenderMask_; }
    uint64_t displayReceiverMask() const { return displayReceiverMask_; }
    ElectricalResult displayResult() const { return displayResult_; }
    bool displayResistanceValid() const { return displayResistanceValid_; }
    float displayResistanceMilliOhm() const { return displayResistanceMilliOhm_; }
    uint8_t completedMeasurements() const { return completedMeasurements_; }
    uint8_t totalMeasurements() const;
    uint8_t progressPercent() const;
    bool stopOnError() const { return stopOnError_; }
    bool continuousMode() const { return continuousMode_; }
    bool isDemo() const { return source_.isDemo(); }
    bool usesCustomMap() const { return customMap_ != nullptr; }
    const CableMap* customMap() const { return customMap_; }
    uint8_t scanSpeedPercent() const { return scanSpeedPercent_; }
    uint32_t scanStepIntervalMs() const { return scanStepIntervalMs_; }

    // Physical fixture labels never move. Only direction() changes.
    const char* leftFixtureLabel() const { return "A"; }
    const char* rightFixtureLabel() const { return "B"; }

    PointVisualStatus statusA(uint8_t slot) const;
    PointVisualStatus statusB(uint8_t slot) const;
    ElectricalResult reportResult(ScanDirection direction,
                                  uint8_t slot) const;
    Measurement reportMeasurement(ScanDirection direction,
                                  uint8_t slot) const;
    uint8_t errorCount(ScanDirection direction) const;

    bool consumeDirty();

private:
    void clearResults();
    void clearVisuals();
    void markCurrentScanning();
    void showMeasurement(uint8_t sourceSlot,
                         const Measurement& measurement);
    void finishCurrentMeasurement();
    void advanceAfterMeasurement(ElectricalResult result);
    void beginNextContinuousCycle();
    uint8_t directionChangeTargetSlot() const;
    bool slotEnabledForDirection(ScanDirection direction, uint8_t slot) const;
    uint8_t firstEnabledSlot(ScanDirection direction) const;
    uint8_t lastEnabledSlot(ScanDirection direction) const;
    bool nextEnabledSlot(ScanDirection direction, uint8_t slot, uint8_t& nextSlot) const;
    void setIdleVisual(uint8_t slot);
    uint64_t activePointMask() const;
    uint64_t expectedReceiverMask(ScanDirection direction,
                                  uint8_t sourceTestIndex) const;
    uint64_t expectedSenderMask(ScanDirection direction,
                                uint8_t sourceTestIndex) const;
    uint8_t expectedDisplayReceiver(ScanDirection direction,
                                    uint8_t sourceTestIndex) const;
    static PointVisualStatus toVisual(ElectricalResult result);

    const CableProfile* profile_;
    ScanSource& source_;
    const CableMap* customMap_ = nullptr;
    RunState runState_ = RunState::Idle;
    ScanDirection direction_ = ScanDirection::AtoB;
    ScanDirection displayDirection_ = ScanDirection::AtoB;
    uint8_t currentSlot_ = 0;
    uint8_t completedMeasurements_ = 0;
    uint32_t lastStepMs_ = 0;
    uint32_t scanStepIntervalMs_ = kDemoStepIntervalMs;
    uint8_t scanSpeedPercent_ = kDefaultTestSpeedPercent;
    bool stopOnError_ = false;
    bool continuousMode_ = false;
    // In NON-STOP + HATADA DUR mode the failing cable remains visible until
    // START. The next START begins a clean cycle for the replacement cable.
    bool restartCycleOnStart_ = false;
    bool automaticReturnSweep_ = false;
    bool dirty_ = true;
    uint8_t displayTestIndexA_ = 0xFF;
    uint8_t displayTestIndexB_ = 0xFF;
    PointVisualStatus activeStatusA_ = PointVisualStatus::Untested;
    PointVisualStatus activeStatusB_ = PointVisualStatus::Untested;
    uint64_t displaySenderMask_ = 0ULL;
    uint64_t displayReceiverMask_ = 0ULL;
    ElectricalResult displayResult_ = ElectricalResult::NotMeasured;
    bool displayResistanceValid_ = false;
    float displayResistanceMilliOhm_ = 0.0f;
    PointVisualStatus visualA_[kTestPointsPerSide]{};
    PointVisualStatus visualB_[kTestPointsPerSide]{};
    Measurement reportAtoB_[kTestPointsPerSide]{};
    Measurement reportBtoA_[kTestPointsPerSide]{};
};

const char* runStateText(RunState state);
const char* electricalResultText(ElectricalResult result);

}  // namespace mg::p4
