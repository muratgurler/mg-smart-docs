#include "LanguageScreen.h"

#include <Arduino.h>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4LanguageVisuals.h"

namespace mg::p4 {

namespace {

lv_obj_t* addLanguageLabel(lv_obj_t* parent,
                           const char* text,
                           const lv_font_t* font,
                           lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

constexpr P4Language kLanguageDisplayOrder[] = {
    P4Language::English,
    P4Language::Dutch,
    P4Language::Turkish,
    P4Language::German,
    P4Language::French,
    P4Language::Spanish,
    P4Language::Polish,
};

static_assert(sizeof(kLanguageDisplayOrder) /
                      sizeof(kLanguageDisplayOrder[0]) ==
                  static_cast<uint8_t>(P4Language::Count),
              "Language display order must contain every language");

}  // namespace

void LanguageScreen::begin() {
    selectionPending_ = false;
    backRequested_ = false;
    if (screen_ == nullptr) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    lv_scr_load(screen_);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* title = addLanguageLabel(screen_,
                                       p4Texts().selectLanguage,
                                       p4Font20(),
                                       lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    for (uint8_t index = 0;
         index < static_cast<uint8_t>(P4Language::Count);
         ++index) {
        const P4Language language = kLanguageDisplayOrder[index];
        bindings_[index].owner = this;
        bindings_[index].language = language;
        const lv_coord_t column = static_cast<lv_coord_t>(index % 2U);
        const lv_coord_t row = static_cast<lv_coord_t>(index / 2U);
        lv_obj_t* button = lv_btn_create(screen_);
        lv_obj_set_pos(button,
                       static_cast<lv_coord_t>(104 + column * 426),
                       static_cast<lv_coord_t>(82 + row * 106));
        lv_obj_set_size(button, 390, 84);
        lv_obj_set_style_bg_color(
            button,
            language == currentP4Language()
                ? lv_color_hex(0x16834D)
                : lv_color_hex(0x245A78),
            LV_PART_MAIN);
        lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(button,
                            languageCallback,
                            LV_EVENT_CLICKED,
                            &bindings_[index]);
        lv_obj_t* rowContent = lv_obj_create(button);
        lv_obj_set_size(rowContent, 344, 44);
        lv_obj_align(rowContent, LV_ALIGN_LEFT_MID, 20, 0);
        lv_obj_clear_flag(rowContent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(rowContent, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_pad_all(rowContent, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_column(rowContent, 10, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(rowContent, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(rowContent, 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(rowContent, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(rowContent,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        createLanguageFlag(rowContent, language, 0, 0);
        addLanguageLabel(rowContent,
                         p4LanguageName(language),
                         p4Font20(),
                         lv_color_hex(0xFFFFFF));
    }

    lv_obj_t* backButton = lv_btn_create(screen_);
    lv_obj_set_pos(backButton, 744, 506);
    lv_obj_set_size(backButton, 176, 66);
    lv_obj_set_style_bg_color(backButton, lv_color_hex(0x596772), LV_PART_MAIN);
    lv_obj_set_style_radius(backButton, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(backButton, backCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* backLabel = addLanguageLabel(backButton,
                                           p4Texts().back,
                                           p4Font20(),
                                           lv_color_hex(0xFFFFFF));
    lv_obj_center(backLabel);
    soundButton_.create(screen_, 970, 18, 42);
    Serial.println("[LANGUAGE] Selection screen ready");
    Serial.flush();
}

void LanguageScreen::languageCallback(lv_event_t* event) {
    auto* binding = static_cast<LanguageBinding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->owner == nullptr) {
        return;
    }
    binding->owner->selectedLanguage_ = binding->language;
    binding->owner->selectionPending_ = true;
}

void LanguageScreen::backCallback(lv_event_t* event) {
    auto* self = static_cast<LanguageScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->backRequested_ = true;
    }
}

bool LanguageScreen::consumeSelection(P4Language& language) {
    if (!selectionPending_) {
        return false;
    }
    selectionPending_ = false;
    language = selectedLanguage_;
    return true;
}

bool LanguageScreen::consumeBackRequest() {
    const bool requested = backRequested_;
    backRequested_ = false;
    return requested;
}

}  // namespace mg::p4
