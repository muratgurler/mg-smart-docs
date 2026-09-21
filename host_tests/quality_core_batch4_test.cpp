#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "QualityBackboneCore.h"

using namespace mg::p4;

static void runKelvin(KelvinQualityDemoEngine& e) {
    e.tick(100U, 1U);
    for (uint32_t t = 200U; t <= 600U; t += 100U) e.tick(t, 1U);
}

static void runPeDs(PeDsQualityDemoEngine& e) {
    e.tick(100U, 1U);
    for (uint32_t t = 200U; t <= 600U; t += 100U) e.tick(t, 1U);
}

static void runFixture(FixtureSpcDemoEngine& e) {
    e.tick(100U, 1U);
    for (uint32_t t = 200U; t <= 600U; t += 100U) e.tick(t, 1U);
}

int main() {
    QualityBackboneCore core;
    assert(core.environment().sample().valid);

    // Kelvin: running without ZERO is still measured, but must warn.
    core.kelvin().setCurrentMa(100U);
    core.kelvin().setAmbientC(core.environment().sample().temperatureC);
    core.kelvin().measure();
    runKelvin(core.kelvin());
    assert(core.kelvin().result().valid);
    assert(core.kelvin().result().verdict == QualityVerdict::Warning);

    // After ZERO the same deterministic demo result passes the 0.5 mOhm dR gate.
    core.kelvin().zero();
    assert(core.kelvin().zeroValid());
    core.kelvin().measure();
    runKelvin(core.kelvin());
    const KelvinResult& k = core.kelvin().result();
    assert(k.verdict == QualityVerdict::Pass);
    assert(k.currentMa == 100U);
    assert(k.deltaMilliOhm > 0.0f && k.deltaMilliOhm < 0.5f);
    assert(k.shuntMillivolts == 100.0f);
    assert(k.normalizedEstimateMilliOhm < k.resistanceMilliOhm);

    char ref[128]{};
    core.kelvin().formatReference(ref, sizeof(ref));
    assert(strstr(ref, "dR") != nullptr);
    assert(strstr(ref, "PASS") != nullptr);

    // PE/dS: connector type never defines bonding; the profile mode/policy does.
    core.peDs().setMode(PeDsProfileMode::Separate);
    // default policy is FORBIDDEN
    core.peDs().startTest();
    runPeDs(core.peDs());
    assert(core.peDs().result().verdict == QualityVerdict::Pass);
    assert(!core.peDs().result().bondDetected);

    core.peDs().setMode(PeDsProfileMode::Bonded);
    core.peDs().startTest();
    runPeDs(core.peDs());
    assert(core.peDs().result().bondDetected);
    assert(core.peDs().result().verdict == QualityVerdict::Fail);

    // FORBIDDEN -> REQUIRED by two cycles.
    core.peDs().cycleBondPolicy(); // REQUIRED? enum wraps Forbidden(2)->Required(0)
    core.peDs().setMode(PeDsProfileMode::Bonded);
    core.peDs().startTest();
    runPeDs(core.peDs());
    assert(core.peDs().bondPolicy() == BondPolicy::Required);
    assert(core.peDs().result().verdict == QualityVerdict::Pass);

    core.peDs().cycleBondPolicy(); // REQUIRED -> ALLOWED for single-ended Kelvin-only check
    core.peDs().setMode(PeDsProfileMode::AOnly);
    core.peDs().startKelvin();
    runPeDs(core.peDs());
    assert(core.peDs().result().verdict == QualityVerdict::NotApplicable);

    // Fixture/SPC: product and process status are deliberately separate.
    const uint32_t baselineRev = core.fixtureSpc().baselineRevision();
    core.fixtureSpc().startFixtureCheck();
    runFixture(core.fixtureSpc());
    const FixtureSpcSnapshot& f = core.fixtureSpc().snapshot();
    assert(f.valid);
    assert(f.productPass);
    assert(f.processWarning);
    assert(f.fixtureHealthPercent == 96.0f);
    assert(f.cpk > 1.0f);
    assert(core.fixtureSpc().baselineRevision() == baselineRev);
    const char* firstPin = f.selectedPin;
    core.fixtureSpc().nextTrendPin();
    assert(strcmp(core.fixtureSpc().snapshot().selectedPin, firstPin) != 0);

    char report[128]{};
    core.fixtureSpc().formatReport(report, sizeof(report));
    assert(strstr(report, "PRODUCT=PASS") != nullptr);
    assert(strstr(report, "PROCESS=WARNING") != nullptr);

    // Environment/SHT40: raw logging and optional per-net WireSpec model.
    auto& env = core.environment();
    const uint32_t before = env.sample().sampleNumber;
    env.toggleLogging();
    env.toggleLive();
    env.tick(2000U, 1U);
    env.tick(2100U, 1U);
    assert(env.sample().sampleNumber > before);
    assert(env.logCount() > 0U);

    const float r1 = env.theoreticalWireMilliOhm();
    assert(fabsf(r1 - 68.96f) < 0.2f);
    env.cycleWireSpec();
    const float r4 = env.theoreticalWireMilliOhm();
    assert(fabsf(r4 - 17.24f) < 0.2f);
    env.cycleWireSpec();
    assert(env.theoreticalWireMilliOhm() < 0.0f);
    assert(env.normalizeCopperResistanceMilliOhm(8.40f) < 8.40f);

    puts("Quality core batch4 executable test passed.");
    return 0;
}
