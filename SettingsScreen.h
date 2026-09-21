#pragma once
#include <lvgl.h>
#include <stdint.h>
#include "SoundToggleButton.h"
namespace mg::p4 {
enum class SettingsAction : uint8_t { None, TouchDiagnostic, Wifi, Ethernet, WorkplaceServer, HardwareStatus, Back };
class SettingsScreen {
public:
    void begin();
    void activate();
    SettingsAction consumeAction();
private:
    struct Binding { SettingsScreen* owner=nullptr; SettingsAction action=SettingsAction::None; };
    static void callback(lv_event_t* event);
    lv_obj_t* addButton(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, const char* title,
                        const char* subtitle, lv_color_t color, SettingsAction action,
                        bool enabled, uint8_t index);
    void request(SettingsAction action);
    lv_obj_t* screen_=nullptr;
    Binding bindings_[6]{};
    SoundToggleButton soundButton_;
    SettingsAction pendingAction_=SettingsAction::None;
};
} // namespace mg::p4
