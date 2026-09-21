#pragma once

#include <lvgl.h>

#include "V33HardwareManager.h"
#include "FeatureBackbone.h"

namespace mg::p4 {

class AdvancedAnalysisScreen {
public:
    void begin(v33::V33HardwareManager& hardware);
    void activate();
    void update(v33::V33HardwareManager& hardware);
    bool consumeBackRequest();
    bool consumeConnectionScanRequest();
    bool consumeBackboneRequest(BackboneModuleId& module, const char*& workflowTitle);

private:
    enum class OperatorAction : uint8_t {
        FullCableTest = 0,
        ConnectionFaults,
        Resistance,
        CrimpContact,
        IntermittentContact,
        FindOpenLocation,
        TwistedPair,
        Calibration
    };

    struct ActionContext {
        AdvancedAnalysisScreen* owner;
        OperatorAction action;
    };

    static void backCallback(lv_event_t* event);
    static void actionCallback(lv_event_t* event);
    static void closeDialogCallback(lv_event_t* event);

    lv_obj_t* addActionButton(lv_obj_t* parent,
                              lv_coord_t x,
                              lv_coord_t y,
                              lv_coord_t w,
                              lv_coord_t h,
                              const char* title,
                              const char* subtitle,
                              lv_color_t color,
                              OperatorAction action);

    void showActionDialog(OperatorAction action);
    void closeActionDialog();

    const char* actionTitle(OperatorAction action) const;
    const char* actionSubtitle(OperatorAction action) const;
    const char* actionInstructions(OperatorAction action) const;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* dialogOverlay_ = nullptr;
    bool backRequested_ = false;
    bool connectionScanRequested_ = false;
    bool backboneRequested_ = false;
    BackboneModuleId requestedModule_ = BackboneModuleId::Kelvin;
    const char* requestedWorkflowTitle_ = nullptr;
    ActionContext actionContexts_[8]{};
};

}  // namespace mg::p4
