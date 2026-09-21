#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../DigitalBackboneCore.h"

using namespace mg::p4;

namespace {

constexpr uint64_t bitFor(uint8_t index) {
    return index < 64U ? (1ULL << index) : 0ULL;
}

void runSweepToEnd(DigitalSweepRunner& runner) {
    runner.start();
    for (int guard = 0; guard < 300 &&
         runner.state() != DigitalWorkflowState::Complete &&
         runner.state() != DigitalWorkflowState::Fault; ++guard) {
        runner.step();
    }
}

void testDemoPhysicalMapUsesConnectedComponents() {
    CableMap map = makeDigitalBackboneDemoMap();
    DemoScanSource source;
    source.setPhysicalMap(&map);
    source.setFaultOverlayEnabled(false);

    const uint64_t expectedA = bitFor(7U) | bitFor(9U);
    const uint64_t expectedB = bitFor(9U) | bitFor(12U) | bitFor(14U);

    PhysicalScanObservation a7 = source.observe(ScanDirection::AtoB, 7U);
    assert(a7.senderGroupMask == expectedA);
    assert(a7.receiverMask == expectedB);

    // Same electrical net must be seen from A9 too, even though the raw map
    // originally listed the fan-out on A7.
    PhysicalScanObservation a9 = source.observe(ScanDirection::AtoB, 9U);
    assert(a9.senderGroupMask == expectedA);
    assert(a9.receiverMask == expectedB);

    PhysicalScanObservation b12 = source.observe(ScanDirection::BtoA, 12U);
    assert(b12.senderGroupMask == expectedB);
    assert(b12.receiverMask == expectedA);
}

void testFaultOverlayCanBeDisabledWithoutBreakingLegacyMode() {
    DemoScanSource source;
    source.setFaultOverlayEnabled(true);
    assert(source.measure(ScanDirection::AtoB, 8U).result ==
           ElectricalResult::WrongConnection);

    source.setFaultOverlayEnabled(false);
    assert(source.measure(ScanDirection::AtoB, 8U).result ==
           ElectricalResult::Ok);
    assert(source.observe(ScanDirection::AtoB, 15U).receiverMask == bitFor(15U));
}

void testFull128NodeSweepContract() {
    CableProfile profile = makeNormalProfile(kMaximumSignalPins, true, true);
    CableMap expected = makeOneToOneCableMap();
    DemoScanSource source;
    source.setFaultOverlayEnabled(true);

    DigitalSweepRunner runner;
    runner.configure(profile, source, &expected);
    assert(runner.stats().total == 128U);
    runSweepToEnd(runner);
    assert(runner.stats().completed == 128U);
    assert(runner.progressPercent() == 100U);
    assert(runner.state() == DigitalWorkflowState::Fault);
    assert(runner.stats().wrongConnection > 0U);
    assert(runner.stats().open > 0U);
    assert(runner.stats().shortCircuit > 0U);
    assert(runner.stats().highResistance > 0U);
    assert(runner.stats().sameSideMismatch > 0U);
}

void testCableLearnReconstructsCommonNet() {
    CableProfile profile = makeNormalProfile(kMaximumSignalPins, true, true);
    CableMap physical = makeDigitalBackboneDemoMap();
    DemoScanSource source;
    source.setPhysicalMap(&physical);
    source.setFaultOverlayEnabled(false);

    CableLearnEngine learn;
    learn.configure(profile, source);
    assert(learn.totalObservations() == 128U);
    learn.start();
    for (int guard = 0; guard < 300 &&
         learn.state() != DigitalWorkflowState::Complete; ++guard) {
        learn.step();
    }
    assert(learn.state() == DigitalWorkflowState::Complete);
    assert(learn.completedObservations() == 128U);
    assert(learn.progressPercent() == 100U);
    assert(learn.learnedNetCount() > 0U);

    const CableMap& map = learn.learnedMap();
    const uint64_t expectedA = bitFor(7U) | bitFor(9U);
    const uint64_t expectedB = bitFor(9U) | bitFor(12U) | bitFor(14U);
    assert(map.senderMaskA(7U) == expectedA);
    assert(map.senderMaskA(9U) == expectedA);
    assert(map.receiverMaskAtoB(7U) == expectedB);
    assert(map.receiverMaskAtoB(9U) == expectedB);
    assert(map.receiverMaskBtoA(12U) == expectedA);

    char summary[128]{};
    learn.formatSummary(summary, sizeof(summary));
    assert(strstr(summary, "128/128") != nullptr);
}

void testSmartProbeReturnsWholeConnectedNet() {
    CableProfile profile = makeNormalProfile(kMaximumSignalPins, true, true);
    CableMap map = makeDigitalBackboneDemoMap();
    SmartProbeEngine probe;
    probe.configure(profile, map);

    // Reset begins at A/PE. Move until A7 is selected.
    for (int guard = 0; guard < 128; ++guard) {
        if (probe.selectedSide() == CableMapSide::A && probe.selectedIndex() == 7U) break;
        probe.nextEnd();
    }
    assert(probe.selectedSide() == CableMapSide::A);
    assert(probe.selectedIndex() == 7U);

    probe.start();
    for (uint32_t t = 100U; t <= 500U &&
         probe.state() != DigitalWorkflowState::Complete; t += 100U) {
        probe.tick(t, 1U);
    }
    assert(probe.state() == DigitalWorkflowState::Complete);
    assert(probe.foundNet());
    assert(probe.found().aMask == (bitFor(7U) | bitFor(9U)));
    assert(probe.found().bMask == (bitFor(9U) | bitFor(12U) | bitFor(14U)));

    char text[128]{};
    probe.formatFoundNet(text, sizeof(text));
    assert(strstr(text, "A7") != nullptr);
    assert(strstr(text, "A9") != nullptr);
    assert(strstr(text, "B12") != nullptr);
}

void testSelfTestPlansAreDistinct() {
    SelfTestDemoEngine self;
    self.start(SelfTestMode::Fast);
    const size_t fastCount = self.itemCount();
    assert(fastCount == 4U);
    while (self.state() == DigitalWorkflowState::Running) self.step();
    assert(self.state() == DigitalWorkflowState::Complete);
    assert(self.passCount() == fastCount);

    self.start(SelfTestMode::Full);
    const size_t fullCount = self.itemCount();
    assert(fullCount > fastCount);
    while (self.state() == DigitalWorkflowState::Running) self.step();
    assert(self.failCount() == 0U);

    self.start(SelfTestMode::Calibration);
    const size_t calCount = self.itemCount();
    assert(calCount > fullCount);
    bool has10m = false;
    bool has100m = false;
    bool has1r = false;
    for (size_t i = 0; i < self.itemCount(); ++i) {
        const char* name = self.item(i).name;
        has10m |= name != nullptr && strstr(name, "10 mOhm") != nullptr;
        has100m |= name != nullptr && strstr(name, "100 mOhm") != nullptr;
        has1r |= name != nullptr && strstr(name, "1 Ohm") != nullptr;
    }
    assert(has10m && has100m && has1r);
}

void finishMultiSegment(MultiConnectorDemoEngine& multi, uint32_t& now) {
    multi.startSelected();
    for (int guard = 0; guard < 300 &&
         multi.state() != DigitalWorkflowState::Complete &&
         multi.state() != DigitalWorkflowState::Fault; ++guard) {
        now += 50U;
        multi.tick(now, 1U);
    }
}

void testMultiConnectorAdjacentSegmentsOnly() {
    MultiConnectorDemoEngine multi;
    uint32_t now = 0U;

    assert(strcmp(multi.segmentName(multi.selected()), "A-B") == 0);
    assert(multi.segmentSignalCount(MultiSegmentId::AB) == 25U);
    assert(multi.segmentSignalCount(MultiSegmentId::BC) == 12U);
    assert(multi.segmentSignalCount(MultiSegmentId::CD) == 8U);

    finishMultiSegment(multi, now);
    assert(multi.result(MultiSegmentId::AB).state == DigitalWorkflowState::Complete);
    assert(multi.result(MultiSegmentId::AB).stats.errorCount() == 0U);

    multi.nextSegment();
    assert(multi.selected() == MultiSegmentId::BC);
    finishMultiSegment(multi, now);
    assert(multi.result(MultiSegmentId::BC).state == DigitalWorkflowState::Fault);
    assert(multi.result(MultiSegmentId::BC).stats.errorCount() > 0U);

    multi.nextSegment();
    assert(multi.selected() == MultiSegmentId::CD);
    finishMultiSegment(multi, now);
    assert(multi.result(MultiSegmentId::CD).state == DigitalWorkflowState::Complete);
    assert(multi.result(MultiSegmentId::CD).stats.errorCount() == 0U);

    char all[128]{};
    multi.formatAllResults(all, sizeof(all));
    assert(strstr(all, "A-B:PASS") != nullptr);
    assert(strstr(all, "B-C:FAIL") != nullptr);
    assert(strstr(all, "C-D:PASS") != nullptr);
}

}  // namespace

int main() {
    testDemoPhysicalMapUsesConnectedComponents();
    testFaultOverlayCanBeDisabledWithoutBreakingLegacyMode();
    testFull128NodeSweepContract();
    testCableLearnReconstructsCommonNet();
    testSmartProbeReturnsWholeConnectedNet();
    testSelfTestPlansAreDistinct();
    testMultiConnectorAdjacentSegmentsOnly();
    puts("Digital core batch3 tests passed.");
    return 0;
}
