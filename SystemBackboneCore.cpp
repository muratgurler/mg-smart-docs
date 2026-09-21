#include "SystemBackboneCore.h"

#include <stdio.h>
#include <string.h>

namespace mg::p4 {
namespace {

template <size_t N>
void copyText(char (&dst)[N], const char* src) {
    if (src == nullptr) src = "";
    snprintf(dst, N, "%s", src);
}

const char* linkText(NetworkLink link) {
    switch (link) {
        case NetworkLink::Ethernet: return "ETHERNET";
        case NetworkLink::WifiSta: return "WI-FI STA";
        case NetworkLink::Offline: default: return "OFFLINE";
    }
}

const char* codeKindText(CodeKind kind) {
    switch (kind) {
        case CodeKind::ProductionPr: return "PR";
        case CodeKind::CustomerReference: return "CUSTOMER";
        case CodeKind::MgProfile: return "MG PROFILE";
        case CodeKind::Unknown: default: return "UNKNOWN";
    }
}

const char* componentText(BasicComponentType type) {
    switch (type) {
        case BasicComponentType::Diode: return "DIODE";
        case BasicComponentType::Led: return "LED";
        case BasicComponentType::Capacitor: return "CAPACITOR";
    }
    return "?";
}

const char* domainText(HardwareValidationDomain domain) {
    switch (domain) {
        case HardwareValidationDomain::Gpio: return "GPIO";
        case HardwareValidationDomain::Bus: return "BUS";
        case HardwareValidationDomain::Boot: return "BOOT";
    }
    return "?";
}

}  // namespace

const char* systemVerdictText(SystemVerdict verdict) {
    switch (verdict) {
        case SystemVerdict::Pass: return "PASS";
        case SystemVerdict::Warning: return "WARNING";
        case SystemVerdict::Fail: return "FAIL";
        case SystemVerdict::Blocked: return "BLOCKED";
        case SystemVerdict::NotRun: default: return "NOT RUN";
    }
}

void NetworkPrDemoEngine::reset() {
    state_ = SystemWorkflowState::Idle;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    serverTested_ = false;
    result_ = {};
    result_.link = link_;
    result_.autoStartRequested = false;
}

void NetworkPrDemoEngine::cycleLink() {
    if (link_ == NetworkLink::Ethernet) link_ = NetworkLink::WifiSta;
    else if (link_ == NetworkLink::WifiSta) link_ = NetworkLink::Offline;
    else link_ = NetworkLink::Ethernet;
    reset();
}

void NetworkPrDemoEngine::testServer() {
    result_ = {};
    result_.link = link_;
    result_.serverReachable = link_ != NetworkLink::Offline;
    result_.valid = true;
    result_.autoStartRequested = false;
    result_.verdict = result_.serverReachable ? SystemVerdict::Pass : SystemVerdict::Warning;
    serverTested_ = true;
    state_ = result_.serverReachable ? SystemWorkflowState::Complete : SystemWorkflowState::Warning;
    phaseIndex_ = 4U;
}

void NetworkPrDemoEngine::lookupPr(const char* pr) {
    result_ = {};
    result_.link = link_;
    copyText(result_.pr, pr);
    result_.oldProfileCleared = true; // stale profile is invalidated before any lookup
    result_.autoStartRequested = false;
    state_ = SystemWorkflowState::Running;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
}

void NetworkPrDemoEngine::finishLookup() {
    result_.serverReachable = link_ != NetworkLink::Offline;
    if (!result_.serverReachable) {
        result_.valid = true;
        result_.profileReady = false;
        result_.verdict = SystemVerdict::Warning;
        state_ = SystemWorkflowState::Warning;
        return;
    }
    if (strncmp(result_.pr, "PR", 2U) != 0) {
        result_.valid = true;
        result_.profileReady = false;
        result_.verdict = SystemVerdict::Warning;
        state_ = SystemWorkflowState::Warning;
        return;
    }
    result_.valid = true;
    result_.profileReady = true;
    result_.autoStartRequested = false;
    copyText(result_.customerReference, "ASML-REF-DEMO-01");
    copyText(result_.revision, "R07");
    copyText(result_.profileId, "DEMO-DB25");
    result_.verdict = SystemVerdict::Pass;
    state_ = SystemWorkflowState::Complete;
}

void NetworkPrDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != SystemWorkflowState::Running) return;
    if (lastStepMs_ != 0U && (nowMs - lastStepMs_) < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) ++phaseIndex_;
    if (phaseIndex_ >= 4U) finishLookup();
}

