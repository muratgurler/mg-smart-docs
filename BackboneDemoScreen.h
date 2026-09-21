#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "FeatureBackbone.h"
#include "DigitalBackboneCore.h"
#include "QualityBackboneCore.h"
#include "HighSpeedBackboneCore.h"
#include "SystemBackboneCore.h"
#include "TestWorkflowController.h"
#include "AnalysisReportData.h"

namespace mg::p4 {

// Role-aware demo screen for the V3.3 feature backbone.
// The rendering shell is shared, but every module exposes controls that match
// that function. This keeps the UI stable while DemoDriver -> HardwareDriver
// is swapped later when the carrier PCB is available.
class BackboneDemoScreen {
public:
    explicit BackboneDemoScreen(FeatureBackbone& backbone, TestWorkflowController* workflowController = nullptr)
        : backbone_(backbone), workflowController_(workflowController) {}

    void begin(BackboneModuleId module, const char* workflowTitle = nullptr);
    void activate();
    void update();
    bool consumeBackRequest();

    // Completion edge used by the application shell to open the dedicated
    // Cable Learn report automatically. It never saves a profile or starts a
    // test; operator action remains mandatory.
    bool consumeCableLearnCompletion();
    bool consumeAnalysisReportRequest();
    const AnalysisReportData& analysisReport() const { return analysisReport_; }
    const CableProfile& cableLearnProfile() const { return digitalCore_.fullProfile(); }
    const CableMap& cableLearnMap() { return digitalCore_.learn().learnedMap(); }
    size_t cableLearnNetCount() { return digitalCore_.learn().learnedNetCount(); }
    bool cableLearnDemoMode() const { return lastSnapshot_.usingDemo; }

private:
    enum class Action : uint8_t { A0 = 0, A1, A2, A3, Back };
    struct Binding {
        BackboneDemoScreen* owner = nullptr;
        Action action = Action::A0;
    };

    static constexpr uint8_t kButtonCount = 5;

    static void buttonCallback(lv_event_t* event);
    void handle(Action action);
    void buildUi();
    void refresh(bool force = false);
    void refreshButtons(const BackboneSnapshot& snap);
    void applyDigitalCoreSnapshot(BackboneSnapshot& snap);
    void applyQualityCoreSnapshot(BackboneSnapshot& snap);
    void applyHighSpeedCoreSnapshot(BackboneSnapshot& snap);
    void applySystemCoreSnapshot(BackboneSnapshot& snap);
    void refreshContext();
    void syncWorkflowBridge();
    void buildAnalysisReport();
    void setContext(const char* fmt, ...);
    void startWorkflow(const char* turkish, const char* english);
    void startOrRestart();
    void toggleRun();

    const char* buttonText(uint8_t index, const BackboneSnapshot& snap) const;
    lv_color_t buttonColor(uint8_t index) const;

    lv_obj_t* makeButton(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                         lv_coord_t w, lv_coord_t h, lv_color_t color,
                         uint8_t bindingIndex, Action action);

    FeatureBackbone& backbone_;
    DigitalBackboneCore digitalCore_{};
    QualityBackboneCore qualityCore_{};
    HighSpeedBackboneCore highSpeedCore_{};
    SystemBackboneCore systemCore_{};
    TestWorkflowController* workflowController_ = nullptr;
    char lastWorkflowToken_[48]{};
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* titleLabel_ = nullptr;
    lv_obj_t* moduleLabel_ = nullptr;
    lv_obj_t* modeLabel_ = nullptr;
    lv_obj_t* stateLabel_ = nullptr;
    lv_obj_t* contextLabel_ = nullptr;
    lv_obj_t* phaseLabel_ = nullptr;
    lv_obj_t* progressBar_ = nullptr;
    lv_obj_t* progressLabel_ = nullptr;
    lv_obj_t* metricLabels_[3]{};
    lv_obj_t* buttons_[kButtonCount]{};
    lv_obj_t* buttonLabels_[kButtonCount]{};
    Binding bindings_[kButtonCount]{};

    bool backRequested_ = false;
    bool cableLearnCompletionPending_ = false;
    bool analysisReportPending_ = false;
    AnalysisReportData analysisReport_{};
    BackboneSnapshot lastSnapshot_{};
    bool haveSnapshot_ = false;

    // Demo-only operator selections. These are intentionally UI state, not
    // fake hardware state. Real drivers will receive equivalent selections
    // from their module controller when the PCB is connected.
    uint8_t selectedTdrPin_ = 17;
    uint8_t selectedPair_ = 1;
    uint8_t selectedPairFrequency_ = 2;  // 100 kHz
    uint8_t selectedKelvinCurrent_ = 0;  // 10/50/100 mA
    uint8_t selectedPeDsMode_ = 0;
    uint8_t selectedComponent_ = 0;
    uint8_t selectedSegment_ = 0;
    bool scanAToB_ = true;
    bool probeTone_ = true;
    bool voiceEnabled_ = true;
    bool environmentLive_ = false;
    bool environmentLogging_ = false;
    char contextText_[160]{};
};

}  // namespace mg::p4
