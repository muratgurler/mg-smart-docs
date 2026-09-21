#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "FeatureBackbone.h"

namespace mg::p4 {

class ExtraFeaturesScreen {
public:
    void begin();
    void activate();
    bool consumeBackRequest();
    bool consumeDocumentImportRequest();
    bool consumeBackboneRequest(BackboneModuleId& module, const char*& workflowTitle);

private:
    enum class ExtraAction : uint8_t {
        SelfTest = 0,
        DocumentImport,
        SmartProbe,
        ShieldQuality,
        SpcTrend,
        VoiceSound,
        EnvironmentAwg,
        UsbComponent
    };

    struct ActionContext {
        ExtraFeaturesScreen* owner = nullptr;
        ExtraAction action = ExtraAction::SelfTest;
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
                              ExtraAction action);
    void showActionDialog(ExtraAction action);
    void closeActionDialog();
    const char* actionTitle(ExtraAction action) const;
    const char* actionSubtitle(ExtraAction action) const;
    const char* actionInstructions(ExtraAction action) const;
    const char* actionStatus(ExtraAction action) const;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* dialogOverlay_ = nullptr;
    bool backRequested_ = false;
    bool documentImportRequested_ = false;
    bool backboneRequested_ = false;
    BackboneModuleId requestedModule_ = BackboneModuleId::SelfTestCalibration;
    const char* requestedWorkflowTitle_ = nullptr;
    ActionContext actionContexts_[8]{};
};

}  // namespace mg::p4