uint8_t NetworkPrDemoEngine::progressPercent() const {
    if (state_ == SystemWorkflowState::Idle) return 0U;
    if (state_ == SystemWorkflowState::Complete || state_ == SystemWorkflowState::Warning || state_ == SystemWorkflowState::Fault) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 4U);
}

void NetworkPrDemoEngine::formatStatus(char* output, size_t outputSize) const {
    snprintf(output, outputSize, "%s | server=%s | free-cable=INDEPENDENT",
             linkText(link_), (link_ != NetworkLink::Offline || result_.serverReachable) ? "READY" : "OFFLINE");
}

void NetworkPrDemoEngine::formatCache(char* output, size_t outputSize) const {
    snprintf(output, outputSize, "3 validated profiles | revision-aware | no auto-start");
}

void BarcodeWorkflowDemoEngine::reset() {
    state_ = SystemWorkflowState::Idle;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    result_ = {};
    result_.autoStartRequested = false;
}

void BarcodeWorkflowDemoEngine::nextDemoCode() {
    demoCodeIndex_ = static_cast<uint8_t>((demoCodeIndex_ + 1U) % 4U);
    reset();
}

void BarcodeWorkflowDemoEngine::beginScan(const char* code, CodeKind kind) {
    result_ = {};
    copyText(result_.code, code);
    result_.kind = kind;
    result_.oldProfileCleared = true; // old profile is cleared before parsing/lookup
    result_.autoStartRequested = false;
    state_ = SystemWorkflowState::Running;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
}

void BarcodeWorkflowDemoEngine::scanCurrent() {
    switch (demoCodeIndex_ % 4U) {
        case 0U: beginScan("PR123456", CodeKind::ProductionPr); break;
        case 1U: beginScan("ASML-REF-98765", CodeKind::CustomerReference); break;
        case 2U: beginScan("MG:PROFILE:FREE-12", CodeKind::MgProfile); break;
        default: beginScan("UNKNOWN-4711", CodeKind::Unknown); break;
    }
}

void BarcodeWorkflowDemoEngine::scanAgain() {
    scanCurrent();
}

void BarcodeWorkflowDemoEngine::manualEntry(const char* code) {
    CodeKind kind = CodeKind::Unknown;
    if (strncmp(code, "PR", 2U) == 0) kind = CodeKind::ProductionPr;
    else if (strncmp(code, "MG", 2U) == 0) kind = CodeKind::MgProfile;
    else if (strncmp(code, "ASML", 4U) == 0) kind = CodeKind::CustomerReference;
    beginScan(code, kind);
}

void BarcodeWorkflowDemoEngine::finishScan() {
    result_.valid = true;
    result_.autoStartRequested = false;
    switch (result_.kind) {
        case CodeKind::ProductionPr:
            result_.accepted = true;
            result_.networkLookupRequired = true;
            result_.profileReady = true;
            copyText(result_.profileId, "DEMO-DB25");
            copyText(result_.revision, "R07");
            result_.verdict = SystemVerdict::Pass;
            state_ = SystemWorkflowState::Complete;
            break;
        case CodeKind::CustomerReference:
            result_.accepted = true;
            result_.networkLookupRequired = true;
            result_.profileReady = true;
            copyText(result_.profileId, "DEMO-HARTING-MIX");
            copyText(result_.revision, "R03");
            result_.verdict = SystemVerdict::Pass;
            state_ = SystemWorkflowState::Complete;
            break;
        case CodeKind::MgProfile:
            result_.accepted = true;
            result_.networkLookupRequired = false;
            result_.profileReady = true;
            copyText(result_.profileId, "MG-FREE-12");
            copyText(result_.revision, "LOCAL");
            result_.verdict = SystemVerdict::Pass;
            state_ = SystemWorkflowState::Complete;
            break;
        case CodeKind::Unknown:
        default:
            result_.accepted = false;
            result_.networkLookupRequired = false;
            result_.profileReady = false;
            result_.verdict = SystemVerdict::Warning;
            state_ = SystemWorkflowState::Warning;
            break;
    }
}

void BarcodeWorkflowDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != SystemWorkflowState::Running) return;
    if (lastStepMs_ != 0U && (nowMs - lastStepMs_) < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) ++phaseIndex_;
    if (phaseIndex_ >= 4U) finishScan();
}

uint8_t BarcodeWorkflowDemoEngine::progressPercent() const {
    if (state_ == SystemWorkflowState::Idle) return 0U;
    if (state_ == SystemWorkflowState::Complete || state_ == SystemWorkflowState::Warning || state_ == SystemWorkflowState::Fault) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 4U);
}

void BarcodeWorkflowDemoEngine::formatCurrentCode(char* output, size_t outputSize) const {
    static const char* codes[] = {"PR123456", "ASML-REF-98765", "MG:PROFILE:FREE-12", "UNKNOWN-4711"};
    static const CodeKind kinds[] = {CodeKind::ProductionPr, CodeKind::CustomerReference, CodeKind::MgProfile, CodeKind::Unknown};
    const uint8_t i = demoCodeIndex_ % 4U;
    snprintf(output, outputSize, "%s | kind=%s", codes[i], codeKindText(kinds[i]));
}

void BarcodeWorkflowDemoEngine::formatLastProfile(char* output, size_t outputSize) const {
    if (!result_.valid) {
        snprintf(output, outputSize, "No code resolved yet");
        return;
    }
    snprintf(output, outputSize, "%s | %s | READY=%s | AUTO-START=NO",
             result_.profileId[0] ? result_.profileId : "NO PROFILE",
             systemVerdictText(result_.verdict), result_.profileReady ? "YES" : "NO");
}

void VoiceCommandDemoEngine::reset() {
    const bool enabled = result_.wakeWordEnabled;
    result_ = {};
    result_.wakeWordEnabled = enabled;
    state_ = SystemWorkflowState::Idle;
    demoCommandIndex_ = 0U;
}

void VoiceCommandDemoEngine::toggleWakeWord() {
    result_.wakeWordEnabled = !result_.wakeWordEnabled;
    result_.wakeDetected = false;
    result_.commandRecognized = false;
    result_.dispatchAllowed = false;
    result_.criticalActionBlocked = false;
    state_ = SystemWorkflowState::Idle;
}

void VoiceCommandDemoEngine::micTest() {
    result_.micHealthy = true;
    result_.micLevelPercent = 64U;
    state_ = SystemWorkflowState::Complete;
}

void VoiceCommandDemoEngine::nextDemoCommand() {
    demoCommandIndex_ = static_cast<uint8_t>((demoCommandIndex_ + 1U) % 9U);
}

void VoiceCommandDemoEngine::applyCommand(VoiceCommand command, const char* text) {
    result_.command = command;
    result_.commandRecognized = true;
    copyText(result_.recognizedText, text);
    result_.criticalActionBlocked = command == VoiceCommand::CalibrationChange;
    result_.dispatchAllowed = !result_.criticalActionBlocked;
    state_ = result_.criticalActionBlocked ? SystemWorkflowState::Warning : SystemWorkflowState::Complete;
}

