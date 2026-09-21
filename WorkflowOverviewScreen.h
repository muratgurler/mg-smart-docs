#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "TestWorkflowController.h"

namespace mg::p4 {

enum class WorkflowOverviewAction : uint8_t {
    None = 0,
    OpenHistory,
    Retest,
    NextCable,
    OpenSpc,
    Back,
};

class WorkflowOverviewScreen {
public:
    void begin(TestWorkflowController& controller);
    void activate();
    void update();
    WorkflowOverviewAction consumeAction();

private:
    struct Binding {
        WorkflowOverviewScreen* owner = nullptr;
        WorkflowOverviewAction action = WorkflowOverviewAction::None;
    };

    static void buttonCallback(lv_event_t* event);
    void buildUi();
    void refresh();
    lv_obj_t* makeButton(lv_obj_t* parent,
                         lv_coord_t x,
                         lv_coord_t y,
                         lv_coord_t w,
                         const char* text,
                         lv_color_t color,
                         uint8_t bindingIndex,
                         WorkflowOverviewAction action);

    TestWorkflowController* controller_ = nullptr;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* stateLabel_ = nullptr;
    lv_obj_t* sourceLabel_ = nullptr;
    lv_obj_t* profileLabel_ = nullptr;
    lv_obj_t* identityLabel_ = nullptr;
    lv_obj_t* resultLabel_ = nullptr;
    lv_obj_t* safetyLabel_ = nullptr;
    lv_obj_t* progressBar_ = nullptr;
    lv_obj_t* progressLabel_ = nullptr;
    Binding bindings_[5]{};
    lv_obj_t* retestButton_ = nullptr;
    lv_obj_t* nextCableButton_ = nullptr;
    WorkflowOverviewAction pendingAction_ = WorkflowOverviewAction::None;
};

}  // namespace mg::p4
