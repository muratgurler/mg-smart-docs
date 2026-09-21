#include "HighSpeedBackboneCore.h"

#include <stdio.h>

namespace mg::p4 {
namespace {

uint8_t clampNode(uint8_t node) {
    // Current operator UI exposes normal signal nodes A1..A62. PE=0 and dS=63
    // remain reserved/special even though the final RF tree is 64:1.
    if (node < 1U) return 1U;
    if (node > 62U) return 62U;
    return node;
}

uint8_t clampPair(uint8_t pairIndex) {
    if (pairIndex < 1U) return 1U;
    if (pairIndex > 32U) return 32U;
    return pairIndex;
}

uint32_t sanitizeFrequency(uint32_t hz) {
    switch (hz) {
        case 20000U:
        case 50000U:
        case 100000U:
        case 250000U:
            return hz;
        default:
            return 100000U;
    }
}

const char* tdrFaultText(TdrFaultType fault) {
    switch (fault) {
        case TdrFaultType::Open: return "OPEN";
        case TdrFaultType::Short: return "SHORT";
        case TdrFaultType::Unknown: default: return "UNKNOWN";
    }
}

const char* orientationText(UsbCOrientation orientation) {
    switch (orientation) {
        case UsbCOrientation::Cc1: return "CC1";
        case UsbCOrientation::Cc2: return "CC2";
        case UsbCOrientation::Unknown: default: return "?";
    }
}

}  // namespace

const char* highSpeedVerdictText(HighSpeedVerdict verdict) {
    switch (verdict) {
        case HighSpeedVerdict::Pass: return "PASS";
        case HighSpeedVerdict::Warning: return "WARNING";
        case HighSpeedVerdict::Fail: return "FAIL";
        case HighSpeedVerdict::NotRun: default: return "NOT RUN";
    }
}

void TdrDemoEngine::reset() {
    state_ = HighSpeedWorkflowState::Idle;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    result_ = TdrResult{};
    result_.aNode = node_;
}

void TdrDemoEngine::setNode(uint8_t aNode) {
    node_ = clampNode(aNode);
    reset();
}

void TdrDemoEngine::start() {
    state_ = HighSpeedWorkflowState::Running;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    result_ = TdrResult{};
    result_.aNode = node_;
    result_.vop = 0.66f;
    result_.calibrationValid = true;
}

uint8_t TdrDemoEngine::progressPercent() const {
    if (state_ == HighSpeedWorkflowState::Idle) return 0U;
    if (state_ == HighSpeedWorkflowState::Complete ||
        state_ == HighSpeedWorkflowState::Warning ||
        state_ == HighSpeedWorkflowState::Fault) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 4U);
}

void TdrDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != HighSpeedWorkflowState::Running) return;
    if (lastStepMs_ == 0U) {
        lastStepMs_ = nowMs;
        return;
    }
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) ++phaseIndex_;
    if (phaseIndex_ >= 4U) finish();
}

void TdrDemoEngine::finish() {
    // Deterministic demo timing. Distance uses one-way line time after
    // calibrated fixture/launch-path subtraction. Real hardware will replace
    // these values with TDC7200 START/STOP data.
    result_.valid = true;
    result_.aNode = node_;
    result_.vop = 0.66f;
    result_.calibrationValid = true;
    result_.distanceM = 5.20f + static_cast<float>(node_ % 5U) * 0.06f;
    constexpr float speedOfLightMPerNs = 0.299792458f;
    result_.lineTimeNs = result_.distanceM / (result_.vop * speedOfLightMPerNs);
    result_.roundTripNs = result_.lineTimeNs * 2.0f;
    result_.fault = (node_ == 31U) ? TdrFaultType::Short : TdrFaultType::Open;
    result_.confidencePercent = result_.calibrationValid ? 94U : 70U;
    result_.verdict = result_.calibrationValid ? HighSpeedVerdict::Pass : HighSpeedVerdict::Warning;
    state_ = result_.verdict == HighSpeedVerdict::Pass
        ? HighSpeedWorkflowState::Complete : HighSpeedWorkflowState::Warning;
}

void TdrDemoEngine::formatResult(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    if (!result_.valid) {
        snprintf(output, outputSize, "A%u | no measurement", static_cast<unsigned>(node_));
        return;
    }
    snprintf(output, outputSize, "A%u %s %.2f m | %.1f ns RT | conf %u%%",
             static_cast<unsigned>(result_.aNode), tdrFaultText(result_.fault),
             static_cast<double>(result_.distanceM), static_cast<double>(result_.roundTripNs),
             static_cast<unsigned>(result_.confidencePercent));
}

void PairIntegrityDemoEngine::reset() {
    state_ = HighSpeedWorkflowState::Idle;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    result_ = PairIntegrityResult{};
    result_.pairIndex = pairIndex_;
    result_.frequencyHz = frequencyHz_;
}

void PairIntegrityDemoEngine::setPair(uint8_t pairIndex) {
    pairIndex_ = clampPair(pairIndex);
    reset();
}

