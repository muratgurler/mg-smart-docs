#include <cassert>
#include <cstddef>
#include <iostream>

#include "CableMap.h"
#include "CableProfile.h"
#include "DigitalBackboneCore.h"
#include "ScanSource.h"

using namespace mg::p4;

int main() {
    DigitalBackboneCore core;
    auto& runner = core.scan();

    // The full-cable analysis sweep must retain every enabled point in natural
    // A->B then B->A order for the scrollable report. This also guards the
    // historical accidental double cursor increment.
    runner.start();
    for (unsigned guard = 0; guard < 1000U &&
         runner.state() != DigitalWorkflowState::Complete &&
         runner.state() != DigitalWorkflowState::Fault; ++guard) {
        runner.step();
    }

    const auto& stats = runner.stats();
    assert(stats.completed == stats.total);
    assert(runner.reportEntryCount() == stats.total);
    assert(stats.total == 128U);

    const auto& first = runner.reportEntry(0U);
    const auto& lastA = runner.reportEntry(63U);
    const auto& firstB = runner.reportEntry(64U);
    const auto& last = runner.reportEntry(127U);

    assert(first.direction == ScanDirection::AtoB && first.source == 0U);
    assert(lastA.direction == ScanDirection::AtoB && lastA.source == 63U);
    assert(firstB.direction == ScanDirection::BtoA && firstB.source == 0U);
    assert(last.direction == ScanDirection::BtoA && last.source == 63U);

    // Report entries carry the same resistance payload used by the normal
    // completion report. The demo high-R point must therefore be renderable.
    bool sawResistance = false;
    bool sawHighR = false;
    for (size_t i = 0U; i < runner.reportEntryCount(); ++i) {
        const auto& e = runner.reportEntry(i);
        if (e.measurement.resistanceValid) sawResistance = true;
        if (e.measurement.result == ElectricalResult::HighResistance) {
            sawHighR = true;
            assert(e.measurement.resistanceValid);
            assert(e.measurement.resistanceMilliOhm > e.measurement.resistanceLimitMilliOhm);
        }
    }
    assert(sawResistance);
    assert(sawHighR);

    std::cout << "Analysis report batch13 data tests passed.\n";
    return 0;
}
