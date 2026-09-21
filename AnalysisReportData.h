#pragma once

#include <stddef.h>
#include <stdint.h>

#include "FeatureBackbone.h"

namespace mg::p4 {

enum class AnalysisReportStatus : uint8_t {
    Info = 0,
    Pass,
    Open,
    ShortCircuit,
    WrongConnection,
    HighResistance,
    Warning,
    Fail,
    NotMeasured,
};

struct AnalysisReportRow {
    char cells[5][64]{};
    AnalysisReportStatus status = AnalysisReportStatus::Info;
};

struct AnalysisReportData {
    static constexpr size_t kMaxRows = 128U;

    BackboneModuleId module = BackboneModuleId::Scan128;
    bool demoMode = true;
    bool liveSnapshot = false;
    char title[64]{};
    char summary[160]{};
    char columns[5][28]{};
    AnalysisReportRow rows[kMaxRows]{};
    size_t rowCount = 0U;
};

}  // namespace mg::p4
