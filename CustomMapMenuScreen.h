#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <stdint.h>

#include "SoundToggleButton.h"

namespace mg::p4 {

enum class CustomMapMenuAction : uint8_t {
    None,
    Test,
    Edit,
    Back,
};

class CustomMapMenuScreen {
public:
    void begin(const String& webUrl, bool webAvailable);
    void activate();
    void updateWebInfo(const String& webUrl, bool webAvailable);
    CustomMapMenuAction consumeAction();

private:
    struct Binding {
        CustomMapMenuScreen* owner = nullptr;
        CustomMapMenuAction action = CustomMapMenuAction::None;
    };

    static void callback(lv_event_t* event);
    lv_obj_t* addButton(lv_obj_t* parent,
                        lv_coord_t x,
                        lv_coord_t y,
                        lv_coord_t width,
                        const char* title,
                        const char* subtitle,
                        lv_color_t color,
                        CustomMapMenuAction action,
                        uint8_t index);
    void request(CustomMapMenuAction action);
    void refreshWebInfo(const String& webUrl, bool webAvailable);

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* webStatusLabel_ = nullptr;
    Binding bindings_[3]{};
    SoundToggleButton soundButton_;
    CustomMapMenuAction pendingAction_ = CustomMapMenuAction::None;
};

}  // namespace mg::p4