void PairIntegrityDemoEngine::setFrequencyHz(uint32_t frequencyHz) {
    frequencyHz_ = sanitizeFrequency(frequencyHz);
    reset();
}

void PairIntegrityDemoEngine::start() {
    state_ = HighSpeedWorkflowState::Running;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    result_ = PairIntegrityResult{};
    result_.pairIndex = pairIndex_;
    result_.frequencyHz = frequencyHz_;
}

uint8_t PairIntegrityDemoEngine::progressPercent() const {
    if (state_ == HighSpeedWorkflowState::Idle) return 0U;
    if (state_ == HighSpeedWorkflowState::Complete ||
        state_ == HighSpeedWorkflowState::Warning ||
        state_ == HighSpeedWorkflowState::Fault) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 4U);
}

void PairIntegrityDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != HighSpeedWorkflowState::Running) return;
    if (lastStepMs_ == 0U) {
        lastStepMs_ = nowMs;
        return;
    }
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) ++phaseIndex_;
    if (phaseIndex_ >= 4U) finish();
}

void PairIntegrityDemoEngine::finish() {
    result_.valid = true;
    result_.pairIndex = pairIndex_;
    result_.frequencyHz = frequencyHz_;
    const float frequencyPenalty = frequencyHz_ == 250000U ? 0.025f :
                                   frequencyHz_ == 100000U ? 0.012f : 0.006f;
    result_.amplitudeRatio = 0.975f - frequencyPenalty - static_cast<float>(pairIndex_ % 4U) * 0.004f;
    result_.phaseDegrees = -1.6f - static_cast<float>(pairIndex_ % 6U) * 0.55f;
    result_.crosstalkDb = -42.0f + static_cast<float>(pairIndex_ % 5U) * 1.1f;
    // Pair 13 is deliberately a demo split-pair fault for UI/service testing.
    result_.splitPair = pairIndex_ == 13U;
    if (result_.splitPair) {
        result_.amplitudeRatio = 0.71f;
        result_.crosstalkDb = -18.5f;
        result_.verdict = HighSpeedVerdict::Fail;
        state_ = HighSpeedWorkflowState::Fault;
    } else if (result_.amplitudeRatio < 0.92f || result_.crosstalkDb > -25.0f) {
        result_.verdict = HighSpeedVerdict::Warning;
        state_ = HighSpeedWorkflowState::Warning;
    } else {
        result_.verdict = HighSpeedVerdict::Pass;
        state_ = HighSpeedWorkflowState::Complete;
    }
}

void PairIntegrityDemoEngine::formatResult(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    if (!result_.valid) {
        snprintf(output, outputSize, "Pair %u | %lu Hz | no result",
                 static_cast<unsigned>(pairIndex_), static_cast<unsigned long>(frequencyHz_));
        return;
    }
    snprintf(output, outputSize, "P%u A=%.3f phase=%+.1f deg XT=%.1f dB | %s%s",
             static_cast<unsigned>(result_.pairIndex), static_cast<double>(result_.amplitudeRatio),
             static_cast<double>(result_.phaseDegrees), static_cast<double>(result_.crosstalkDb),
             highSpeedVerdictText(result_.verdict), result_.splitPair ? " SPLIT-PAIR" : "");
}

void UsbCIdentityDemoEngine::reset() {
    state_ = HighSpeedWorkflowState::Idle;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    result_ = UsbCableIdentityResult{};
}

void UsbCIdentityDemoEngine::readCable() {
    reset();
    state_ = HighSpeedWorkflowState::Running;
}

uint8_t UsbCIdentityDemoEngine::progressPercent() const {
    if (state_ == HighSpeedWorkflowState::Idle) return 0U;
    if (result_.profileCompared) return 100U;
    if (result_.valid) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 5U);
}

void UsbCIdentityDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != HighSpeedWorkflowState::Running) return;
    if (lastStepMs_ == 0U) {
        lastStepMs_ = nowMs;
        return;
    }
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) ++phaseIndex_;
    if (phaseIndex_ >= 4U) finishIdentity();
}

void UsbCIdentityDemoEngine::finishIdentity() {
    result_.valid = true;
    result_.attached = true;
    result_.orientation = UsbCOrientation::Cc1;
    result_.emarkerPresent = true;
    result_.declaredCurrentA = 5U;
    result_.eprCapable = true;
    result_.vconnRequired = true;
    result_.passiveCable = true;
    result_.profileCompared = false;
    result_.profileMatch = false;
    result_.vendorId = 0x1234U;
    result_.productId = 0x5678U;
    result_.verdict = HighSpeedVerdict::NotRun;
    state_ = HighSpeedWorkflowState::Complete;
}

void UsbCIdentityDemoEngine::compareProfile() {
    if (!result_.valid) return;
    phaseIndex_ = 5U;
    result_.profileCompared = true;
    // Demo profile expects a passive E-Marked 5 A / EPR capable cable.
    result_.profileMatch = result_.emarkerPresent && result_.declaredCurrentA == 5U &&
                           result_.eprCapable && result_.vconnRequired && result_.passiveCable;
    result_.verdict = result_.profileMatch ? HighSpeedVerdict::Pass : HighSpeedVerdict::Fail;
    state_ = result_.profileMatch ? HighSpeedWorkflowState::Complete : HighSpeedWorkflowState::Fault;
}