void VoiceCommandDemoEngine::listenAndRecognize() {
    result_.wakeDetected = result_.wakeWordEnabled;
    result_.commandRecognized = false;
    result_.dispatchAllowed = false;
    result_.criticalActionBlocked = false;
    if (!result_.wakeWordEnabled) {
        state_ = SystemWorkflowState::Warning;
        copyText(result_.recognizedText, "Wake word disabled");
        return;
    }
    static const VoiceCommand commands[] = {
        VoiceCommand::Start, VoiceCommand::Pause, VoiceCommand::Resume,
        VoiceCommand::Stop, VoiceCommand::RepeatResult, VoiceCommand::SelfTest,
        VoiceCommand::WireFinder, VoiceCommand::Home, VoiceCommand::CalibrationChange
    };
    static const char* texts[] = {
        "Start test", "Pause", "Resume", "Stop", "Repeat result",
        "Self test", "Wire finder", "Home", "Change calibration limit"
    };
    const uint8_t i = demoCommandIndex_ % 9U;
    applyCommand(commands[i], texts[i]);
}

void VoiceCommandDemoEngine::soundTest() {
    result_.micHealthy = true;
    result_.micLevelPercent = 72U;
    state_ = SystemWorkflowState::Complete;
}

void VoiceCommandDemoEngine::formatResult(char* output, size_t outputSize) const {
    if (!result_.commandRecognized) {
        snprintf(output, outputSize, "Hi MG=%s | mic=%s %u%%",
                 result_.wakeWordEnabled ? "ON" : "OFF",
                 result_.micHealthy ? "PASS" : "NOT TESTED",
                 static_cast<unsigned>(result_.micLevelPercent));
        return;
    }
    snprintf(output, outputSize, "'%s' | dispatch=%s%s",
             result_.recognizedText,
             result_.dispatchAllowed ? "QUEUED" : "BLOCKED",
             result_.criticalActionBlocked ? " (critical action)" : "");
}

void VoiceCommandDemoEngine::formatCommandList(char* output, size_t outputSize) const {
    snprintf(output, outputSize, "START/PAUSE/RESUME/STOP/REPEAT/SELF TEST/WIRE FINDER/HOME | critical config blocked");
}

void BasicComponentDemoEngine::reset() {
    state_ = SystemWorkflowState::Idle;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    result_ = {};
    result_.type = type_;
}

void BasicComponentDemoEngine::cycleType() {
    type_ = static_cast<BasicComponentType>((static_cast<uint8_t>(type_) + 1U) % 3U);
    reset();
}

void BasicComponentDemoEngine::test() {
    result_ = {};
    result_.type = type_;
    state_ = SystemWorkflowState::Running;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
}

void BasicComponentDemoEngine::repeat() {
    test();
}

void BasicComponentDemoEngine::finish() {
    result_.valid = true;
    result_.present = true;
    result_.shorted = false;
    result_.verdict = SystemVerdict::Pass;
    if (type_ == BasicComponentType::Diode) {
        result_.polarityKnown = true;
        result_.forwardPolarity = true;
        result_.forwardVoltageV = 0.68f;
    } else if (type_ == BasicComponentType::Led) {
        result_.polarityKnown = true;
        result_.forwardPolarity = true;
        result_.forwardVoltageV = 1.92f;
    } else {
        result_.polarityKnown = false;
        result_.roughCapacitanceUf = 9.6f;
    }
    state_ = SystemWorkflowState::Complete;
}

void BasicComponentDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != SystemWorkflowState::Running) return;
    if (lastStepMs_ != 0U && (nowMs - lastStepMs_) < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 3U) ++phaseIndex_;
    if (phaseIndex_ >= 3U) finish();
}

uint8_t BasicComponentDemoEngine::progressPercent() const {
    if (state_ == SystemWorkflowState::Idle) return 0U;
    if (state_ == SystemWorkflowState::Complete || state_ == SystemWorkflowState::Warning || state_ == SystemWorkflowState::Fault) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 3U);
}

