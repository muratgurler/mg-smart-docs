#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../CableProfile.h"
#include "../CableNetGraph.h"
#include "../ScanSession.h"
#include "../ScanSource.h"
#include "../ReportFormatter.h"

using namespace mg::p4;

namespace {

void testProfilePointCounts() {
    const uint8_t expected[] = {26, 10, 16, 38, 45, 51, 63};
    const char* names[] = {
        "Sub-D25", "Sub-D9", "Sub-D15", "Sub-D37",
        "Sub-D44", "Sub-D50", "Sub-D62"};
    assert(profileCount() == sizeof(expected));
    for (size_t i = 0; i < profileCount(); ++i) {
        assert(profileAt(i).activePointCount() == expected[i]);
        assert(strcmp(profileAt(i).name, names[i]) == 0);
        assert(profileAt(i).isSubD());
        assert(!profileAt(i).includePe);
        assert(profileAt(i).includeDrainShield);
    }
}

void testSharedNumberRowLabels() {
    const CableProfile& db25 = defaultProfile();
    char label[4] = {};
    db25.formatSlotLabel(0, label, sizeof(label));
    assert(strcmp(label, "1") == 0);
    db25.formatSlotLabel(24, label, sizeof(label));
    assert(strcmp(label, "25") == 0);
    db25.formatSlotLabel(25, label, sizeof(label));
    assert(strcmp(label, "dS") == 0);
}

void testNormalProfileOptions() {
    CableProfile normal = makeNormalProfile(25, true, true);
    assert(!normal.isSubD());
    assert(normal.activePointCount() == 27);
    assert(normal.slotToTestIndex(0) == kPeTestIndex);
    assert(normal.slotToTestIndex(1) == 1U);
    assert(normal.slotToTestIndex(26) == kDrainShieldTestIndex);

    normal.includePeA = false;
    normal.includePeB = false;
    normal.syncSpecialPointUnion();
    assert(normal.activePointCount() == 26);
    assert(normal.slotToTestIndex(0) == 1U);
    normal.includeDrainShieldA = false;
    normal.includeDrainShieldB = false;
    normal.syncSpecialPointUnion();
    assert(normal.activePointCount() == 25);

    setNormalSignalPinCount(normal, 0);
    assert(normal.signalPinCount == 1U);
    setNormalSignalPinCount(normal, 255);
    assert(normal.signalPinCount == kMaximumSignalPins);
}

void testSideSpecificPeDrainProfile() {
    CableProfile profile = makeNormalProfile(5, true, true);
    profile.includePeA = true;
    profile.includePeB = false;
    profile.includeDrainShieldA = false;
    profile.includeDrainShieldB = true;
    profile.syncSpecialPointUnion();

    assert(profile.includePe);
    assert(profile.includeDrainShield);
    assert(profile.pointEnabledOnSide(true, kPeTestIndex));
    assert(!profile.pointEnabledOnSide(false, kPeTestIndex));
    assert(!profile.pointEnabledOnSide(true, kDrainShieldTestIndex));
    assert(profile.pointEnabledOnSide(false, kDrainShieldTestIndex));

    DemoScanSource source;
    source.setSpecialCrossConnections(false, false);
    ScanSession session(profile, source);
    assert(session.totalMeasurements() == 12U);  // 5+PE from A, 5+dS from B

    for (uint8_t i = 0; i < 6U; ++i) {
        session.step();
    }
    assert(session.direction() == ScanDirection::BtoA);
    assert(session.reportResult(ScanDirection::AtoB, profile.testIndexToSlot(kPeTestIndex)) == ElectricalResult::Ok);
    assert(session.reportResult(ScanDirection::AtoB, profile.testIndexToSlot(kDrainShieldTestIndex)) == ElectricalResult::NotMeasured);

    for (uint8_t i = 0; i < 6U; ++i) {
        session.step();
    }
    assert(session.runState() == RunState::Complete);
    assert(session.reportResult(ScanDirection::BtoA, profile.testIndexToSlot(kDrainShieldTestIndex)) == ElectricalResult::Ok);
    assert(session.reportResult(ScanDirection::BtoA, profile.testIndexToSlot(kPeTestIndex)) == ElectricalResult::NotMeasured);
}

void testNormalPinPresetCycle() {
    assert(nextNormalSignalPinPreset(0U) == 5U);
    assert(nextNormalSignalPinPreset(1U) == 5U);
    assert(nextNormalSignalPinPreset(4U) == 5U);
    assert(nextNormalSignalPinPreset(5U) == 10U);
    assert(nextNormalSignalPinPreset(6U) == 10U);
    assert(nextNormalSignalPinPreset(9U) == 10U);
    assert(nextNormalSignalPinPreset(10U) == 20U);
    assert(nextNormalSignalPinPreset(19U) == 20U);
    assert(nextNormalSignalPinPreset(20U) == 30U);
    assert(nextNormalSignalPinPreset(29U) == 30U);
    assert(nextNormalSignalPinPreset(30U) == 40U);
    assert(nextNormalSignalPinPreset(39U) == 40U);
    assert(nextNormalSignalPinPreset(40U) == 50U);
    assert(nextNormalSignalPinPreset(49U) == 50U);
    assert(nextNormalSignalPinPreset(50U) == 60U);
    assert(nextNormalSignalPinPreset(59U) == 60U);
    assert(nextNormalSignalPinPreset(60U) == 5U);
    assert(nextNormalSignalPinPreset(61U) == 5U);
    assert(nextNormalSignalPinPreset(kMaximumSignalPins) == 5U);
}

void testFixtureLabelsNeverSwap() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    assert(strcmp(session.leftFixtureLabel(), "A") == 0);
    assert(strcmp(session.rightFixtureLabel(), "B") == 0);
    session.toggleDirection();
    assert(session.direction() == ScanDirection::BtoA);
    assert(strcmp(session.leftFixtureLabel(), "A") == 0);
    assert(strcmp(session.rightFixtureLabel(), "B") == 0);
}

