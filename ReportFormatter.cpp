#include "ReportFormatter.h"

#include <stdio.h>
#include <string.h>

namespace mg::p4 {
namespace {

constexpr uint64_t bitFor(uint8_t index) {
    return index < 64U ? (1ULL << index) : 0ULL;
}

void formatIndex(uint8_t index, char* output, size_t outputSize) {
    if (index == kPeTestIndex) snprintf(output, outputSize, "PE");
    else if (index == kDrainShieldTestIndex) snprintf(output, outputSize, "dS");
    else if (index <= kLastSignalTestIndex) snprintf(output, outputSize, "%u", index);
    else snprintf(output, outputSize, "-");
}

void append(char* output, size_t outputSize, const char* text) {
    if (output == nullptr || outputSize == 0U || text == nullptr) return;
    const size_t used = strlen(output);
    if (used + 1U >= outputSize) return;
    snprintf(output + used, outputSize - used, "%s", text);
}

void copy(char* output, size_t outputSize, const char* text) {
    if (output == nullptr || outputSize == 0U) return;
    snprintf(output, outputSize, "%s", text != nullptr ? text : "");
}

void formatMask(uint64_t mask, char side, char* output, size_t outputSize) {
    if (output == nullptr || outputSize == 0U) return;
    output[0] = '\0';
    for (uint8_t i = 0U; i < 64U; ++i) {
        if ((mask & bitFor(i)) == 0ULL) continue;
        char index[8]{};
        formatIndex(i, index, sizeof(index));
        char item[12]{};
        snprintf(item, sizeof(item), "%c%s", side, index);
        if (output[0] != '\0') append(output, outputSize, "+");
        append(output, outputSize, item);
    }
    if (output[0] == '\0') snprintf(output, outputSize, "-");
}

uint64_t expectedSenderMask(const ScanSession& session,
                            ScanDirection direction,
                            uint8_t sourceIndex) {
    if (session.customMap() == nullptr) return bitFor(sourceIndex);
    return direction == ScanDirection::AtoB
        ? session.customMap()->senderMaskA(sourceIndex)
        : session.customMap()->senderMaskB(sourceIndex);
}

bool resistanceMeaningful(const Measurement& measurement) {
    return measurement.result == ElectricalResult::Ok ||
           measurement.result == ElectricalResult::HighResistance;
}

void formatResistance(const Measurement& measurement,
                      char* value,
                      size_t valueSize,
                      char* state,
                      size_t stateSize) {
    if (!resistanceMeaningful(measurement) || !measurement.resistanceValid) {
        copy(value, valueSize, "-");
        copy(state, stateSize, "N/A");
        return;
    }

    snprintf(value, valueSize, "%.2f", static_cast<double>(measurement.resistanceMilliOhm));
    const bool high = measurement.result == ElectricalResult::HighResistance ||
                      (measurement.resistanceLimitMilliOhm > 0.0f &&
                       measurement.resistanceMilliOhm > measurement.resistanceLimitMilliOhm);
    copy(state, stateSize, high ? "HIGH-R" : "OK");
}

void tally(TestReportStats& stats, const Measurement& measurement) {
    switch (measurement.result) {
        case ElectricalResult::Ok: ++stats.measured; ++stats.ok; break;
        case ElectricalResult::Open: ++stats.measured; ++stats.open; break;
        case ElectricalResult::ShortCircuit: ++stats.measured; ++stats.shortCircuit; break;
        case ElectricalResult::WrongConnection: ++stats.measured; ++stats.wrongConnection; break;
        case ElectricalResult::HighResistance: ++stats.measured; ++stats.highResistance; break;
        case ElectricalResult::NotMeasured: default: ++stats.notMeasured; break;
    }
    if (measurement.result != ElectricalResult::NotMeasured) {
        if (resistanceMeaningful(measurement) && measurement.resistanceValid) ++stats.resistanceMeasured;
        else ++stats.resistanceUnavailable;
    }
}

uint8_t firstSetIndex(uint64_t mask) {
    if (mask == 0ULL) return 0xFFU;
    for (uint8_t index = 0U; index < 64U; ++index) {
        if ((mask & bitFor(index)) != 0ULL) return index;
    }
    return 0xFFU;
}

uint8_t popcount64(uint64_t value) {
#if defined(__GNUC__)
    return static_cast<uint8_t>(__builtin_popcountll(value));
#else
    uint8_t count = 0U;
    while (value != 0ULL) {
        value &= value - 1ULL;
        ++count;
    }
    return count;
#endif
}

bool maskAlreadyStored(const uint64_t* masks, size_t count, uint64_t value) {
    for (size_t i = 0U; i < count; ++i) {
        if (masks[i] == value) return true;
    }
    return false;
}

}  // namespace

bool formatReportRow(const ScanSession& session,
                     ScanDirection direction,
                     uint8_t slot,
                     char* output,
                     size_t outputSize) {
    if (output == nullptr || outputSize == 0 || !session.profile().isValidSlot(slot)) return false;

    char point[4] = {};
    char expected[4] = {};
    char actual[4] = {};
    session.profile().formatSlotLabel(slot, point, sizeof(point));
    const Measurement measurement = session.reportMeasurement(direction, slot);
    formatIndex(measurement.expectedTestIndex, expected, sizeof(expected));
    formatIndex(measurement.actualTestIndex, actual, sizeof(actual));
    snprintf(output, outputSize, "%s,%s,%s,%s,%s",
             direction == ScanDirection::AtoB ? "A_TO_B" : "B_TO_A",
             point, expected, actual, electricalResultText(measurement.result));
    return true;
}

TestReportStats buildTestReportStats(const ScanSession& session) {
    TestReportStats stats{};
    for (uint8_t pass = 0U; pass < 2U; ++pass) {
        const ScanDirection direction = pass == 0U ? ScanDirection::AtoB : ScanDirection::BtoA;
        for (uint8_t slot = 0U; slot < session.profile().activePointCount(); ++slot) {
            tally(stats, session.reportMeasurement(direction, slot));
        }
    }
    return stats;
}

OperatorPhysicalFaultSummary buildOperatorPhysicalFaultSummary(const ScanSession& session) {
    OperatorPhysicalFaultSummary summary{};

    struct WrongObservation {
        uint8_t source = 0xFFU;
        uint8_t actual = 0xFFU;
        bool paired = false;
    };

    WrongObservation wrong[kTestPointsPerSide]{};
    size_t wrongCount = 0U;
    uint64_t uniqueShortGroups[kTestPointsPerSide]{};
    size_t uniqueShortGroupCount = 0U;

    // Only a strict identity one-to-one expected map can be collapsed into
    // intuitive physical defects without making assumptions about splice/common
    // nets. The A->B sweep is the canonical physical view; B->A is verification.
    summary.identityOneToOne = true;
    for (uint8_t slot = 0U; slot < session.profile().activePointCount(); ++slot) {
        const uint8_t source = session.profile().slotToTestIndex(slot);
        const Measurement m = session.reportMeasurement(ScanDirection::AtoB, slot);
        if (m.result == ElectricalResult::NotMeasured) continue;
        if (m.expectedReceiverMask != bitFor(source)) {
            summary.identityOneToOne = false;
            return summary;
        }

        switch (m.result) {
            case ElectricalResult::WrongConnection:
                if (popcount64(m.actualReceiverMask) == 1U && wrongCount < kTestPointsPerSide) {
                    wrong[wrongCount++] = {source, firstSetIndex(m.actualReceiverMask), false};
                } else {
                    ++summary.wrongLineCount;
                    if (summary.wrongLineCount <= kOperatorFaultSummaryMaxItems) {
                        const size_t out = summary.wrongLineCount - 1U;
                        summary.wrongSource[out] = source;
                        summary.wrongActual[out] = firstSetIndex(m.actualReceiverMask);
                    }
                }
                break;
            case ElectricalResult::Open:
                ++summary.openLineCount;
                if (summary.openLineCount <= kOperatorFaultSummaryMaxItems) {
                    summary.openLine[summary.openLineCount - 1U] = source;
                }
                break;
            case ElectricalResult::ShortCircuit: {
                uint64_t group = popcount64(m.senderGroupMask) > 1U
                    ? m.senderGroupMask
                    : (popcount64(m.actualReceiverMask) > 1U
                        ? m.actualReceiverMask
                        : bitFor(source));
                if (!maskAlreadyStored(uniqueShortGroups, uniqueShortGroupCount, group)) {
                    if (uniqueShortGroupCount < kTestPointsPerSide) {
                        uniqueShortGroups[uniqueShortGroupCount++] = group;
                    }
                    ++summary.shortGroupCount;
                    if (summary.shortGroupCount <= kOperatorFaultSummaryMaxItems) {
                        summary.shortGroupMask[summary.shortGroupCount - 1U] = group;
                    }
                }
                break;
            }
            case ElectricalResult::HighResistance:
                ++summary.highResistanceLineCount;
                if (summary.highResistanceLineCount <= kOperatorFaultSummaryMaxItems) {
                    summary.highResistanceLine[summary.highResistanceLineCount - 1U] = source;
                }
                break;
            case ElectricalResult::Ok:
            case ElectricalResult::NotMeasured:
            default:
                break;
        }
    }

    // Two opposite wrong rows A8->B9 and A9->B8 are one crossed physical pair,
    // not four independent faults after the reverse sweep is counted as well.
    for (size_t i = 0U; i < wrongCount; ++i) {
        if (wrong[i].paired) continue;
        for (size_t j = i + 1U; j < wrongCount; ++j) {
            if (wrong[j].paired) continue;
            if (wrong[i].actual == wrong[j].source &&
                wrong[j].actual == wrong[i].source) {
                wrong[i].paired = true;
                wrong[j].paired = true;
                ++summary.crossedPairCount;
                if (summary.crossedPairCount <= kOperatorFaultSummaryMaxItems) {
                    const size_t out = summary.crossedPairCount - 1U;
                    if (wrong[i].source <= wrong[j].source) {
                        summary.crossedPairFirst[out] = wrong[i].source;
                        summary.crossedPairSecond[out] = wrong[j].source;
                    } else {
                        summary.crossedPairFirst[out] = wrong[j].source;
                        summary.crossedPairSecond[out] = wrong[i].source;
                    }
                }
                break;
            }
        }
    }

    for (size_t i = 0U; i < wrongCount; ++i) {
        if (wrong[i].paired) continue;
        ++summary.wrongLineCount;
        if (summary.wrongLineCount <= kOperatorFaultSummaryMaxItems) {
            const size_t out = summary.wrongLineCount - 1U;
            summary.wrongSource[out] = wrong[i].source;
            summary.wrongActual[out] = wrong[i].actual;
        }
    }

    return summary;
}

bool formatDetailedReportRow(const ScanSession& session,
                             ScanDirection direction,
                             uint8_t slot,
                             char* output,
                             size_t outputSize) {
    if (output == nullptr || outputSize == 0U || !session.profile().isValidSlot(slot)) return false;
    const Measurement m = session.reportMeasurement(direction, slot);
    if (m.result == ElectricalResult::NotMeasured) return false;

    const uint8_t sourceIndex = session.profile().slotToTestIndex(slot);
    const uint64_t expectedSender = expectedSenderMask(session, direction, sourceIndex);
    const char sourceSide = direction == ScanDirection::AtoB ? 'A' : 'B';
    const char receiverSide = direction == ScanDirection::AtoB ? 'B' : 'A';

    char expSender[150]{};
    char expReceiver[150]{};
    char gotSender[150]{};
    char gotReceiver[150]{};
    formatMask(expectedSender, sourceSide, expSender, sizeof(expSender));
    formatMask(m.expectedReceiverMask, receiverSide, expReceiver, sizeof(expReceiver));
    formatMask(m.senderGroupMask, sourceSide, gotSender, sizeof(gotSender));
    formatMask(m.actualReceiverMask, receiverSide, gotReceiver, sizeof(gotReceiver));

    snprintf(output, outputSize, "%s <-> %s | GOT %s <-> %s | %s",
             expSender, expReceiver, gotSender, gotReceiver,
             electricalResultText(m.result));
    return true;
}

bool buildReportTableRow(const ScanSession& session,
                         ScanDirection direction,
                         uint8_t slot,
                         ReportTableRow& row) {
    row = {};
    if (!session.profile().isValidSlot(slot)) return false;
    const Measurement measurement = session.reportMeasurement(direction, slot);
    if (measurement.result == ElectricalResult::NotMeasured) return false;

    const uint8_t sourceIndex = session.profile().slotToTestIndex(slot);
    const uint64_t expectedSender = expectedSenderMask(session, direction, sourceIndex);
    const char sourceSide = direction == ScanDirection::AtoB ? 'A' : 'B';
    const char receiverSide = direction == ScanDirection::AtoB ? 'B' : 'A';

    char gotSender[128]{};
    char gotReceiver[160]{};
    formatMask(expectedSender, sourceSide, row.source, sizeof(row.source));
    formatMask(measurement.expectedReceiverMask, receiverSide, row.expected, sizeof(row.expected));
    formatMask(measurement.senderGroupMask, sourceSide, gotSender, sizeof(gotSender));
    formatMask(measurement.actualReceiverMask, receiverSide, gotReceiver, sizeof(gotReceiver));
    row.actual[0] = '\0';
    append(row.actual, sizeof(row.actual), gotSender);
    append(row.actual, sizeof(row.actual), " <-> ");
    append(row.actual, sizeof(row.actual), gotReceiver);
    formatResistance(measurement,
                     row.resistance, sizeof(row.resistance),
                     row.resistanceState, sizeof(row.resistanceState));
    copy(row.result, sizeof(row.result), electricalResultText(measurement.result));
    return true;
}

}  // namespace mg::p4