void UsbCIdentityDemoEngine::formatIdentity(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    if (!result_.valid) {
        snprintf(output, outputSize, "No cable identity");
        return;
    }
    snprintf(output, outputSize, "%s | E-Marker %s | %uA | EPR %s | VCONN %s",
             orientationText(result_.orientation), result_.emarkerPresent ? "YES" : "NO",
             static_cast<unsigned>(result_.declaredCurrentA), result_.eprCapable ? "YES" : "NO",
             result_.vconnRequired ? "YES" : "NO");
}

void UsbCIdentityDemoEngine::formatProfile(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    if (!result_.valid) {
        snprintf(output, outputSize, "READ CABLE FIRST");
    } else if (!result_.profileCompared) {
        snprintf(output, outputSize, "IDENTITY READY | PROFILE COMPARE PENDING");
    } else {
        snprintf(output, outputSize, "PROFILE %s | %s",
                 result_.profileMatch ? "MATCH" : "MISMATCH", highSpeedVerdictText(result_.verdict));
    }
}

void FlexGlitchDemoEngine::reset() {
    state_ = HighSpeedWorkflowState::Idle;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    snapshot_ = FlexGlitchSnapshot{};
}

void FlexGlitchDemoEngine::start() {
    state_ = HighSpeedWorkflowState::Running;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    snapshot_.armed = true;
}

void FlexGlitchDemoEngine::pause() {
    if (state_ == HighSpeedWorkflowState::Running) state_ = HighSpeedWorkflowState::Paused;
}

void FlexGlitchDemoEngine::resume() {
    if (state_ == HighSpeedWorkflowState::Paused) {
        state_ = HighSpeedWorkflowState::Running;
        lastStepMs_ = 0U;
    }
}

void FlexGlitchDemoEngine::pauseResume() {
    if (state_ == HighSpeedWorkflowState::Running) pause();
    else if (state_ == HighSpeedWorkflowState::Paused) resume();
    else start();
}

void FlexGlitchDemoEngine::clearLatch() {
    snapshot_.latched = false;
    if (state_ == HighSpeedWorkflowState::Idle) snapshot_.armed = false;
}

uint8_t FlexGlitchDemoEngine::progressPercent() const {
    if (state_ == HighSpeedWorkflowState::Idle) return 0U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 4U);
}

void FlexGlitchDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != HighSpeedWorkflowState::Running) return;
    if (lastStepMs_ == 0U) {
        lastStepMs_ = nowMs;
        return;
    }
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) ++phaseIndex_;
    if (phaseIndex_ == 3U && !snapshot_.latched) captureDemoEvent(nowMs);
    if (phaseIndex_ >= 4U) phaseIndex_ = 1U;  // keep monitoring after one UI cycle
}

void FlexGlitchDemoEngine::captureDemoEvent(uint32_t nowMs) {
    snapshot_.latched = true;
    ++snapshot_.eventCount;
    snapshot_.lastEvent.valid = true;
    snapshot_.lastEvent.eventNumber = snapshot_.eventCount;
    snapshot_.lastEvent.timestampMs = nowMs;
    snapshot_.lastEvent.durationUs = 42U + (snapshot_.eventCount - 1U) * 7U;
    snapshot_.lastEvent.netName = "A17-B17";
    if (snapshot_.shortestDurationUs == 0U ||
        snapshot_.lastEvent.durationUs < snapshot_.shortestDurationUs) {
        snapshot_.shortestDurationUs = snapshot_.lastEvent.durationUs;
    }
}

void FlexGlitchDemoEngine::formatLastResult(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    if (!snapshot_.lastEvent.valid) {
        snprintf(output, outputSize, "No captured dropout");
        return;
    }
    snprintf(output, outputSize, "#%lu %s | %lu us | t=%lu ms",
             static_cast<unsigned long>(snapshot_.lastEvent.eventNumber), snapshot_.lastEvent.netName,
             static_cast<unsigned long>(snapshot_.lastEvent.durationUs),
             static_cast<unsigned long>(snapshot_.lastEvent.timestampMs));
}

void FlexGlitchDemoEngine::formatEventList(char* output, size_t outputSize) const {
    if (outputSize == 0U) return;
    snprintf(output, outputSize, "events=%lu | shortest=%lu us | latch=%s",
             static_cast<unsigned long>(snapshot_.eventCount),
             static_cast<unsigned long>(snapshot_.shortestDurationUs),
             snapshot_.latched ? "SET" : "CLEAR");
}

void HighSpeedBackboneCore::resetAll() {
    tdr_.reset();
    pair_.reset();
    usbC_.reset();
    flexGlitch_.reset();
}

void HighSpeedBackboneCore::tick(uint32_t nowMs) {
    tdr_.tick(nowMs);
    pair_.tick(nowMs);
    usbC_.tick(nowMs);
    flexGlitch_.tick(nowMs);
}

}  // namespace mg::p4
