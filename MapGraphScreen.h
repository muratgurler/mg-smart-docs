#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "CableMap.h"
#include "CableProfile.h"
#include "ScanSession.h"
#include "SoundToggleButton.h"

namespace mg::p4 {

enum class ResultGraphView : uint8_t {
    Combined = 0,
    Expected,
    Measured,
};

enum class ResultGraphFilter : uint8_t {
    All = 0,
    Errors,
    Ok,
    Open,
    ShortCircuit,
    WrongConnection,
    HighResistance,
};

class MapGraphScreen {
public:
    // Editor/learned-map graph. returnToCompletionReport is used by Cable Learn
    // so the same approved graph renderer can be reused from its result page.
    void begin(const CableMap& map,
               const CableProfile& profile,
               bool returnToCompletionReport = false);

    // Finite scan result graph. NON-STOP never reaches the completion report.
    void beginResults(const ScanSession& session);

    void activate();
    // After any graph is first presented, force two subsequent
    // full-refresh cycles so both P4 display framebuffers receive the complete
    // pin-label layer without requiring operator interaction.
    void servicePostPresentRefresh();
    bool consumeBackRequest();
    bool returnsToCompletionReport() const { return returnToCompletionReport_; }
    bool resultMode() const { return resultMode_; }

private:
    enum class ResultControl : uint8_t {
        ViewCombined = 0,
        ViewExpected,
        ViewMeasured,
        FilterAll,
        FilterErrors,
        FilterOk,
        FilterOpen,
        FilterShort,
        FilterWrong,
        FilterHighR,
    };

    struct ResultBinding {
        MapGraphScreen* owner = nullptr;
        ResultControl control = ResultControl::ViewCombined;
    };

    static void drawCallback(lv_event_t* event);
    static void backCallback(lv_event_t* event);
    static void resultControlCallback(lv_event_t* event);

    void build();
    void buildEditorFooter(lv_obj_t* footer);
    void buildResultFooter(lv_obj_t* footer);
    lv_obj_t* makeResultButton(lv_obj_t* parent,
                               lv_coord_t x,
                               lv_coord_t y,
                               lv_coord_t w,
                               lv_coord_t h,
                               const char* text,
                               ResultControl control,
                               uint8_t bindingIndex);
    void refresh();
    void presentPreparedScreen();
    void refreshResultControls();
    void drawGraph(lv_draw_ctx_t* drawCtx, lv_obj_t* graphObject);
    void drawResultGraph(lv_draw_ctx_t* drawCtx, lv_obj_t* graphObject);
    void formatPin(uint8_t testIndex, char* output, size_t outputSize) const;
    uint8_t visibleTestIndex(uint8_t visibleSlot) const;
    int16_t pointY(uint8_t visibleSlot, uint8_t visibleCount, lv_coord_t height) const;
    uint64_t visibleMask() const;
    bool resultMatchesFilter(ElectricalResult result) const;
    ElectricalResult worstResultForPoint(bool sideA, uint8_t testIndex) const;
    static lv_color_t resultColor(ElectricalResult result);
    static uint8_t resultSeverity(ElectricalResult result);

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* graphObject_ = nullptr;
    lv_obj_t* subtitleLabel_ = nullptr;
    lv_obj_t* countLabel_ = nullptr;
    lv_obj_t* leftLabels_[kTestPointsPerSide]{};
    lv_obj_t* rightLabels_[kTestPointsPerSide]{};
    lv_obj_t* viewButtons_[3]{};
    lv_obj_t* filterButtons_[7]{};
    ResultBinding resultBindings_[10]{};

    SoundToggleButton soundButton_;
    CableMap map_;
    CableProfile profile_{};
    const ScanSession* resultSession_ = nullptr;
    ResultGraphView resultView_ = ResultGraphView::Combined;
    ResultGraphFilter resultFilter_ = ResultGraphFilter::All;
    bool resultMode_ = false;
    bool returnToCompletionReport_ = false;
    bool filterVisible_ = false;
    bool backRequested_ = false;
    uint8_t postPresentRefreshPasses_ = 0U;
    lv_coord_t graphY_ = 94;
    lv_coord_t graphHeight_ = 404;
};

}  // namespace mg::p4
