#pragma once

#include <lvgl.h>

#include "P4Localization.h"
#include "SoundToggleButton.h"

namespace mg::p4 {

class LanguageScreen {
public:
    void begin();
    bool consumeSelection(P4Language& language);
    bool consumeBackRequest();

private:
    struct LanguageBinding {
        LanguageScreen* owner = nullptr;
        P4Language language = P4Language::Turkish;
    };

    static void languageCallback(lv_event_t* event);
    static void backCallback(lv_event_t* event);

    lv_obj_t* screen_ = nullptr;
    LanguageBinding bindings_[static_cast<uint8_t>(P4Language::Count)]{};
    bool selectionPending_ = false;
    bool backRequested_ = false;
    P4Language selectedLanguage_ = P4Language::Turkish;
    SoundToggleButton soundButton_;
};

}  // namespace mg::p4
