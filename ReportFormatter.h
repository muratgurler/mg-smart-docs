#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ScanSession.h"

namespace mg::p4 {

struct TestReportStats {
    uint16_t measured = 0U;
    uint16_t ok = 0U;
    uint16_t open = 0U;
    uint16_t shortCircuit = 0U;
    uint16_t wrongConnection = 0U;
    uint16_t highResistance = 0U;
    uint16_t notMeasured = 0U;
    uint16_t resistanceMeasured = 0U;
    uint16_t resistanceUnavailable = 0U;

    uint16_t errorCount() const {
        return static_cast<uint16_t>(open + shortCircuit + wrongConnection + highResistance);
    }
};

static constexpr size_t kOperatorFaultSummaryMaxItems = 8U;

struct OperatorPhysicalFaultSummary {
    bool identityOneToOne = false;

    uint16_t crossedPairCount = 0U;
    uint8_t crossedPairFirst[kOperatorFaultSummaryMaxItems] = {};
    uint8_t crossedPairSecond[kOperatorFaultSummaryMaxItems] = {};

    uint16_t openLineCount = 0U;
    uint8_t openLine[kOperatorFaultSummaryMaxItems] = {};

    uint16_t shortGroupCount = 0U;
    uint64_t shortGroupMask[kOperatorFaultSummaryMaxItems] = {};

    uint16_t highResistanceLineCount = 0U;
    uint8_t highResistanceLine[kOperatorFaultSummaryMaxItems] = {};

    uint16_t wrongLineCount = 0U;
    uint8_t wrongSource[kOperatorFaultSummaryMaxItems] = {};
    uint8_t wrongActual[kOperatorFaultSummaryMaxItems] = {};

    uint16_t physicalIssueCount() const {
        return static_cast<uint16_t>(crossedPairCount + openLineCount +
                                     shortGroupCount + highResistanceLineCount +
                                     wrongLineCount);
    }
};

struct ReportTableRow {
    char source[128] = {};
    char expected[160] = {};
    char actual[192] = {};
    char resistance[32] = {};
    char resistanceState[24] = {};
    char result[32] = {};
};

// Produces one direction-specific CSV row without allocating String objects.
// Legacy CSV format is deliberately unchanged:
// DIRECTION,POINT,EXPECTED,ACTUAL,RESULT
bool formatReportRow(const ScanSession& session,
                     ScanDirection direction,
                     uint8_t slot,
                     char* output,
                     size_t outputSize);

// Full end-of-test aggregation over both A->B and B->A sweeps.
TestReportStats buildTestReportStats(const ScanSession& session);

// Operator-facing de-duplication for simple identity 1:1 profiles. The test
// engine intentionally scans A->B and B->A, so raw report counts contain the
// same physical defect twice (and a crossed pair four times). This helper
// collapses those directional rows into physical issues only when the expected
// topology is a strict identity 1:1 map. Complex/common nets deliberately fall
// back to the raw directional statistics instead of inventing a physical count.
OperatorPhysicalFaultSummary buildOperatorPhysicalFaultSummary(const ScanSession& session);

// Human-readable electrical-net row. For custom/common nets this keeps all
// expected and observed members, e.g.:
// A7+A9 <-> B9+B12+B14 | GOT A7+A9 <-> B9+B12 | ACIK
bool formatDetailedReportRow(const ScanSession& session,
                             ScanDirection direction,
                             uint8_t slot,
                             char* output,
                             size_t outputSize);

// Structured data for the scrollable completion-report table. Resistance is
// optional: real hardware fills Measurement::resistance* from the Kelvin path;
// otherwise the row explicitly reports N/A instead of inventing a value.
bool buildReportTableRow(const ScanSession& session,
                         ScanDirection direction,
                         uint8_t slot,
                         ReportTableRow& row);

}  // namespace mg::p4
