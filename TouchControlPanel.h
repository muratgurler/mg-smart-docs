#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "FrontPanelController.h"
#include "ScanSession.h"
#include "TestWorkflowController.h"

namespace mg::p4 {

// Controls that must remain reachable while the scan screen is active.
// There is deliberately no secondary control page: scan operations belong
// on the scan screen and diagnostics belong on the main menu.
class TouchControlPanel {
public:
    TouchControlPanel(FrontPanelController& controller,
                      ScanSession& session,
                      TestWorkflowController& workflow)
        : controller_(controller), session_(session), workflow_(workflow) {}

    void begin(lv_obj_t* parent);
    void refresh();
    bool consumeMainMenuRequest();

private:
    struct ButtonBinding {
        TouchControlPanel* owner = nullptr;
        ControlAction action = ControlAction::AutoStartPause;
    };

    static void mainMenuCallback(lv_event_t* event);
    static void actionCallback(lv_event_t* event);
    static void stepButtonCallback(lv_event_t* event);

    lv_obj_t* createButton(lv_obj_t* parent,
                           lv_coord_t x,
                           lv_coord_t y,
                           lv_coord_t width,
                           lv_coord_t height,
                           const char* text,
                           lv_event_cb_t callback,
                           void* userData,
                           lv_obj_t** labelOut = nullptr,
                           const lv_font_t* font = nullptr,
                           lv_event_code_t eventCode = LV_EVENT_CLICKED);
    void handleAction(ControlAction action);
    void beginStepPress();
    void updateStepHold();
    void endStepPress(bool releasedNormally);
    void refreshButtonStates();

    FrontPanelController& controller_;
    ScanSession& session_;
    TestWorkflowController& workflow_;
    lv_obj_t* mainMenuButton_ = nullptr;
    lv_obj_t* nextCableButton_ = nullptr;
    lv_obj_t* quickActionButtons_[4]{};
    lv_obj_t* quickActionLabels_[4]{};
    lv_obj_t* mainMenuLabel_ = nullptr;
    lv_obj_t* nextCableLabel_ = nullptr;
    ButtonBinding quickBindings_[4]{};
    ButtonBinding nextCableBinding_{};
    bool mainMenuRequested_ = false;
    bool stepPressActive_ = false;
    bool stepHoldRunning_ = false;
    uint32_t stepPressedAtMs_ = 0;
    uint8_t stepSavedSpeedPercent_ = kDefaultTestSpeedPercent;
};

}  // namespace mg::p4
