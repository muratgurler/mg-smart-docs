#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "SystemBackboneCore.h"

using namespace mg::p4;

static void runNetwork(NetworkPrDemoEngine& n) {
    n.lookupPr("PR123456");
    assert(n.result().oldProfileCleared);
    assert(!n.result().autoStartRequested);
    for (uint32_t t = 100U; t < 2000U; t += 300U) n.tick(t, 1U);
    assert(n.result().valid);
    assert(n.result().serverReachable);
    assert(n.result().profileReady);
    assert(!n.result().autoStartRequested);
    assert(strcmp(n.result().profileId, "DEMO-DB25") == 0);
    assert(n.freeCableIndependent());

    n.cycleLink(); // Wi-Fi
    n.cycleLink(); // offline
    assert(n.link() == NetworkLink::Offline);
    n.testServer();
    assert(!n.result().serverReachable);
    assert(!n.result().profileReady);
    assert(n.freeCableIndependent());
}

static void runBarcode(BarcodeWorkflowDemoEngine& b) {
    b.scanCurrent();
    assert(b.result().oldProfileCleared);
    assert(!b.result().autoStartRequested);
    for (uint32_t t = 100U; t < 1800U; t += 250U) b.tick(t, 1U);
    assert(b.result().kind == CodeKind::ProductionPr);
    assert(b.result().profileReady);
    assert(b.result().networkLookupRequired);
    assert(!b.result().autoStartRequested);

    b.nextDemoCode();
    b.nextDemoCode();
    b.nextDemoCode(); // unknown
    b.scanCurrent();
    for (uint32_t t = 2000U; t < 3600U; t += 250U) b.tick(t, 1U);
    assert(b.result().kind == CodeKind::Unknown);
    assert(!b.result().accepted);
    assert(!b.result().profileReady);

    b.manualEntry("MG-MANUAL-01");
    for (uint32_t t = 4000U; t < 5600U; t += 250U) b.tick(t, 1U);
    assert(b.result().kind == CodeKind::MgProfile);
    assert(b.result().profileReady);
    assert(!b.result().networkLookupRequired);
    assert(!b.result().autoStartRequested);
}

static void runVoice(VoiceCommandDemoEngine& v) {
    assert(v.wakeEnabled());
    v.micTest();
    assert(v.result().micHealthy);
    assert(v.result().micLevelPercent > 0U);

    v.listenAndRecognize();
    assert(v.result().commandRecognized);
    assert(v.result().dispatchAllowed);
    assert(!v.result().criticalActionBlocked);

    // Advance to the protected calibration-change demo command.
    for (int i = 0; i < 8; ++i) v.nextDemoCommand();
    v.listenAndRecognize();
    assert(v.result().command == VoiceCommand::CalibrationChange);
    assert(v.result().criticalActionBlocked);
    assert(!v.result().dispatchAllowed);

    v.toggleWakeWord();
    assert(!v.wakeEnabled());
    v.listenAndRecognize();
    assert(!v.result().dispatchAllowed);
}

static void finishComponent(BasicComponentDemoEngine& c, uint32_t base) {
    c.test();
    for (uint32_t t = base; t < base + 1000U; t += 250U) c.tick(t, 1U);
    assert(c.result().valid);
    assert(c.result().present);
    assert(c.result().verdict == SystemVerdict::Pass);
}

static void runComponent(BasicComponentDemoEngine& c) {
    finishComponent(c, 100U);
    assert(c.result().type == BasicComponentType::Diode);
    assert(c.result().polarityKnown);
    assert(c.result().forwardVoltageV > 0.5f);

    c.cycleType();
    finishComponent(c, 2000U);
    assert(c.result().type == BasicComponentType::Led);
    assert(c.result().polarityKnown);

    c.cycleType();
    finishComponent(c, 4000U);
    assert(c.result().type == BasicComponentType::Capacitor);
    assert(!c.result().polarityKnown);
    assert(c.result().roughCapacitanceUf > 1.0f);
}

static void finishHardware(HardwareValidationDemoEngine& h, HardwareValidationDomain domain, uint32_t base) {
    h.run(domain);
    for (uint32_t t = base; t < base + 1600U; t += 300U) h.tick(t, 1U);
    assert(h.result().valid);
    assert(h.result().candidatePass);
    assert(h.result().gpio5Reserved);
    assert(!h.result().measuredOnRealHardware);
    assert(!h.result().freezeEligible);
    assert(h.result().verdict == SystemVerdict::Warning);
}

static void runHardware(HardwareValidationDemoEngine& h) {
    finishHardware(h, HardwareValidationDomain::Gpio, 100U);
    finishHardware(h, HardwareValidationDomain::Bus, 2000U);
    assert(h.result().selectedSpiHz == 10000000U);
    finishHardware(h, HardwareValidationDomain::Boot, 4000U);
    char report[200]{};
    h.formatReport(report, sizeof(report));
    assert(strstr(report, "CANDIDATE") != nullptr);
    assert(strstr(report, "GPIO5=LCD_RESET RESERVED") != nullptr);
    assert(strstr(report, "Gerber release=BLOCKED") != nullptr);
}

int main() {
    SystemBackboneCore core;
    runNetwork(core.network());
    core.resetAll();
    runBarcode(core.barcode());
    core.resetAll();
    runVoice(core.voice());
    core.resetAll();
    runComponent(core.component());
    core.resetAll();
    runHardware(core.hardwareValidation());
    puts("System core batch6 executable test passed.");
    return 0;
}
