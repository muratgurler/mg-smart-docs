#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mg::p4 {

enum class QualityWorkflowState : uint8_t {
    Idle,
    Running,
    Complete,
    Warning,
    Fault,
};

enum class QualityVerdict : uint8_t {
    NotRun,
    Pass,
    Warning,
    Fail,
    NotApplicable,
};

struct KelvinResult {
    bool valid = false;
    uint16_t currentMa = 10U;
    float dutMicrovolts = 0.0f;
    float shuntMillivolts = 0.0f;
    float resistanceMilliOhm = 0.0f;
    float masterMilliOhm = 0.0f;
    float deltaMilliOhm = 0.0f;
    float ambientC = 23.6f;
    float normalizedEstimateMilliOhm = 0.0f;
    QualityVerdict verdict = QualityVerdict::NotRun;
};

// Demo engine for the future ADS122C04 + DAC80501 + OPA197 Kelvin path.
// It models the production workflow and data ownership without claiming that
// analog hardware exists on the current UI-only prototype.
class KelvinQualityDemoEngine {
public:
    void reset();
    void setCurrentMa(uint16_t currentMa);
    uint16_t currentMa() const { return currentMa_; }
    void setAmbientC(float ambientC) { ambientC_ = ambientC; }
    void zero();
    void measure();
    void tick(uint32_t nowMs, uint32_t intervalMs = 260U);

    QualityWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    bool zeroValid() const { return zeroValid_; }
    float dutOffsetUv() const { return dutOffsetUv_; }
    float shuntOffsetUv() const { return shuntOffsetUv_; }
    const KelvinResult& result() const { return result_; }
    void formatReference(char* output, size_t outputSize) const;

private:
    void finishMeasurement();

    QualityWorkflowState state_ = QualityWorkflowState::Idle;
    uint16_t currentMa_ = 10U;
    float ambientC_ = 23.6f;
    bool zeroValid_ = false;
    float dutOffsetUv_ = 3.2f;
    float shuntOffsetUv_ = -1.1f;
    uint8_t phaseIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
    KelvinResult result_{};
};

enum class PeDsProfileMode : uint8_t {
    Separate = 0,
    Bonded,
    AOnly,
    BOnly,
};

enum class BondPolicy : uint8_t {
    Required = 0,
    Allowed,
    Forbidden,
};

struct PeDsResult {
    bool valid = false;
    PeDsProfileMode mode = PeDsProfileMode::Separate;
    BondPolicy bondPolicy = BondPolicy::Forbidden;
    bool peContinuity = false;
    bool dsAConnected = false;
    bool dsBConnected = false;
    bool bondDetected = false;
    float peMilliOhm = 0.0f;
    float dsMilliOhm = 0.0f;
    float bondMilliOhm = 0.0f;
    QualityVerdict verdict = QualityVerdict::NotRun;
};

class PeDsQualityDemoEngine {
public:
    void reset();
    void setMode(PeDsProfileMode mode) { mode_ = mode; }
    PeDsProfileMode mode() const { return mode_; }
    void cycleMode();
    BondPolicy bondPolicy() const { return bondPolicy_; }
    void cycleBondPolicy();
    void startTest();
    void startKelvin();
    void tick(uint32_t nowMs, uint32_t intervalMs = 300U);

    QualityWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    const PeDsResult& result() const { return result_; }
    void formatMode(char* output, size_t outputSize) const;
    void formatBondPolicy(char* output, size_t outputSize) const;

private:
    void evaluate();

    PeDsProfileMode mode_ = PeDsProfileMode::Separate;
    BondPolicy bondPolicy_ = BondPolicy::Forbidden;
    QualityWorkflowState state_ = QualityWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    bool kelvinOnly_ = false;
    uint32_t lastStepMs_ = 0U;
    PeDsResult result_{};
};

struct FixtureSpcSnapshot {
    bool valid = false;
    float fixtureHealthPercent = 100.0f;
    float selectedPinDriftMilliOhm = 0.0f;
    float processMeanMilliOhm = 0.0f;
    float processStdDevMilliOhm = 0.0f;
    float cpk = 0.0f;
    uint8_t repeatedAnomalyCount = 0U;
    bool productPass = true;
    bool processWarning = false;
    const char* selectedPin = "A12";
};

class FixtureSpcDemoEngine {
public:
    void reset();
    void startFixtureCheck();
    void tick(uint32_t nowMs, uint32_t intervalMs = 330U);
    void nextTrendPin();
    void toggleReportView() { reportView_ = !reportView_; }

    QualityWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    bool reportView() const { return reportView_; }
    const FixtureSpcSnapshot& snapshot() const { return snapshot_; }
    uint32_t baselineRevision() const { return baselineRevision_; }
    void formatTrend(char* output, size_t outputSize) const;
    void formatReport(char* output, size_t outputSize) const;
    void formatMasterReference(char* output, size_t outputSize) const;

private:
    void calculateStatistics();
    void refreshSelectedPin();

    QualityWorkflowState state_ = QualityWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    uint8_t selectedTrendPin_ = 0U;
    bool reportView_ = false;
    uint32_t lastStepMs_ = 0U;
    uint32_t baselineRevision_ = 3U;
    FixtureSpcSnapshot snapshot_{};
};

struct EnvironmentSample {
    bool valid = false;
    float temperatureC = 23.6f;
    float humidityRh = 48.0f;
    uint32_t sampleNumber = 0U;
};

struct WireSpecDemo {
    const char* label = "1.0 mm2 Cu / 4.0 m";
    float areaMm2 = 1.0f;
    float lengthM = 4.0f;
    bool copper = true;
    bool known = true;
};

class EnvironmentQualityDemoEngine {
public:
    void reset();
    void measure();
    void toggleLive();
    void toggleLogging();
    void cycleWireSpec();
    void tick(uint32_t nowMs, uint32_t intervalMs = 1200U);

    bool live() const { return live_; }
    bool logging() const { return logging_; }
    uint32_t logCount() const { return logCount_; }
    const EnvironmentSample& sample() const { return sample_; }
    const WireSpecDemo& wireSpec() const;
    float theoreticalWireMilliOhm() const;
    float normalizeCopperResistanceMilliOhm(float rawMilliOhm, float referenceC = 20.0f) const;
    void formatWireSpec(char* output, size_t outputSize) const;

private:
    void takeSample();

    bool live_ = false;
    bool logging_ = false;
    uint8_t wireSpecIndex_ = 0U;
    uint32_t lastSampleMs_ = 0U;
    uint32_t logCount_ = 0U;
    EnvironmentSample sample_{};
};

class QualityBackboneCore {
public:
    QualityBackboneCore();
    void resetAll();
    void tick(uint32_t nowMs);

    KelvinQualityDemoEngine& kelvin() { return kelvin_; }
    const KelvinQualityDemoEngine& kelvin() const { return kelvin_; }
    PeDsQualityDemoEngine& peDs() { return peDs_; }
    const PeDsQualityDemoEngine& peDs() const { return peDs_; }
    FixtureSpcDemoEngine& fixtureSpc() { return fixtureSpc_; }
    const FixtureSpcDemoEngine& fixtureSpc() const { return fixtureSpc_; }
    EnvironmentQualityDemoEngine& environment() { return environment_; }
    const EnvironmentQualityDemoEngine& environment() const { return environment_; }

private:
    KelvinQualityDemoEngine kelvin_{};
    PeDsQualityDemoEngine peDs_{};
    FixtureSpcDemoEngine fixtureSpc_{};
    EnvironmentQualityDemoEngine environment_{};
};

const char* qualityVerdictText(QualityVerdict verdict);

}  // namespace mg::p4
