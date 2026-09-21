#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mg::p4 {

enum class SystemWorkflowState : uint8_t {
    Idle,
    Running,
    Complete,
    Warning,
    Fault,
};

enum class SystemVerdict : uint8_t {
    NotRun,
    Pass,
    Warning,
    Fail,
    Blocked,
};

const char* systemVerdictText(SystemVerdict verdict);

enum class NetworkLink : uint8_t {
    Offline = 0,
    Ethernet,
    WifiSta,
};

struct NetworkLookupResult {
    bool valid = false;
    bool serverReachable = false;
    bool profileReady = false;
    bool autoStartRequested = false; // must remain false: operator START is mandatory
    bool oldProfileCleared = false;
    NetworkLink link = NetworkLink::Offline;
    char pr[20] = {};
    char customerReference[32] = {};
    char revision[16] = {};
    char profileId[32] = {};
    SystemVerdict verdict = SystemVerdict::NotRun;
};

// Demo workflow for company-network lookup. It models the PR -> HTTP GET ->
// customer reference -> revision -> profile validation chain, but performs no
// real network I/O. free/manual cable testing is never gated by this engine.
class NetworkPrDemoEngine {
public:
    void reset();
    void cycleLink();
    NetworkLink link() const { return link_; }
    void testServer();
    void lookupPr(const char* pr = "PR123456");
    void tick(uint32_t nowMs, uint32_t intervalMs = 260U);

    SystemWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    uint8_t cacheCount() const { return 3U; }
    bool freeCableIndependent() const { return true; }
    const NetworkLookupResult& result() const { return result_; }
    void formatStatus(char* output, size_t outputSize) const;
    void formatCache(char* output, size_t outputSize) const;

private:
    void finishLookup();

    NetworkLink link_ = NetworkLink::Ethernet;
    SystemWorkflowState state_ = SystemWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
    bool serverTested_ = false;
    NetworkLookupResult result_{};
};

enum class CodeKind : uint8_t {
    Unknown = 0,
    ProductionPr,
    CustomerReference,
    MgProfile,
};

struct BarcodeScanResult {
    bool valid = false;
    CodeKind kind = CodeKind::Unknown;
    bool accepted = false;
    bool oldProfileCleared = false;
    bool profileReady = false;
    bool autoStartRequested = false; // frozen safety rule: always false
    bool networkLookupRequired = false;
    char code[32] = {};
    char profileId[32] = {};
    char revision[16] = {};
    SystemVerdict verdict = SystemVerdict::NotRun;
};

// Generic code-scanner model: PR, customer/ASML code and MG profile codes are
// separate identifiers. Unknown codes are not treated as electrical failures.
class BarcodeWorkflowDemoEngine {
public:
    void reset();
    void nextDemoCode();
    void scanCurrent();
    void scanAgain();
    void manualEntry(const char* code = "MG-MANUAL-01");
    void tick(uint32_t nowMs, uint32_t intervalMs = 230U);

    SystemWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    uint8_t demoCodeIndex() const { return demoCodeIndex_; }
    const BarcodeScanResult& result() const { return result_; }
    void formatCurrentCode(char* output, size_t outputSize) const;
    void formatLastProfile(char* output, size_t outputSize) const;

private:
    void beginScan(const char* code, CodeKind kind);
    void finishScan();

    SystemWorkflowState state_ = SystemWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
    uint8_t demoCodeIndex_ = 0U;
    BarcodeScanResult result_{};
};

enum class VoiceCommand : uint8_t {
    None = 0,
    Start,
    Pause,
    Resume,
    Stop,
    RepeatResult,
    SelfTest,
    WireFinder,
    Home,
    CalibrationChange, // intentionally protected/blocked in voice path
};

struct VoiceCommandResult {
    bool wakeWordEnabled = true;
    bool wakeDetected = false;
    bool micHealthy = false;
    bool commandRecognized = false;
    bool dispatchAllowed = false;
    bool criticalActionBlocked = false;
    uint8_t micLevelPercent = 0U;
    VoiceCommand command = VoiceCommand::None;
    char recognizedText[40] = {};
};

// Wake-word/command contract. It queues only safe state-machine intents; it
// never changes calibration limits, deletes data, or directly drives outputs.
class VoiceCommandDemoEngine {
public:
    void reset();
    void toggleWakeWord();
    void micTest();
    void nextDemoCommand();
    void listenAndRecognize();
    void soundTest();

