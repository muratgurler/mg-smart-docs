#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "HighSpeedBackboneCore.h"

using namespace mg::p4;

static void runTdr(TdrDemoEngine& e) {
    e.tick(100U, 1U);
    for (uint32_t t = 200U; t <= 600U; t += 100U) e.tick(t, 1U);
}

static void runPair(PairIntegrityDemoEngine& e) {
    e.tick(100U, 1U);
    for (uint32_t t = 200U; t <= 600U; t += 100U) e.tick(t, 1U);
}

static void runUsb(UsbCIdentityDemoEngine& e) {
    e.tick(100U, 1U);
    for (uint32_t t = 200U; t <= 600U; t += 100U) e.tick(t, 1U);
}

int main() {
    HighSpeedBackboneCore core;

    // TDR: A-side selection, calibrated VOP and approximate fault distance.
    core.tdr().setNode(17U);
    core.tdr().start();
    runTdr(core.tdr());
    const TdrResult& t = core.tdr().result();
    assert(t.valid);
    assert(t.aNode == 17U);
    assert(t.calibrationValid);
    assert(t.fault == TdrFaultType::Open);
    assert(t.distanceM > 5.0f && t.distanceM < 6.0f);
    assert(fabsf(t.roundTripNs - t.lineTimeNs * 2.0f) < 0.01f);
    char tdrText[128]{};
    core.tdr().formatResult(tdrText, sizeof(tdrText));
    assert(strstr(tdrText, "OPEN") != nullptr);

    // Pair integrity: nominal pair passes, deterministic pair 13 demonstrates split-pair failure.
    core.pair().setPair(1U);
    core.pair().setFrequencyHz(100000U);
    core.pair().start();
    runPair(core.pair());
    assert(core.pair().result().valid);
    assert(!core.pair().result().splitPair);
    assert(core.pair().result().verdict == HighSpeedVerdict::Pass);

    core.pair().setPair(13U);
    core.pair().setFrequencyHz(250000U);
    core.pair().start();
    runPair(core.pair());
    assert(core.pair().result().splitPair);
    assert(core.pair().result().verdict == HighSpeedVerdict::Fail);

    // USB-C identity is low-power metadata discovery; profile compare is explicit.
    core.usbC().readCable();
    runUsb(core.usbC());
    const UsbCableIdentityResult& u = core.usbC().result();
    assert(u.valid && u.attached && u.emarkerPresent);
    assert(u.declaredCurrentA == 5U);
    assert(u.eprCapable && u.vconnRequired);
    assert(!u.profileCompared);
    core.usbC().compareProfile();
    assert(core.usbC().result().profileCompared);
    assert(core.usbC().result().profileMatch);
    assert(core.usbC().result().verdict == HighSpeedVerdict::Pass);
    char profile[128]{};
    core.usbC().formatProfile(profile, sizeof(profile));
    assert(strstr(profile, "PROFILE MATCH") != nullptr);

    // Flex/glitch: hardware-latch semantics preserve a short event until explicit clear.
    core.flexGlitch().start();
    core.flexGlitch().tick(100U, 1U);
    core.flexGlitch().tick(200U, 1U);
    core.flexGlitch().tick(300U, 1U);
    core.flexGlitch().tick(400U, 1U);
    const auto& g = core.flexGlitch().snapshot();
    assert(g.armed);
    assert(g.latched);
    assert(g.eventCount == 1U);
    assert(g.lastEvent.valid);
    assert(g.lastEvent.durationUs > 0U);
    const uint32_t historyCount = g.eventCount;
    core.flexGlitch().clearLatch();
    assert(!core.flexGlitch().snapshot().latched);
    assert(core.flexGlitch().snapshot().eventCount == historyCount);
    core.flexGlitch().pause();
    assert(core.flexGlitch().state() == HighSpeedWorkflowState::Paused);
    core.flexGlitch().resume();
    assert(core.flexGlitch().state() == HighSpeedWorkflowState::Running);

    puts("High-speed core batch5 executable test passed.");
    return 0;
}
