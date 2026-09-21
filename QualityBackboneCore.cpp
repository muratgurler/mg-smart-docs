#include "QualityBackboneCore.h"

#include <math.h>
#include <stdio.h>

namespace mg::p4 {
namespace {

float clampFloat(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

}  // namespace

const char* qualityVerdictText(QualityVerdict verdict) {
    switch (verdict) {
        case QualityVerdict::Pass: return "PASS";
        case QualityVerdict::Warning: return "WARNING";
        case QualityVerdict::Fail: return "FAIL";
        case QualityVerdict::NotApplicable: return "N/A";
        case QualityVerdict::NotRun: default: return "NOT RUN";
    }
}

void KelvinQualityDemoEngine::reset() {
    state_ = QualityWorkflowState::Idle;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    zeroValid_ = false;
    dutOffsetUv_ = 3.2f;
    shuntOffsetUv_ = -1.1f;
    result_ = KelvinResult{};
    result_.currentMa = currentMa_;
    result_.ambientC = ambientC_;
}

void KelvinQualityDemoEngine::setCurrentMa(uint16_t currentMa) {
    if (currentMa <= 10U) currentMa_ = 10U;
    else if (currentMa <= 50U) currentMa_ = 50U;
    else currentMa_ = 100U;
}

void KelvinQualityDemoEngine::zero() {
    // In hardware this will average ADC internal-short + DUT current-off paths.
    zeroValid_ = true;
    dutOffsetUv_ = 0.4f;
    shuntOffsetUv_ = 0.2f;
    state_ = QualityWorkflowState::Complete;
    phaseIndex_ = 0U;
    result_.valid = false;
}

void KelvinQualityDemoEngine::measure() {
    state_ = QualityWorkflowState::Running;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    result_ = KelvinResult{};
    result_.currentMa = currentMa_;
    result_.ambientC = ambientC_;
}

uint8_t KelvinQualityDemoEngine::progressPercent() const {
    if (state_ == QualityWorkflowState::Idle) return 0U;
    if (state_ == QualityWorkflowState::Complete ||
        state_ == QualityWorkflowState::Warning ||
        state_ == QualityWorkflowState::Fault) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 4U);
}

void KelvinQualityDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != QualityWorkflowState::Running) return;
    if (lastStepMs_ == 0U) {
        lastStepMs_ = nowMs;
        return;
    }
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) {
        ++phaseIndex_;
        if (phaseIndex_ == 4U) finishMeasurement();
    }
}

void KelvinQualityDemoEngine::finishMeasurement() {
    // Deterministic values are chosen to exercise the production data model.
    // Rshunt=1.000 ohm -> Vshunt[mV] equals I[mA].
    const float baseR = currentMa_ == 10U ? 8.44f : (currentMa_ == 50U ? 8.39f : 8.37f);
    const float master = 8.15f;
    result_.valid = true;
    result_.currentMa = currentMa_;
    result_.ambientC = ambientC_;
    result_.shuntMillivolts = static_cast<float>(currentMa_);
    result_.resistanceMilliOhm = baseR;
    result_.masterMilliOhm = master;
    result_.deltaMilliOhm = baseR - master;
    result_.dutMicrovolts = baseR * static_cast<float>(currentMa_);
    const float factor = 1.0f + 0.00393f * (ambientC_ - 20.0f);
    result_.normalizedEstimateMilliOhm = factor > 0.1f ? baseR / factor : baseR;
    if (!zeroValid_) {
        result_.verdict = QualityVerdict::Warning;
        state_ = QualityWorkflowState::Warning;
    } else if (result_.deltaMilliOhm >= 0.50f) {
        result_.verdict = QualityVerdict::Fail;
        state_ = QualityWorkflowState::Fault;
    } else if (result_.deltaMilliOhm >= 0.35f) {
        result_.verdict = QualityVerdict::Warning;
        state_ = QualityWorkflowState::Warning;
    } else {
        result_.verdict = QualityVerdict::Pass;
        state_ = QualityWorkflowState::Complete;
    }
}

void KelvinQualityDemoEngine::formatReference(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    if (!result_.valid) {
        snprintf(output, outputSize, "Master 8.15 mOhm | no measurement yet");
        return;
    }
    snprintf(output, outputSize, "Master %.2f mOhm | dR %+.2f mOhm | %s",
             static_cast<double>(result_.masterMilliOhm),
             static_cast<double>(result_.deltaMilliOhm),
             qualityVerdictText(result_.verdict));
}