void testAutomaticTwoDirectionCycleAndSeparateReports() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    const uint8_t points = session.profile().activePointCount();
    assert(points == 26);

    for (uint8_t i = 0; i < points; ++i) {
        session.step();
    }
    assert(session.direction() == ScanDirection::BtoA);
    assert(session.runState() == RunState::Paused);
    assert(session.reportResult(ScanDirection::AtoB, 7) ==
           ElectricalResult::WrongConnection);
    assert(session.reportResult(ScanDirection::BtoA, 8) ==
           ElectricalResult::NotMeasured);

    for (uint8_t i = 0; i < points; ++i) {
        session.step();
    }
    assert(session.runState() == RunState::Complete);
    assert(session.direction() == ScanDirection::AtoB);
    // Fix11: the logical next-cycle direction resets to A->B, but the live
    // display keeps the just-measured final return point B1 -> A1 until the
    // completion report is opened.
    assert(session.displayDirection() == ScanDirection::BtoA);
    assert(session.displayTestIndexB() == 1U);
    assert(session.displayTestIndexA() == 1U);
    assert(session.currentSlot() == 0);
    assert(session.progressPercent() == 100);
    assert(session.completedMeasurements() == session.totalMeasurements());
    assert(session.reportResult(ScanDirection::BtoA, 17) ==
           ElectricalResult::HighResistance);
    assert(session.errorCount(ScanDirection::AtoB) == 6);
    assert(session.errorCount(ScanDirection::BtoA) == 6);
}

void testDemoFaultsComeFromOnePhysicalCable() {
    DemoScanSource source;

    // Wrong mapping is a physical two-wire swap, not two unrelated demo rows.
    Measurement m = source.measure(ScanDirection::AtoB, 8U);
    assert(m.result == ElectricalResult::WrongConnection);
    assert(m.actualTestIndex == 9U);
    assert(m.actualReceiverMask == (1ULL << 9U));
    m = source.measure(ScanDirection::BtoA, 9U);
    assert(m.result == ElectricalResult::WrongConnection);
    assert(m.actualTestIndex == 8U);
    assert(m.actualReceiverMask == (1ULL << 8U));

    m = source.measure(ScanDirection::AtoB, 9U);
    assert(m.result == ElectricalResult::WrongConnection);
    assert(m.actualTestIndex == 8U);
    m = source.measure(ScanDirection::BtoA, 8U);
    assert(m.result == ElectricalResult::WrongConnection);
    assert(m.actualTestIndex == 9U);

    // One broken conductor is open from both ends.
    m = source.measure(ScanDirection::AtoB, 15U);
    assert(m.result == ElectricalResult::Open);
    assert(m.actualReceiverMask == 0ULL);
    assert(m.actualTestIndex == 0xFFU);
    m = source.measure(ScanDirection::BtoA, 15U);
    assert(m.result == ElectricalResult::Open);
    assert(m.actualReceiverMask == 0ULL);
    assert(m.actualTestIndex == 0xFFU);

    // Resistance belongs to the same physical path and is direction-neutral.
    m = source.measure(ScanDirection::AtoB, 18U);
    assert(m.result == ElectricalResult::HighResistance);
    assert(m.actualTestIndex == 18U);
    m = source.measure(ScanDirection::BtoA, 18U);
    assert(m.result == ElectricalResult::HighResistance);
    assert(m.actualTestIndex == 18U);

    // A22/A23 and B22/B23 are one shorted component. Driving either source
    // reveals the other source-side contact and both receiver-side contacts.
    const uint64_t shortPairMask = (1ULL << 22U) | (1ULL << 23U);
    m = source.measure(ScanDirection::AtoB, 22U);
    assert(m.result == ElectricalResult::ShortCircuit);
    assert(m.senderGroupMask == shortPairMask);
    assert(m.actualReceiverMask == shortPairMask);
    m = source.measure(ScanDirection::AtoB, 23U);
    assert(m.result == ElectricalResult::ShortCircuit);
    assert(m.senderGroupMask == shortPairMask);
    assert(m.actualReceiverMask == shortPairMask);
    m = source.measure(ScanDirection::BtoA, 22U);
    assert(m.result == ElectricalResult::ShortCircuit);
    assert(m.senderGroupMask == shortPairMask);
    assert(m.actualReceiverMask == shortPairMask);
    m = source.measure(ScanDirection::BtoA, 23U);
    assert(m.result == ElectricalResult::ShortCircuit);
    assert(m.senderGroupMask == shortPairMask);
    assert(m.actualReceiverMask == shortPairMask);
}