    bool wakeEnabled() const { return result_.wakeWordEnabled; }
    SystemWorkflowState state() const { return state_; }
    uint8_t progressPercent() const { return state_ == SystemWorkflowState::Idle ? 0U : 100U; }
    const VoiceCommandResult& result() const { return result_; }
    void formatResult(char* output, size_t outputSize) const;
    void formatCommandList(char* output, size_t outputSize) const;

private:
    void applyCommand(VoiceCommand command, const char* text);

    SystemWorkflowState state_ = SystemWorkflowState::Idle;
    uint8_t demoCommandIndex_ = 0U;
    VoiceCommandResult result_{};
};

enum class BasicComponentType : uint8_t {
    Diode = 0,
    Led,
    Capacitor,
};

struct BasicComponentResult {
    bool valid = false;
    BasicComponentType type = BasicComponentType::Diode;
    bool present = false;
    bool shorted = false;
    bool polarityKnown = false;
    bool forwardPolarity = false;
    float forwardVoltageV = 0.0f;
    float roughCapacitanceUf = 0.0f;
    SystemVerdict verdict = SystemVerdict::NotRun;
};

// Minimal optional component test. It deliberately does not claim precision
// LCR, LED brightness, ESR, leakage, or semiconductor characterization.
class BasicComponentDemoEngine {
public:
    void reset();
    void setType(BasicComponentType type) { type_ = type; }
    BasicComponentType type() const { return type_; }
    void cycleType();
    void test();
    void repeat();
    void tick(uint32_t nowMs, uint32_t intervalMs = 240U);

    SystemWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    const BasicComponentResult& result() const { return result_; }
    void formatDetails(char* output, size_t outputSize) const;

private:
    void finish();

    BasicComponentType type_ = BasicComponentType::Diode;
    SystemWorkflowState state_ = SystemWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
    BasicComponentResult result_{};
};

enum class HardwareValidationDomain : uint8_t {
    Gpio = 0,
    Bus,
    Boot,
};

struct HardwareValidationResult {
    bool valid = false;
    HardwareValidationDomain domain = HardwareValidationDomain::Gpio;
    bool measuredOnRealHardware = false; // demo must never set true
    bool candidatePass = false;
    bool gpio5Reserved = true;
    bool freezeEligible = false;          // remains false until real-board validation
    uint32_t selectedSpiHz = 0U;
    uint8_t checksPassed = 0U;
    uint8_t checksTotal = 0U;
    SystemVerdict verdict = SystemVerdict::NotRun;
};

// Pin-Freeze validation contract. Demo checks exercise the future procedure,
// not the electrical pins. Gerber/production freeze stays blocked until the
// same checks are executed by a real JC1060 + carrier hardware driver.
class HardwareValidationDemoEngine {
public:
    void reset();
    void run(HardwareValidationDomain domain);
    void tick(uint32_t nowMs, uint32_t intervalMs = 250U);

    SystemWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint8_t phaseIndex() const { return phaseIndex_; }
    const HardwareValidationResult& result() const { return result_; }
    void formatReport(char* output, size_t outputSize) const;
    void formatPinSet(char* output, size_t outputSize) const;

private:
    void finish();

    HardwareValidationDomain domain_ = HardwareValidationDomain::Gpio;
    SystemWorkflowState state_ = SystemWorkflowState::Idle;
    uint8_t phaseIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
    HardwareValidationResult result_{};
};

// Remaining application/system backbone group. All engines are deterministic
// demos with hardware/network/audio seams left explicit for later PCB work.
class SystemBackboneCore {
public:
    SystemBackboneCore() { resetAll(); }

    void resetAll();
    void tick(uint32_t nowMs);

    NetworkPrDemoEngine& network() { return network_; }
    const NetworkPrDemoEngine& network() const { return network_; }
    BarcodeWorkflowDemoEngine& barcode() { return barcode_; }
    const BarcodeWorkflowDemoEngine& barcode() const { return barcode_; }
    VoiceCommandDemoEngine& voice() { return voice_; }
    const VoiceCommandDemoEngine& voice() const { return voice_; }
    BasicComponentDemoEngine& component() { return component_; }
    const BasicComponentDemoEngine& component() const { return component_; }
    HardwareValidationDemoEngine& hardwareValidation() { return hardwareValidation_; }
    const HardwareValidationDemoEngine& hardwareValidation() const { return hardwareValidation_; }

private:
    NetworkPrDemoEngine network_{};
    BarcodeWorkflowDemoEngine barcode_{};
    VoiceCommandDemoEngine voice_{};
    BasicComponentDemoEngine component_{};
    HardwareValidationDemoEngine hardwareValidation_{};
};

}  // namespace mg::p4