void PeDsQualityDemoEngine::reset() {
    state_ = QualityWorkflowState::Idle;
    phaseIndex_ = 0U;
    kelvinOnly_ = false;
    lastStepMs_ = 0U;
    result_ = PeDsResult{};
    result_.mode = mode_;
    result_.bondPolicy = bondPolicy_;
}

void PeDsQualityDemoEngine::cycleMode() {
    mode_ = static_cast<PeDsProfileMode>((static_cast<uint8_t>(mode_) + 1U) % 4U);
    reset();
}

void PeDsQualityDemoEngine::cycleBondPolicy() {
    bondPolicy_ = static_cast<BondPolicy>((static_cast<uint8_t>(bondPolicy_) + 1U) % 3U);
    reset();
}

void PeDsQualityDemoEngine::startTest() {
    kelvinOnly_ = false;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    state_ = QualityWorkflowState::Running;
    result_ = PeDsResult{};
    result_.mode = mode_;
    result_.bondPolicy = bondPolicy_;
}

void PeDsQualityDemoEngine::startKelvin() {
    kelvinOnly_ = true;
    phaseIndex_ = 3U;  // jump to KELVIN QUALITY phase in the shared screen
    lastStepMs_ = 0U;
    state_ = QualityWorkflowState::Running;
    result_ = PeDsResult{};
    result_.mode = mode_;
    result_.bondPolicy = bondPolicy_;
}

uint8_t PeDsQualityDemoEngine::progressPercent() const {
    if (state_ == QualityWorkflowState::Idle) return 0U;
    if (state_ == QualityWorkflowState::Complete ||
        state_ == QualityWorkflowState::Warning ||
        state_ == QualityWorkflowState::Fault) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 4U);
}

void PeDsQualityDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != QualityWorkflowState::Running) return;
    if (lastStepMs_ == 0U) {
        lastStepMs_ = nowMs;
        return;
    }
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) ++phaseIndex_;
    if (phaseIndex_ >= 4U) evaluate();
}

void PeDsQualityDemoEngine::evaluate() {
    result_.valid = true;
    result_.mode = mode_;
    result_.bondPolicy = bondPolicy_;
    result_.peContinuity = true;
    result_.peMilliOhm = 6.80f;
    result_.bondMilliOhm = 1.10f;
    result_.dsMilliOhm = 11.40f;

    switch (mode_) {
        case PeDsProfileMode::Separate:
            result_.dsAConnected = true;
            result_.dsBConnected = true;
            result_.bondDetected = false;
            break;
        case PeDsProfileMode::Bonded:
            result_.dsAConnected = true;
            result_.dsBConnected = true;
            result_.bondDetected = true;
            result_.dsMilliOhm = 6.95f;
            break;
        case PeDsProfileMode::AOnly:
            result_.dsAConnected = true;
            result_.dsBConnected = false;
            result_.bondDetected = false;
            result_.dsMilliOhm = 0.0f;
            break;
        case PeDsProfileMode::BOnly:
            result_.dsAConnected = false;
            result_.dsBConnected = true;
            result_.bondDetected = false;
            result_.dsMilliOhm = 0.0f;
            break;
    }

    bool bondOk = true;
    if (bondPolicy_ == BondPolicy::Required) bondOk = result_.bondDetected;
    if (bondPolicy_ == BondPolicy::Forbidden) bondOk = !result_.bondDetected;

    if (!result_.peContinuity || !bondOk) {
        result_.verdict = QualityVerdict::Fail;
        state_ = QualityWorkflowState::Fault;
    } else if (kelvinOnly_ && (mode_ == PeDsProfileMode::AOnly || mode_ == PeDsProfileMode::BOnly)) {
        result_.verdict = QualityVerdict::NotApplicable;
        state_ = QualityWorkflowState::Complete;
    } else {
        result_.verdict = QualityVerdict::Pass;
        state_ = QualityWorkflowState::Complete;
    }
}

void PeDsQualityDemoEngine::formatMode(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    const char* text = "SEPARATE";
    switch (mode_) {
        case PeDsProfileMode::Separate: text = "SEPARATE"; break;
        case PeDsProfileMode::Bonded: text = "BONDED"; break;
        case PeDsProfileMode::AOnly: text = "A ONLY"; break;
        case PeDsProfileMode::BOnly: text = "B ONLY"; break;
    }
    snprintf(output, outputSize, "%s", text);
}