void testMidScanDirectionChangeStaysAtCurrentContact() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);

    // Measure A1..A12. The visible pair after the final step is A12 -> B12,
    // while currentSlot already points at the next forward contact.
    for (uint8_t measurement = 0; measurement < 12; ++measurement) {
        session.step();
    }
    assert(session.displayTestIndexA() == 12);
    assert(session.displayTestIndexB() == 12);
    assert(session.currentSlot() == 12);

    session.toggleDirection();
    assert(session.direction() == ScanDirection::BtoA);
    assert(session.currentTestIndex() == 12);
    assert(session.displayTestIndexA() == 12);
    assert(session.displayTestIndexB() == 12);

    // First reverse measurement is B12 -> A12; then it continues B11, B10...
    session.step();
    assert(session.displayDirection() == ScanDirection::BtoA);
    assert(session.displayTestIndexB() == 12);
    assert(session.currentTestIndex() == 11);
    session.step();
    assert(session.displayTestIndexB() == 11);
    assert(session.currentTestIndex() == 10);

    // Finish the manual B->A return to pin 1. The state machine then turns
    // back to A->B at pin 1, but remeasuring it must not inflate progress
    // was already recorded before the mid-scan reversal.
    for (uint8_t source = 10; source <= 10; --source) {
        session.step();
        if (source == 0) break;
    }
    assert(session.direction() == ScanDirection::AtoB);
    const uint8_t completedBeforeDuplicate = session.completedMeasurements();
    session.step();
    assert(session.completedMeasurements() == completedBeforeDuplicate);
}

void testWrongConnectionDeterminesReverseStart() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);

    for (uint8_t measurement = 0; measurement < 8; ++measurement) {
        session.step();
    }
    assert(session.displayTestIndexA() == 8);
    assert(session.displayTestIndexB() == 9);

    session.toggleDirection();
    assert(session.direction() == ScanDirection::BtoA);
    assert(session.currentTestIndex() == 9);
    session.step();
    assert(session.displayTestIndexB() == 9);
    assert(session.displayTestIndexA() == 8);
    assert(session.reportResult(ScanDirection::BtoA, 8) ==
           ElectricalResult::WrongConnection);
    assert(session.currentTestIndex() == 8);
}

void testRunningDirectionChangeReversesWithoutPausing() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    session.setDemoStepIntervalMs(1);
    session.start();

    for (uint32_t now = 1; now <= 12; ++now) {
        session.tick(now);
    }
    assert(session.runState() == RunState::Running);
    assert(session.displayTestIndexA() == 12);
    assert(session.displayTestIndexB() == 12);

    session.toggleDirection();
    assert(session.runState() == RunState::Running);
    assert(session.direction() == ScanDirection::BtoA);
    assert(session.currentTestIndex() == 12);
    assert(session.statusA(11) == PointVisualStatus::Scanning);
    assert(session.statusB(11) == PointVisualStatus::Scanning);

    session.tick(13);
    assert(session.displayTestIndexB() == 12);
    assert(session.currentTestIndex() == 11);
}

void testContinuousModeRepeatsAndNormalModeCanBeRestored() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    session.setDemoStepIntervalMs(1);

    session.setStopOnError(true);
    assert(session.stopOnError());
    session.setContinuousMode(true);
    assert(session.continuousMode());
    assert(session.stopOnError());
    session.setContinuousMode(false);
    assert(session.stopOnError());

    // This test exercises uninterrupted recycling. The separate regression
    // below covers NON-STOP combined with HATADA DUR.
    session.setStopOnError(false);
    session.setContinuousMode(true);

    session.start();
    const uint8_t cycleLength = session.totalMeasurements();
    for (uint32_t now = 1; now <= cycleLength; ++now) {
        session.tick(now);
    }

    // A complete A->B/B->A sweep immediately returns to the home contact.
    // It never enters Complete and the aggregate report storage is recycled.
    assert(session.runState() == RunState::Running);
    assert(session.direction() == ScanDirection::AtoB);
    assert(session.currentSlot() == 0);
    assert(session.completedMeasurements() == 0);
    assert(session.progressPercent() == 0);
    assert(session.reportResult(ScanDirection::AtoB, 7) ==
           ElectricalResult::NotMeasured);
    assert(session.reportResult(ScanDirection::BtoA, 17) ==
           ElectricalResult::NotMeasured);

    // Turning NON-STOP off does not jump or abort. The next whole cycle ends
    // normally and leaves its two direction reports available.
    session.setContinuousMode(false);
    assert(!session.continuousMode());
    for (uint32_t now = cycleLength + 1U;
         now <= static_cast<uint32_t>(cycleLength) * 2U;
         ++now) {
        session.tick(now);
    }
    assert(session.runState() == RunState::Complete);
    assert(session.progressPercent() == 100);
    assert(session.reportResult(ScanDirection::AtoB, 7) ==
           ElectricalResult::WrongConnection);
    assert(session.reportResult(ScanDirection::BtoA, 17) ==
           ElectricalResult::HighResistance);
}