void BasicComponentDemoEngine::formatDetails(char* output, size_t outputSize) const {
    if (!result_.valid) {
        snprintf(output, outputSize, "%s | basic presence/direction only", componentText(type_));
        return;
    }
    if (result_.type == BasicComponentType::Capacitor) {
        snprintf(output, outputSize, "CAP present | rough≈%.1f uF | no precision LCR/ESR/leakage claim",
                 static_cast<double>(result_.roughCapacitanceUf));
    } else {
        snprintf(output, outputSize, "%s present | Vf≈%.2f V | polarity=%s | no optical test",
                 componentText(result_.type), static_cast<double>(result_.forwardVoltageV),
                 result_.forwardPolarity ? "FORWARD" : "REVERSE");
    }
}

void HardwareValidationDemoEngine::reset() {
    state_ = SystemWorkflowState::Idle;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
    result_ = {};
    result_.domain = domain_;
    result_.gpio5Reserved = true;
    result_.measuredOnRealHardware = false;
    result_.freezeEligible = false;
}

void HardwareValidationDemoEngine::run(HardwareValidationDomain domain) {
    domain_ = domain;
    result_ = {};
    result_.domain = domain_;
    result_.gpio5Reserved = true;
    result_.measuredOnRealHardware = false;
    result_.freezeEligible = false;
    state_ = SystemWorkflowState::Running;
    phaseIndex_ = 0U;
    lastStepMs_ = 0U;
}

void HardwareValidationDemoEngine::finish() {
    result_.valid = true;
    result_.measuredOnRealHardware = false;
    result_.gpio5Reserved = true;
    result_.freezeEligible = false;
    result_.candidatePass = true;
    result_.verdict = SystemVerdict::Warning; // demo can never release Gerber/production freeze
    if (domain_ == HardwareValidationDomain::Gpio) {
        result_.checksPassed = 10U;
        result_.checksTotal = 10U;
    } else if (domain_ == HardwareValidationDomain::Bus) {
        result_.checksPassed = 5U;
        result_.checksTotal = 5U;
        result_.selectedSpiHz = 10000000U;
    } else {
        result_.checksPassed = 4U;
        result_.checksTotal = 4U;
    }
    state_ = SystemWorkflowState::Warning;
}

void HardwareValidationDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != SystemWorkflowState::Running) return;
    if (lastStepMs_ != 0U && (nowMs - lastStepMs_) < intervalMs) return;
    lastStepMs_ = nowMs;
    if (phaseIndex_ < 4U) ++phaseIndex_;
    if (phaseIndex_ >= 4U) finish();
}

uint8_t HardwareValidationDemoEngine::progressPercent() const {
    if (state_ == SystemWorkflowState::Idle) return 0U;
    if (state_ == SystemWorkflowState::Warning || state_ == SystemWorkflowState::Complete || state_ == SystemWorkflowState::Fault) return 100U;
    return static_cast<uint8_t>((static_cast<uint16_t>(phaseIndex_) * 100U) / 4U);
}

void HardwareValidationDemoEngine::formatReport(char* output, size_t outputSize) const {
    snprintf(output, outputSize,
             "PIN FREEZE=CANDIDATE | real-hardware=%s | GPIO5=LCD_RESET RESERVED | Gerber release=BLOCKED",
             result_.measuredOnRealHardware ? "YES" : "NO");
}

void HardwareValidationDemoEngine::formatPinSet(char* output, size_t outputSize) const {
    snprintf(output, outputSize,
             "GPIO 1/2/3/4/20/32/33/45/46/47 | GPIO5 reserved | %s demo %u/%u",
             domainText(domain_), static_cast<unsigned>(result_.checksPassed),
             static_cast<unsigned>(result_.checksTotal));
}

void SystemBackboneCore::resetAll() {
    network_.reset();
    barcode_.reset();
    voice_.reset();
    component_.reset();
    hardwareValidation_.reset();
}

void SystemBackboneCore::tick(uint32_t nowMs) {
    network_.tick(nowMs);
    barcode_.tick(nowMs);
    component_.tick(nowMs);
    hardwareValidation_.tick(nowMs);
}

}  // namespace mg::p4
