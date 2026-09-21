#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mg::p4 {

enum class HighSpeedWorkflowState : uint8_t {
    Idle,
    Running,
    Paused,
    Complete,
    Warning,
    Fault,
};

enum class HighSpeedVerdict : uint8_t {
    NotRun,
    Pass,
    Warning,
    Fail,
};

const char* highSpeedVerdictText(HighSpeedVerdict verdict);

enum class TdrFaultType : uint8_t {
    Unknown,
    Open,
    Short,
};

struct TdrResult {
    bool valid = false;
    uint8_t aNode = 17U;
    float vop = 0.66f;
    bool calibrationValid = true;
    float roundTripNs = 0.0f;
    float lineTimeNs = 0.0f;
    float distanceM = 0.0f;
    TdrFaultType fault = TdrFaultType::Unknown;
    uint8_t confidencePercent = 0U;
    HighSpeedVerdict verdict = HighSpeedVerdict::NotRun;
};

// Demo model for the future ADG904/TDC7200/TLV3601 TDR path.
// No pulse output is driven here. A hardware driver will later supply the
// captured timing while this engine/UI contract remains unchanged.
class TdrDemoEngine {
public:
    void reset();
    void setNode(uint8_t aNode);
    uint8_t node() const { return node_; }
    void start();
    void tick(uint32_t nowMs, uint32_t intervalMs = 260U);

    HighSpeedWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    const TdrResult& result() const { return result_; }
    void formatResult(char* output, size_t outputSize) const;

private:
    void finish();

    uint8_t node_ = 17U;
    HighSpeedWorkflowState state_ = HighSpeedWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
    TdrResult result_{};
};

struct PairIntegrityResult {
    bool valid = false;
    uint8_t pairIndex = 1U;
    uint32_t frequencyHz = 100000U;
    float amplitudeRatio = 1.0f;
    float phaseDegrees = 0.0f;
    float crosstalkDb = -40.0f;
    bool splitPair = false;
    HighSpeedVerdict verdict = HighSpeedVerdict::NotRun;
};

// Demo model for AD9833 -> THS4551 -> routing -> THS4551 -> ADS8681.
// This is a production comparison/trend aid, not CAT6 certification.
class PairIntegrityDemoEngine {
public:
    void reset();
    void setPair(uint8_t pairIndex);
    uint8_t pair() const { return pairIndex_; }
    void setFrequencyHz(uint32_t frequencyHz);
    uint32_t frequencyHz() const { return frequencyHz_; }
    void start();
    void tick(uint32_t nowMs, uint32_t intervalMs = 280U);

    HighSpeedWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    const PairIntegrityResult& result() const { return result_; }
    void formatResult(char* output, size_t outputSize) const;

private:
    void finish();

    uint8_t pairIndex_ = 1U;
    uint32_t frequencyHz_ = 100000U;
    HighSpeedWorkflowState state_ = HighSpeedWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
    PairIntegrityResult result_{};
};

enum class UsbCOrientation : uint8_t {
    Unknown,
    Cc1,
    Cc2,
};

struct UsbCableIdentityResult {
    bool valid = false;
    bool attached = false;
    UsbCOrientation orientation = UsbCOrientation::Unknown;
    bool emarkerPresent = false;
    uint8_t declaredCurrentA = 0U;
    bool eprCapable = false;
    bool vconnRequired = false;
    bool passiveCable = true;
    bool profileCompared = false;
    bool profileMatch = false;
    uint16_t vendorId = 0U;
    uint16_t productId = 0U;
    HighSpeedVerdict verdict = HighSpeedVerdict::NotRun;
};

// Low-power cable identity flow. The demo deliberately models only safe
// VBUS/VCONN + SOP' identity discovery; it never models a 5 A/EPR load test.
class UsbCIdentityDemoEngine {
public:
    void reset();
    void readCable();
    void compareProfile();
    void tick(uint32_t nowMs, uint32_t intervalMs = 300U);

    HighSpeedWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    const UsbCableIdentityResult& result() const { return result_; }
    void formatIdentity(char* output, size_t outputSize) const;
    void formatProfile(char* output, size_t outputSize) const;

private:
    void finishIdentity();

    HighSpeedWorkflowState state_ = HighSpeedWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
    UsbCableIdentityResult result_{};
};

struct FlexGlitchEvent {
    bool valid = false;
    uint32_t eventNumber = 0U;
    uint32_t timestampMs = 0U;
    uint32_t durationUs = 0U;
    const char* netName = "A17-B17";
};

struct FlexGlitchSnapshot {
    bool armed = false;
    bool latched = false;
    uint32_t eventCount = 0U;
    uint32_t shortestDurationUs = 0U;
    FlexGlitchEvent lastEvent{};
};

// Demo model for TLV3601 -> SN74LVC1G74 hardware latch -> GPIO46.
// Clearing the latch does not erase the recorded event history.
class FlexGlitchDemoEngine {
public:
    void reset();
    void pauseResume();
    void start();
    void pause();
    void resume();
    void clearLatch();
    void tick(uint32_t nowMs, uint32_t intervalMs = 350U);

    HighSpeedWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    const FlexGlitchSnapshot& snapshot() const { return snapshot_; }
    void formatLastResult(char* output, size_t outputSize) const;
    void formatEventList(char* output, size_t outputSize) const;

private:
    void captureDemoEvent(uint32_t nowMs);

    HighSpeedWorkflowState state_ = HighSpeedWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
    FlexGlitchSnapshot snapshot_{};
};

// High-speed/special-measurement backbone group. The UI talks only to these
// stable module contracts. When the carrier PCB exists, the demo acquisition
// internals can be replaced by real TDC/SPI/PD/latch drivers without changing
// menu navigation or operator workflow.
class HighSpeedBackboneCore {
public:
    HighSpeedBackboneCore() { resetAll(); }

    void resetAll();
    void tick(uint32_t nowMs);

    TdrDemoEngine& tdr() { return tdr_; }
    const TdrDemoEngine& tdr() const { return tdr_; }
    PairIntegrityDemoEngine& pair() { return pair_; }
    const PairIntegrityDemoEngine& pair() const { return pair_; }
    UsbCIdentityDemoEngine& usbC() { return usbC_; }
    const UsbCIdentityDemoEngine& usbC() const { return usbC_; }
    FlexGlitchDemoEngine& flexGlitch() { return flexGlitch_; }
    const FlexGlitchDemoEngine& flexGlitch() const { return flexGlitch_; }

private:
    TdrDemoEngine tdr_{};
    PairIntegrityDemoEngine pair_{};
    UsbCIdentityDemoEngine usbC_{};
    FlexGlitchDemoEngine flexGlitch_{};
};

}  // namespace mg::p4