void testContinuousModeStopsOnFaultAndStartBeginsNextCable() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    session.setDemoStepIntervalMs(1);
    session.setContinuousMode(true);
    session.setStopOnError(true);

    assert(session.continuousMode());
    assert(session.stopOnError());
    session.start();

    // Sub-D25 slot 7 is test index 8. Demo data deliberately maps A8 -> B9.
    for (uint32_t now = 1; now <= 8; ++now) {
        session.tick(now);
    }
    assert(session.runState() == RunState::Paused);
    assert(session.continuousMode());
    assert(session.stopOnError());
    assert(session.displayDirection() == ScanDirection::AtoB);
    assert(session.displayTestIndexA() == 8U);
    assert(session.displayTestIndexB() == 9U);
    assert(session.reportResult(ScanDirection::AtoB, 7) ==
           ElectricalResult::WrongConnection);
    assert(session.completedMeasurements() == 8U);

    // Time passing cannot restart the line. The failed cable remains visible.
    session.tick(1000);
    assert(session.runState() == RunState::Paused);
    assert(session.displayTestIndexA() == 8U);
    assert(session.displayTestIndexB() == 9U);

    // After the rejected cable is removed, one START is enough. The next
    // cable begins as a clean A->B cycle at the first active contact.
    session.start();
    assert(session.runState() == RunState::Running);
    assert(session.direction() == ScanDirection::AtoB);
    assert(session.currentSlot() == 0U);
    assert(session.currentTestIndex() == 1U);
    assert(session.completedMeasurements() == 0U);
    assert(session.reportResult(ScanDirection::AtoB, 7) ==
           ElectricalResult::NotMeasured);
    assert(session.statusA(0) == PointVisualStatus::Scanning);
    assert(session.statusB(0) == PointVisualStatus::Scanning);
}

void testAdjustableScanSpeed() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);

    assert(session.scanSpeedPercent() == kDefaultTestSpeedPercent);
    assert(session.scanStepIntervalMs() == kDemoStepIntervalMs);
    assert(session.scanStepIntervalMs() == 180U);

    session.setScanSpeedPercent(100);
    assert(session.scanSpeedPercent() == kMaximumTestSpeedPercent);
    assert(session.scanStepIntervalMs() == kFastestScanStepIntervalMs);

    session.setScanSpeedPercent(10);
    assert(session.scanSpeedPercent() == kMinimumTestSpeedPercent);
    assert(session.scanStepIntervalMs() == kSlowestScanStepIntervalMs);

    session.setScanSpeedPercent(45);
    assert(session.scanStepIntervalMs() == 840U);
    session.setScanSpeedPercent(90);
    assert(session.scanStepIntervalMs() == kRapidScanStepIntervalMs);
    assert(session.scanStepIntervalMs() == 25U);
    session.setScanSpeedPercent(95);
    assert(session.scanStepIntervalMs() == 23U);
    session.setScanSpeedPercent(100);
    assert(session.scanStepIntervalMs() == 20U);

    session.setScanSpeedPercent(0);
    assert(session.scanSpeedPercent() == kMinimumTestSpeedPercent);
    session.setScanSpeedPercent(255);
    assert(session.scanSpeedPercent() == kMaximumTestSpeedPercent);
}

void testNoUnusedBarsContract() {
    for (size_t profile = 0; profile < profileCount(); ++profile) {
        const CableProfile& current = profileAt(profile);
        for (uint8_t slot = 0; slot < current.activePointCount(); ++slot) {
            assert(current.isValidSlot(slot));
            assert(current.slotToTestIndex(slot) != 0xFF);
        }
        assert(!current.isValidSlot(current.activePointCount()));
    }
}

void testDirectionSpecificReportRows() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    for (uint8_t i = 0; i < session.totalMeasurements(); ++i) {
        session.step();
    }
    char row[96] = {};
    assert(formatReportRow(session, ScanDirection::AtoB, 7, row, sizeof(row)));
    assert(strstr(row, "A_TO_B,8,8,9,YANLIS BAGLANTI") != nullptr);
    assert(formatReportRow(session, ScanDirection::BtoA, 8, row, sizeof(row)));
    assert(strstr(row, "B_TO_A,9,9,8,YANLIS BAGLANTI") != nullptr);
    assert(formatReportRow(session, ScanDirection::AtoB, 14, row, sizeof(row)));
    assert(strstr(row, "A_TO_B,15,15,-,ACIK") != nullptr);
    assert(formatReportRow(session, ScanDirection::BtoA, 14, row, sizeof(row)));
    assert(strstr(row, "B_TO_A,15,15,-,ACIK") != nullptr);
    assert(formatReportRow(session, ScanDirection::BtoA, 17, row, sizeof(row)));
    assert(strstr(row, "B_TO_A,18,18,18,YUKSEK DIRENC") != nullptr);
    assert(formatReportRow(session, ScanDirection::AtoB, 21, row, sizeof(row)));
    assert(strstr(row, "A_TO_B,22,22,22,KISA DEVRE") != nullptr);
    assert(formatReportRow(session, ScanDirection::BtoA, 21, row, sizeof(row)));
    assert(strstr(row, "B_TO_A,22,22,22,KISA DEVRE") != nullptr);
}