void PeDsQualityDemoEngine::formatBondPolicy(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    const char* text = bondPolicy_ == BondPolicy::Required ? "REQUIRED"
                     : bondPolicy_ == BondPolicy::Allowed ? "ALLOWED" : "FORBIDDEN";
    snprintf(output, outputSize, "%s", text);
}

void FixtureSpcDemoEngine::reset() {
    state_ = QualityWorkflowState::Idle;
    phaseIndex_ = 0U;
    selectedTrendPin_ = 0U;
    reportView_ = false;
    lastStepMs_ = 0U;
    snapshot_ = FixtureSpcSnapshot{};
    calculateStatistics();
    refreshSelectedPin();
}

void FixtureSpcDemoEngine::startFixtureCheck() {
    state_ = QualityWorkflowState::Running;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    calculateStatistics();
    refreshSelectedPin();
}

uint8_t FixtureSpcDemoEngine::progressPercent() const {
    if (state_ == QualityWorkflowState::Idle) return 0U;
    if (state_ == QualityWorkflowState::Complete ||
        state_ == QualityWorkflowState::Warning ||
        state_ == QualityWorkflowState::Fault) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 4U);
}

void FixtureSpcDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != QualityWorkflowState::Running) return;
    if (lastStepMs_ == 0U) {
        lastStepMs_ = nowMs;
        return;
    }
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) ++phaseIndex_;
    if (phaseIndex_ >= 4U) {
        calculateStatistics();
        refreshSelectedPin();
        state_ = snapshot_.fixtureHealthPercent < 90.0f ? QualityWorkflowState::Fault
               : snapshot_.processWarning ? QualityWorkflowState::Warning
               : QualityWorkflowState::Complete;
    }
}

void FixtureSpcDemoEngine::nextTrendPin() {
    selectedTrendPin_ = static_cast<uint8_t>((selectedTrendPin_ + 1U) % 4U);
    refreshSelectedPin();
}

void FixtureSpcDemoEngine::calculateStatistics() {
    // Stable demo dataset: production records do not modify the master baseline.
    static const float samples[] = {8.04f, 8.12f, 8.16f, 8.19f, 8.20f, 8.23f,
                                    8.10f, 8.31f, 8.15f, 8.22f, 8.17f, 8.25f};
    const size_t count = sizeof(samples) / sizeof(samples[0]);
    float sum = 0.0f;
    for (size_t i = 0; i < count; ++i) sum += samples[i];
    const float mean = sum / static_cast<float>(count);
    float sq = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        const float d = samples[i] - mean;
        sq += d * d;
    }
    const float stddev = sqrtf(sq / static_cast<float>(count - 1U));
    const float usl = 8.70f;  // demo upper process limit for this net/profile
    const float cpk = stddev > 0.0001f ? (usl - mean) / (3.0f * stddev) : 99.0f;

    snapshot_.valid = true;
    snapshot_.fixtureHealthPercent = 96.0f;
    snapshot_.processMeanMilliOhm = mean;
    snapshot_.processStdDevMilliOhm = stddev;
    snapshot_.cpk = clampFloat(cpk, 0.0f, 9.99f);
    snapshot_.repeatedAnomalyCount = 2U;
    snapshot_.productPass = true;
    snapshot_.processWarning = true;
}

void FixtureSpcDemoEngine::refreshSelectedPin() {
    static const char* pins[] = {"A12", "B07", "PE", "dS"};
    static const float drift[] = {0.60f, 0.18f, 0.09f, 0.22f};
    snapshot_.selectedPin = pins[selectedTrendPin_ % 4U];
    snapshot_.selectedPinDriftMilliOhm = drift[selectedTrendPin_ % 4U];
}

void FixtureSpcDemoEngine::formatTrend(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    snprintf(output, outputSize, "%s drift %+.2f mOhm | repeat=%u | Cpk=%.2f",
             snapshot_.selectedPin,
             static_cast<double>(snapshot_.selectedPinDriftMilliOhm),
             static_cast<unsigned>(snapshot_.repeatedAnomalyCount),
             static_cast<double>(snapshot_.cpk));
}

