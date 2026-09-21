#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "SoundToggleButton.h"

namespace mg::p4 {

enum class MainMenuAction : uint8_t {
    None,
    NormalScan,
    SubDScan,
    AdvancedAnalysis,
    ExtraFeatures,
    Settings,
    CableLearn,
    QrBarcode,
    Reports,
    MultiConnector,
    Language,
};

class MainMenuScreen {
public:
    void begin();
    void activate();
    MainMenuAction consumeAction();

private:
    struct MenuBinding {
        MainMenuScreen* owner = nullptr;
        MainMenuAction action = MainMenuAction::None;
    };

    static void menuCallback(lv_event_t* event);
    lv_obj_t* createMenuButton(lv_obj_t* parent,
                               lv_coord_t x,
                               lv_coord_t y,
                               const char* title,
                               const char* subtitle,
                               lv_color_t color,
                               MainMenuAction action,
                               bool enabled,
                               uint8_t bindingIndex);
    void request(MainMenuAction action);

    lv_obj_t* screen_ = nullptr;
    MenuBinding bindings_[10]{};
    SoundToggleButton soundButton_;
    MainMenuAction pendingAction_ = MainMenuAction::None;
};

}  // namespace mg::p4