void testKnightRiderPairAndReverseOrder() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    const uint8_t points = session.profile().activePointCount();

    session.start();
    assert(session.currentSlot() == 0);
    assert(session.direction() == ScanDirection::AtoB);
    assert(session.statusA(0) == PointVisualStatus::Scanning);
    assert(session.statusB(0) == PointVisualStatus::Scanning);
    assert(session.statusA(1) == PointVisualStatus::Untested);
    assert(session.statusB(1) == PointVisualStatus::Untested);

    session.pause();
    for (uint8_t measurement = 0; measurement < 8; ++measurement) {
        session.step();
    }
    // Demo A8 is deliberately wired to B9. Only that source/destination pair
    // may remain lit; earlier successful measurements must not accumulate.
    assert(session.displayTestIndexA() == 8);
    assert(session.displayTestIndexB() == 9);
    assert(session.statusA(7) == PointVisualStatus::WrongConnection);
    assert(session.statusB(8) == PointVisualStatus::WrongConnection);
    assert(session.statusA(6) == PointVisualStatus::Untested);
    assert(session.statusB(7) == PointVisualStatus::Untested);

    for (uint8_t measurement = 8; measurement < points; ++measurement) {
        session.step();
    }
    assert(session.direction() == ScanDirection::BtoA);
    assert(session.displayDirection() == ScanDirection::AtoB);
    assert(session.currentSlot() == points - 1U);

    // The reverse sweep begins at dS / the highest active slot, not at B0 or
    // B1. After measuring it once, the next source is the preceding slot.
    session.step();
    assert(session.displayDirection() == ScanDirection::BtoA);
    assert(session.displayTestIndexB() == kDrainShieldTestIndex);
    assert(session.displayTestIndexA() == kDrainShieldTestIndex);
    assert(session.currentSlot() == points - 2U);
}

void testCustomMapCrossRoutingAndReverseSweep() {
    CableMap map = makeDefaultCustomCableMap();
    DemoScanSource source;
    source.setPhysicalMap(&map);
    ScanSession session(defaultProfile(), source);
    session.setCustomMap(map);

    assert(session.usesCustomMap());

    // The scanning indicator itself already points at the expected mapped
    // destination before the first electrical result arrives.
    session.start();
    assert(session.displayTestIndexA() == 1U);
    assert(session.displayTestIndexB() == 3U);
    session.pause();

    session.step();
    Measurement m = session.reportMeasurement(ScanDirection::AtoB, 0U);
    assert(m.result == ElectricalResult::Ok);
    assert(m.expectedTestIndex == 3U);
    assert(m.actualTestIndex == 3U);
    assert(session.displayTestIndexA() == 1U);
    assert(session.displayTestIndexB() == 3U);

    session.step();
    m = session.reportMeasurement(ScanDirection::AtoB, 1U);
    assert(m.result == ElectricalResult::Ok);
    assert(m.expectedTestIndex == 1U);
    assert(m.actualTestIndex == 1U);

    session.step();
    m = session.reportMeasurement(ScanDirection::AtoB, 2U);
    assert(m.result == ElectricalResult::Ok);
    assert(m.expectedTestIndex == 2U);
    assert(m.actualTestIndex == 2U);

    // Finish the whole two-direction cycle. B->A must be the reverse of the
    // SAME A->B map, never an independently scripted table.
    while (session.runState() != RunState::Complete) {
        session.step();
    }
    m = session.reportMeasurement(ScanDirection::BtoA, 0U); // B1 -> A2
    assert(m.result == ElectricalResult::Ok);
    assert(m.expectedTestIndex == 2U);
    assert(m.actualTestIndex == 2U);
    m = session.reportMeasurement(ScanDirection::BtoA, 1U); // B2 -> A3
    assert(m.result == ElectricalResult::Ok);
    assert(m.expectedTestIndex == 3U);
    assert(m.actualTestIndex == 3U);
    m = session.reportMeasurement(ScanDirection::BtoA, 2U); // B3 -> A1
    assert(m.result == ElectricalResult::Ok);
    assert(m.expectedTestIndex == 1U);
    assert(m.actualTestIndex == 1U);

    char row[96] = {};
    assert(formatReportRow(session, ScanDirection::AtoB, 0U, row, sizeof(row)));
    assert(strstr(row, "A_TO_B,1,3,3,OK") != nullptr);
    assert(formatReportRow(session, ScanDirection::BtoA, 2U, row, sizeof(row)));
    assert(strstr(row, "B_TO_A,3,1,1,OK") != nullptr);
}

void testCustomPhysicalCableFailsInOneToOneMode() {
    CableMap map = makeDefaultCustomCableMap();
    DemoScanSource source;
    source.setPhysicalMap(&map);
    ScanSession session(defaultProfile(), source);
    session.setOneToOneMap();

    session.step(); // A1 physically reaches B3, but ONE_TO_ONE expects B1.
    const Measurement m = session.reportMeasurement(ScanDirection::AtoB, 0U);
    assert(m.result == ElectricalResult::WrongConnection);
    assert(m.expectedTestIndex == 1U);
    assert(m.actualTestIndex == 3U);
}

void testCustomMapSupportsMultipleReceivers() {
    const uint64_t expectedReceivers = (1ULL << 4U) | (1ULL << 5U);
    PhysicalScanObservation observed{};
    observed.senderGroupMask = 1ULL << 4U;
    observed.receiverMask = expectedReceivers;

    Measurement m = classifyObservation(4U,
                                        observed,
                                        expectedReceivers,
                                        1ULL << 4U);
    assert(m.result == ElectricalResult::Ok);
    assert(m.expectedReceiverMask == expectedReceivers);

    // An extra receiver beyond the expected multi-target net is a SHORT.
    observed.receiverMask |= 1ULL << 6U;
    m = classifyObservation(4U,
                            observed,
                            expectedReceivers,
                            1ULL << 4U);
    assert(m.result == ElectricalResult::ShortCircuit);
}