void FixtureSpcDemoEngine::formatReport(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    snprintf(output, outputSize, "PRODUCT=%s | PROCESS=%s | fixture %.0f%%",
             snapshot_.productPass ? "PASS" : "FAIL",
             snapshot_.processWarning ? "WARNING" : "OK",
             static_cast<double>(snapshot_.fixtureHealthPercent));
}

void FixtureSpcDemoEngine::formatMasterReference(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    snprintf(output, outputSize,
             "Master baseline rev %lu | production DUT never auto-updates baseline",
             static_cast<unsigned long>(baselineRevision_));
}

void EnvironmentQualityDemoEngine::reset() {
    live_ = false;
    logging_ = false;
    wireSpecIndex_ = 0U;
    lastSampleMs_ = 0U;
    logCount_ = 0U;
    sample_ = EnvironmentSample{};
}

void EnvironmentQualityDemoEngine::takeSample() {
    ++sample_.sampleNumber;
    const int cycle = static_cast<int>(sample_.sampleNumber % 5U) - 2;
    sample_.temperatureC = 23.6f + static_cast<float>(cycle) * 0.08f;
    sample_.humidityRh = 48.0f + static_cast<float>((sample_.sampleNumber % 3U)) * 0.3f;
    sample_.valid = true;
    if (logging_) ++logCount_;
}

void EnvironmentQualityDemoEngine::measure() {
    takeSample();
}

void EnvironmentQualityDemoEngine::toggleLive() {
    live_ = !live_;
}

void EnvironmentQualityDemoEngine::toggleLogging() {
    logging_ = !logging_;
}

void EnvironmentQualityDemoEngine::cycleWireSpec() {
    wireSpecIndex_ = static_cast<uint8_t>((wireSpecIndex_ + 1U) % 3U);
}

void EnvironmentQualityDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (!live_) return;
    if (lastSampleMs_ == 0U || nowMs - lastSampleMs_ >= intervalMs) {
        lastSampleMs_ = nowMs;
        takeSample();
    }
}

const WireSpecDemo& EnvironmentQualityDemoEngine::wireSpec() const {
    static const WireSpecDemo specs[] = {
        {"1.0 mm2 Cu / 4.0 m", 1.0f, 4.0f, true, true},
        {"4.0 mm2 Cu / 4.0 m", 4.0f, 4.0f, true, true},
        {"UNKNOWN / profile optional", 0.0f, 0.0f, false, false},
    };
    return specs[wireSpecIndex_ % 3U];
}

float EnvironmentQualityDemoEngine::theoreticalWireMilliOhm() const {
    const WireSpecDemo& spec = wireSpec();
    if (!spec.known || !spec.copper || spec.areaMm2 <= 0.0f) return -1.0f;
    constexpr float rhoCuOhmMm2PerM = 0.01724f;
    return (rhoCuOhmMm2PerM * spec.lengthM / spec.areaMm2) * 1000.0f;
}

float EnvironmentQualityDemoEngine::normalizeCopperResistanceMilliOhm(float rawMilliOhm, float referenceC) const {
    if (!sample_.valid) return rawMilliOhm;
    constexpr float alphaCu = 0.00393f;
    const float factor = 1.0f + alphaCu * (sample_.temperatureC - referenceC);
    return factor > 0.1f ? rawMilliOhm / factor : rawMilliOhm;
}

void EnvironmentQualityDemoEngine::formatWireSpec(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    const WireSpecDemo& spec = wireSpec();
    const float theoretical = theoreticalWireMilliOhm();
    if (theoretical < 0.0f) {
        snprintf(output, outputSize, "%s | theoretical R disabled", spec.label);
    } else {
        snprintf(output, outputSize, "%s | rhoL/A ~= %.1f mOhm",
                 spec.label, static_cast<double>(theoretical));
    }
}

QualityBackboneCore::QualityBackboneCore() {
    resetAll();
}

void QualityBackboneCore::resetAll() {
    environment_.reset();
    environment_.measure();
    kelvin_.reset();
    kelvin_.setAmbientC(environment_.sample().temperatureC);
    peDs_.reset();
    fixtureSpc_.reset();
}

void QualityBackboneCore::tick(uint32_t nowMs) {
    environment_.tick(nowMs);
    kelvin_.setAmbientC(environment_.sample().temperatureC);
    kelvin_.tick(nowMs);
    peDs_.tick(nowMs);
    fixtureSpc_.tick(nowMs);
}

}  // namespace mg::p4
