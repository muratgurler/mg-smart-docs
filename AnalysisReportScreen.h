#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "AnalysisReportData.h"
#include "MgResultTable.h"

namespace mg::p4 {

enum class AnalysisReportAction : uint8_t {
    None = 0,
    ReturnToMeasurement,
    ReturnToOriginMenu,
    MainMenu,
};

// Scrollable, fixed-header result table for measurements launched from
// KABLO ANALIZI / CABLE ANALYSIS. The measurement engines own the data; this
// screen only renders a frozen result snapshot and never changes test logic.
class AnalysisReportScreen {
public:
    void begin(const AnalysisReportData& report);
    void activate();
    void refresh();
    AnalysisReportAction consumeAction();

    const AnalysisReportData& report() const { return report_; }

private:
    struct Binding {
        AnalysisReportScreen* owner = nullptr;
        AnalysisReportAction action = AnalysisReportAction::None;
    };

    static void buttonCallback(lv_event_t* event);
    static MgResultStatus visualStatus(AnalysisReportStatus status);

    void buildUi();
    void refreshHeader();
    void refreshTable();
    lv_obj_t* makeButton(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                         lv_coord_t w, const char* text, lv_color_t color,
                         uint8_t bindingIndex, AnalysisReportAction action);

    AnalysisReportData report_{};
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* titleLabel_ = nullptr;
    lv_obj_t* verdictLabel_ = nullptr;
    lv_obj_t* modeLabel_ = nullptr;
    lv_obj_t* summaryLabel_ = nullptr;
    MgResultTable resultTable_{};
    lv_obj_t* buttons_[3]{};
    lv_obj_t* buttonLabels_[3]{};
    Binding bindings_[3]{};
    AnalysisReportAction pendingAction_ = AnalysisReportAction::None;

    static constexpr uint16_t kColumnCount = 6U;  // # + five module-specific columns
};

}  // namespace mg::p4