void testCustomMapSupportsExpectedSameSideGroups() {
    CableMap map = makeOneToOneCableMap();
    map.setSameSideConnection(CableMapSide::A, 10U, 11U, true);
    const uint64_t expectedSender = (1ULL << 10U) | (1ULL << 11U);
    assert(map.senderMaskA(10U) == expectedSender);
    assert(map.senderMaskA(11U) == expectedSender);

    PhysicalScanObservation observed{};
    observed.senderGroupMask = expectedSender;
    observed.receiverMask = 1ULL << 10U;
    Measurement m = classifyObservation(10U,
                                        observed,
                                        1ULL << 10U,
                                        map.senderMaskA(10U));
    assert(m.result == ElectricalResult::Ok);

    // The same electrical observation is a SHORT in one-to-one mode, proving
    // that expected same-side joins are map data rather than globally ignored.
    m = classifyOneToOneObservation(10U, observed);
    assert(m.result == ElectricalResult::ShortCircuit);
}

void testCustomMapReverseIsAlwaysDerivedFromAtoB() {
    CableMap map;
    map.clear();
    map.setAtoBMask(1U, 1ULL << 3U);
    map.setAtoBMask(2U, (1ULL << 3U) | (1ULL << 4U));
    assert(map.receiverMaskBtoA(3U) == ((1ULL << 1U) | (1ULL << 2U)));
    assert(map.receiverMaskBtoA(4U) == (1ULL << 2U));
}

void testCableMapRawLoadRebuildsReverseAndSameSideSymmetry() {
    uint64_t ab[kTestPointsPerSide]{};
    uint64_t aa[kTestPointsPerSide]{};
    uint64_t bb[kTestPointsPerSide]{};
    ab[1] = 1ULL << 3U;
    ab[2] = (1ULL << 1U) | (1ULL << 4U);
    aa[10] = 1ULL << 11U;  // deliberately one-sided persisted input
    bb[20] = (1ULL << 20U) | (1ULL << 21U); // self bit must be removed

    CableMap map;
    map.loadRaw(ab, aa, bb, "PERSISTED");
    assert(strcmp(map.name(), "PERSISTED") == 0);
    assert(map.receiverMaskBtoA(3U) == (1ULL << 1U));
    assert(map.receiverMaskBtoA(1U) == (1ULL << 2U));
    assert(map.senderMaskA(10U) == ((1ULL << 10U) | (1ULL << 11U)));
    assert(map.senderMaskA(11U) == ((1ULL << 10U) | (1ULL << 11U)));
    assert(map.sameSideMaskB(20U) == (1ULL << 21U));
    assert(map.sameSideMaskB(21U) == (1ULL << 20U));
}

void testCableNetGraphCollapsesOneToManyIntoOneElectricalNet() {
    CableMap map;
    map.clear();
    // Electrical drawing: A2 has one cable trunk; B2/B3/B4 are joined near B.
    map.setAtoBMask(2U, 1ULL << 2U);
    map.setSameSideConnection(CableMapSide::B, 2U, 3U, true);
    map.setSameSideConnection(CableMapSide::B, 3U, 4U, true);

    uint64_t visible = 0ULL;
    for (uint8_t i = 1U; i <= 8U; ++i) visible |= 1ULL << i;
    CableNet nets[kTestPointsPerSide]{};
    const size_t count = CableNetGraph::build(map, visible, nets, kTestPointsPerSide);
    assert(count == 1U);
    assert(nets[0].aMask == (1ULL << 2U));
    assert(nets[0].bMask == ((1ULL << 2U) | (1ULL << 3U) | (1ULL << 4U)));
}

void testCableNetGraphCollapsesManyToOneIntoOneElectricalNet() {
    CableMap map;
    map.clear();
    // Electrical drawing: A6/A7 join near A and continue as one trunk to B7.
    map.setSameSideConnection(CableMapSide::A, 6U, 7U, true);
    map.setAtoBMask(7U, 1ULL << 7U);

    uint64_t visible = 0ULL;
    for (uint8_t i = 1U; i <= 8U; ++i) visible |= 1ULL << i;
    CableNet nets[kTestPointsPerSide]{};
    const size_t count = CableNetGraph::build(map, visible, nets, kTestPointsPerSide);
    assert(count == 1U);
    assert(nets[0].aMask == ((1ULL << 6U) | (1ULL << 7U)));
    assert(nets[0].bMask == (1ULL << 7U));
}



void testLiveDisplaySnapshotPreservesGroupsResultAndResistance() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);

    // Before classification, the live snapshot exposes the expected path so
    // the UI can show the active direction/pins while the contact is scanning.
    session.start();
    assert(session.displayDirection() == ScanDirection::AtoB);
    assert(session.displaySenderMask() == (1ULL << 1U));
    assert(session.displayReceiverMask() == (1ULL << 1U));
    assert(session.displayResult() == ElectricalResult::NotMeasured);
    assert(!session.displayResistanceValid());
    session.pause();

    // A8 is deliberately cross-wired to B9 in the demo cable. The result
    // snapshot must switch from expected geometry to the actually measured
    // receiver and retain the electrical result for status colouring.
    for (uint8_t measurement = 0U; measurement < 8U; ++measurement) {
        session.step();
    }
    assert(session.displaySenderMask() == (1ULL << 8U));
    assert(session.displayReceiverMask() == (1ULL << 9U));
    assert(session.displayResult() == ElectricalResult::WrongConnection);

    // A18/B18 carries the demo high-resistance fault and a truthful Kelvin
    // value. The live UI gets the same result/value pair as the report layer.
    for (uint8_t measurement = 8U; measurement < 18U; ++measurement) {
        session.step();
    }
    assert(session.displaySenderMask() == (1ULL << 18U));
    assert(session.displayReceiverMask() == (1ULL << 18U));
    assert(session.displayResult() == ElectricalResult::HighResistance);
    assert(session.displayResistanceValid());
    assert(session.displayResistanceMilliOhm() > 18.39f);
    assert(session.displayResistanceMilliOhm() < 18.41f);

    // A22/A23 is one physical shorted component. Both sender and receiver
    // masks must survive intact so the screen can render A22+A23 -> B22+B23
    // instead of silently showing only the first destination.
    for (uint8_t measurement = 18U; measurement < 22U; ++measurement) {
        session.step();
    }
    const uint64_t shortPairMask = (1ULL << 22U) | (1ULL << 23U);
    assert(session.displaySenderMask() == shortPairMask);
    assert(session.displayReceiverMask() == shortPairMask);
    assert(session.displayResult() == ElectricalResult::ShortCircuit);
}

void testEndOfTestReportStatsMatchFiniteSweep() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    const uint8_t total = session.totalMeasurements();
    for (uint8_t i = 0U; i < total; ++i) session.step();
    assert(session.runState() == RunState::Complete);

    const TestReportStats stats = buildTestReportStats(session);
    assert(stats.measured == total);
    assert(stats.ok == 40U);
    assert(stats.open == 2U);
    assert(stats.shortCircuit == 4U);
    assert(stats.wrongConnection == 4U);
    assert(stats.highResistance == 2U);
    assert(stats.errorCount() == 12U);
}


void testScrollableTableCarriesResistanceTruthfully() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    while (session.runState() != RunState::Complete) session.step();

    ReportTableRow row{};
    const uint8_t slot18 = session.profile().testIndexToSlot(18U);
    assert(slot18 != 0xFFU);
    assert(buildReportTableRow(session, ScanDirection::AtoB, slot18, row));
    assert(strcmp(row.resistance, "18.40") == 0);
    assert(strcmp(row.resistanceState, "HIGH-R") == 0);
    assert(strstr(row.result, "YUKSEK DIRENC") != nullptr);

    const uint8_t slot1 = session.profile().testIndexToSlot(1U);
    assert(buildReportTableRow(session, ScanDirection::AtoB, slot1, row));
    assert(strcmp(row.resistanceState, "OK") == 0);
    assert(strcmp(row.resistance, "-") != 0);

    const uint8_t slot15 = session.profile().testIndexToSlot(15U);
    assert(buildReportTableRow(session, ScanDirection::AtoB, slot15, row));
    assert(strcmp(row.resistance, "-") == 0);
    assert(strcmp(row.resistanceState, "N/A") == 0);

    // A multi-branch/common net cannot honestly be represented by one branch
    // resistance. Until branch-selective Kelvin sampling exists it stays N/A.
    CableProfile profile = makeNormalProfile(20U, false, false);
    CableMap common;
    common.clear();
    common.setAtoBMask(7U, (1ULL << 9U) | (1ULL << 12U));
    common.setSameSideConnection(CableMapSide::A, 7U, 9U, true);
    common.rebuildReverse();
    DemoScanSource commonSource;
    commonSource.setFaultOverlayEnabled(false);
    commonSource.setPhysicalMap(&common);
    ScanSession commonSession(profile, commonSource);
    commonSession.setCustomMap(common);
    while (commonSession.runState() != RunState::Complete) commonSession.step();
    const uint8_t slot7 = profile.testIndexToSlot(7U);
    assert(buildReportTableRow(commonSession, ScanDirection::AtoB, slot7, row));
    assert(strcmp(row.resistance, "-") == 0);
    assert(strcmp(row.resistanceState, "N/A") == 0);
}

void testDetailedReportPreservesWholeCommonNet() {
    CableProfile profile = makeNormalProfile(20U, false, false);
    CableMap common;
    common.clear();
    common.setName("COMMON NET");
    const uint64_t bGroup = (1ULL << 9U) | (1ULL << 12U) | (1ULL << 14U);
    common.setAtoBMask(7U, bGroup);
    common.setAtoBMask(9U, bGroup);
    common.setSameSideConnection(CableMapSide::A, 7U, 9U, true);
    common.setSameSideConnection(CableMapSide::B, 9U, 12U, true);
    common.setSameSideConnection(CableMapSide::B, 12U, 14U, true);
    common.rebuildReverse();

    DemoScanSource source;
    source.setFaultOverlayEnabled(false);
    source.setPhysicalMap(&common);
    ScanSession session(profile, source);
    session.setCustomMap(common);
    const uint8_t total = session.totalMeasurements();
    for (uint8_t i = 0U; i < total; ++i) session.step();
    assert(session.runState() == RunState::Complete);

    char detail[384]{};
    const uint8_t slot7 = profile.testIndexToSlot(7U);
    assert(formatDetailedReportRow(session, ScanDirection::AtoB,
                                   slot7, detail, sizeof(detail)));
    assert(strstr(detail, "A7+A9") != nullptr);
    assert(strstr(detail, "B9+B12+B14") != nullptr);
    assert(strstr(detail, "GOT A7+A9") != nullptr);
    assert(strstr(detail, "OK") != nullptr);
}

void testOperatorPhysicalFaultSummaryDeduplicatesBidirectionalRows() {
    DemoScanSource source;
    ScanSession session(defaultProfile(), source);
    while (session.runState() != RunState::Complete) session.step();

    const OperatorPhysicalFaultSummary summary = buildOperatorPhysicalFaultSummary(session);
    assert(summary.identityOneToOne);
    assert(summary.crossedPairCount == 1U);
    assert(summary.crossedPairFirst[0] == 8U);
    assert(summary.crossedPairSecond[0] == 9U);
    assert(summary.openLineCount == 1U);
    assert(summary.openLine[0] == 15U);
    assert(summary.shortGroupCount == 1U);
    assert(summary.shortGroupMask[0] == ((1ULL << 22U) | (1ULL << 23U)));
    assert(summary.highResistanceLineCount == 1U);
    assert(summary.highResistanceLine[0] == 18U);
    assert(summary.wrongLineCount == 0U);
    assert(summary.physicalIssueCount() == 4U);
}

void testOperatorPhysicalFaultSummaryMatchesSubD15Demo() {
    const CableProfile& profile = profileAt(2U);  // Sub-D15 + dS
    DemoScanSource source;
    ScanSession session(profile, source);
    while (session.runState() != RunState::Complete) session.step();

    const OperatorPhysicalFaultSummary summary = buildOperatorPhysicalFaultSummary(session);
    assert(summary.identityOneToOne);
    assert(summary.crossedPairCount == 1U);
    assert(summary.crossedPairFirst[0] == 8U);
    assert(summary.crossedPairSecond[0] == 9U);
    assert(summary.openLineCount == 1U);
    assert(summary.openLine[0] == 15U);
    assert(summary.shortGroupCount == 0U);
    assert(summary.highResistanceLineCount == 0U);
    assert(summary.wrongLineCount == 0U);
    assert(summary.physicalIssueCount() == 2U);
}

void testOperatorPhysicalFaultSummaryDoesNotGuessForCommonNets() {
    CableProfile profile = makeNormalProfile(20U, false, false);
    CableMap common;
    common.clear();
    common.setAtoBMask(7U, (1ULL << 9U) | (1ULL << 12U));
    common.setSameSideConnection(CableMapSide::A, 7U, 9U, true);
    common.rebuildReverse();

    DemoScanSource source;
    source.setFaultOverlayEnabled(false);
    source.setPhysicalMap(&common);
    ScanSession session(profile, source);
    session.setCustomMap(common);
    while (session.runState() != RunState::Complete) session.step();

    const OperatorPhysicalFaultSummary summary = buildOperatorPhysicalFaultSummary(session);
    assert(!summary.identityOneToOne);
}

}  // namespace

int main() {
    testProfilePointCounts();
    testSharedNumberRowLabels();
    testNormalProfileOptions();
    testSideSpecificPeDrainProfile();
    testNormalPinPresetCycle();
    testFixtureLabelsNeverSwap();
    testAutomaticTwoDirectionCycleAndSeparateReports();
    testDemoFaultsComeFromOnePhysicalCable();
    testNoUnusedBarsContract();
    testDirectionSpecificReportRows();
    testKnightRiderPairAndReverseOrder();
    testMidScanDirectionChangeStaysAtCurrentContact();
    testWrongConnectionDeterminesReverseStart();
    testRunningDirectionChangeReversesWithoutPausing();
    testContinuousModeRepeatsAndNormalModeCanBeRestored();
    testContinuousModeStopsOnFaultAndStartBeginsNextCable();
    testAdjustableScanSpeed();
    testCustomMapCrossRoutingAndReverseSweep();
    testCustomPhysicalCableFailsInOneToOneMode();
    testCustomMapSupportsMultipleReceivers();
    testCustomMapSupportsExpectedSameSideGroups();
    testCustomMapReverseIsAlwaysDerivedFromAtoB();
    testCableMapRawLoadRebuildsReverseAndSameSideSymmetry();
    testCableNetGraphCollapsesOneToManyIntoOneElectricalNet();
    testCableNetGraphCollapsesManyToOneIntoOneElectricalNet();
    testLiveDisplaySnapshotPreservesGroupsResultAndResistance();
    testEndOfTestReportStatsMatchFiniteSweep();
    testDetailedReportPreservesWholeCommonNet();
    testScrollableTableCarriesResistanceTruthfully();
    testOperatorPhysicalFaultSummaryDeduplicatesBidirectionalRows();
    testOperatorPhysicalFaultSummaryMatchesSubD15Demo();
    testOperatorPhysicalFaultSummaryDoesNotGuessForCommonNets();
    puts("31 host regression groups passed.");
    return 0;
}
