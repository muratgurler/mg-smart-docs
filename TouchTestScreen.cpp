#include "TouchTestScreen.h"

#include <Arduino.h>
#include <stdlib.h>

#include "ConfigP4.h"
#include "P4Fonts.h"

namespace mg::p4 {

const TouchTestScreen::TargetPoint TouchTestScreen::kTargets[kTargetCount] = {
    {90, 135, "SOL ÜST"},
    {934, 135, "SAĞ ÜST"},
    {512, 300, "MERKEZ"},
    {90, 480, "SOL ALT"},
    {934, 480, "SAĞ ALT"},
};

namespace {

lv_obj_t* addText(lv_obj_t* parent,
                  const char* text,
                  const lv_font_t* font,
                  lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

void makeFlatObject(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

}  // namespace

void TouchTestScreen::begin() {
    currentTarget_ = 0;
    passedTargets_ = 0;
    pressCaptured_ = false;
    complete_ = false;
    continueRequested_ = false;

    if (screen_ == nullptr) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    lv_scr_load(screen_);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 68);
    makeFlatObject(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* title = addText(header,
                              "GT911 DOKUNMATİK DOĞRULAMA",
                              p4Font20(),
                              lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    instructionLabel_ = addText(
        header,
        "Mavi hedefin merkezine dokunun ve parmağınızı kaldırın",
        p4Font14(),
        lv_color_hex(0x9CC6DC));
    lv_obj_align(instructionLabel_, LV_ALIGN_BOTTOM_MID, 0, -7);

    target_ = lv_obj_create(screen_);
    lv_obj_set_size(target_, 92, 92);
    lv_obj_set_style_radius(target_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(target_, lv_color_hex(0x123A52), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(target_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(target_, lv_color_hex(0x36C7FF), LV_PART_MAIN);
    lv_obj_set_style_border_width(target_, 5, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(target_, lv_color_hex(0x36C7FF), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(target_, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(target_, LV_OPA_40, LV_PART_MAIN);
    lv_obj_clear_flag(target_, LV_OBJ_FLAG_SCROLLABLE);

    targetPlus_ = addText(target_,
                          "+",
                          &lv_font_montserrat_48,
                          lv_color_hex(0xFFFFFF));
    lv_obj_center(targetPlus_);

    counterLabel_ = addText(screen_,
                            "HEDEF 1/5",
                            p4Font20(),
                            lv_color_hex(0x69D991));
    lv_obj_set_width(counterLabel_, 420);
    lv_obj_set_style_text_align(counterLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(counterLabel_, 302, 76);

    coordinateLabel_ = addText(screen_,
                               "BEKLENEN: --,--    OKUNAN: --,--",
                               p4Font14(),
                               lv_color_hex(0xD2E3ED));
    lv_obj_set_width(coordinateLabel_, 700);
    lv_obj_set_style_text_align(coordinateLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(coordinateLabel_, 162, 548);

    resultLabel_ = addText(screen_,
                           "Dokunmatik başlatıldı. Beş noktalı test bekleniyor.",
                           p4Font14(),
                           lv_color_hex(0xAEBFCA));
    lv_obj_set_width(resultLabel_, 800);
    lv_obj_set_style_text_align(resultLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(resultLabel_, 112, 574);

    crossHorizontal_ = lv_obj_create(screen_);
    lv_obj_set_size(crossHorizontal_, 45, 3);
    makeFlatObject(crossHorizontal_);
    lv_obj_set_style_bg_color(crossHorizontal_, lv_color_hex(0xFFD34D), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(crossHorizontal_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(crossHorizontal_, LV_OBJ_FLAG_HIDDEN);

    crossVertical_ = lv_obj_create(screen_);
    lv_obj_set_size(crossVertical_, 3, 45);
    makeFlatObject(crossVertical_);
    lv_obj_set_style_bg_color(crossVertical_, lv_color_hex(0xFFD34D), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(crossVertical_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(crossVertical_, LV_OBJ_FLAG_HIDDEN);

    continueButton_ = lv_btn_create(screen_);
    lv_obj_set_size(continueButton_, 330, 78);
    lv_obj_align(continueButton_, LV_ALIGN_CENTER, 0, 72);
    lv_obj_set_style_bg_color(continueButton_, lv_color_hex(0x167A49), LV_PART_MAIN);
    lv_obj_set_style_radius(continueButton_, 12, LV_PART_MAIN);
    lv_obj_add_flag(continueButton_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(continueButton_, continueEventCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* continueText = addText(continueButton_,
                                     "TARAMA EKRANINA GEÇ",
                                     p4Font20(),
                                     lv_color_hex(0xFFFFFF));
    lv_obj_center(continueText);

    // This transparent, full-screen object records every tap, even if the
    // coordinate transform is wrong and the visible target is missed.
    captureOverlay_ = lv_obj_create(screen_);
    lv_obj_set_pos(captureOverlay_, 0, 0);
    lv_obj_set_size(captureOverlay_, kDisplayWidth, kDisplayHeight);
    makeFlatObject(captureOverlay_);
    lv_obj_set_style_bg_opa(captureOverlay_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(captureOverlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(captureOverlay_, touchEventCallback, LV_EVENT_ALL, this);

    // Created last so it remains usable above the full-screen capture object.
    soundButton_.create(screen_, 970, 14, 42);

    showCurrentTarget();
    Serial.println("[TOUCHTEST] Five-point verification ready; K closes the test");
    Serial.flush();
}

void TouchTestScreen::touchEventCallback(lv_event_t* event) {
    auto* self = static_cast<TouchTestScreen*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->handleTouchEvent(lv_event_get_code(event));
    }
}

void TouchTestScreen::continueEventCallback(lv_event_t* event) {
    auto* self = static_cast<TouchTestScreen*>(lv_event_get_user_data(event));
    if (self != nullptr && lv_event_get_code(event) == LV_EVENT_CLICKED) {
        self->continueRequested_ = true;
        Serial.println("[TOUCHTEST] Continue button pressed");
        Serial.flush();
    }
}

void TouchTestScreen::handleTouchEvent(lv_event_code_t code) {
    if (complete_) {
        return;
    }

    lv_indev_t* input = lv_indev_get_act();
    if (input == nullptr) {
        return;
    }

    lv_point_t point{};
    lv_indev_get_point(input, &point);

    if (code == LV_EVENT_PRESSED) {
        pressedPoint_ = point;
        pressCaptured_ = true;
        showTouchPoint(point.x, point.y);
        Serial.printf("[TOUCHTEST][PRESS] target=%u %s expected=%d,%d actual=%d,%d\n",
                      static_cast<unsigned>(currentTarget_ + 1U),
                      kTargets[currentTarget_].name,
                      static_cast<int>(kTargets[currentTarget_].x),
                      static_cast<int>(kTargets[currentTarget_].y),
                      static_cast<int>(point.x),
                      static_cast<int>(point.y));
        Serial.flush();
        return;
    }

    if (code == LV_EVENT_PRESSING && pressCaptured_) {
        pressedPoint_ = point;
        showTouchPoint(point.x, point.y);
        return;
    }

    if (code != LV_EVENT_RELEASED || !pressCaptured_) {
        return;
    }

    pressCaptured_ = false;
    const TargetPoint& expected = kTargets[currentTarget_];
    const int deltaX = abs(static_cast<int>(pressedPoint_.x - expected.x));
    const int deltaY = abs(static_cast<int>(pressedPoint_.y - expected.y));
    const bool passed = deltaX <= kToleranceX && deltaY <= kToleranceY;
    if (passed) {
        ++passedTargets_;
    }

    Serial.printf("[TOUCHTEST][RELEASE] target=%u dx=%d dy=%d result=%s\n",
                  static_cast<unsigned>(currentTarget_ + 1U),
                  deltaX,
                  deltaY,
                  passed ? "PASS" : "MISS");
    Serial.flush();

    ++currentTarget_;
    if (currentTarget_ >= kTargetCount) {
        finishTest();
    } else {
        showCurrentTarget();
    }
}

void TouchTestScreen::showTouchPoint(lv_coord_t x, lv_coord_t y) {
    lv_obj_clear_flag(crossHorizontal_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(crossVertical_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(crossHorizontal_, x - 22, y - 1);
    lv_obj_set_pos(crossVertical_, x - 1, y - 22);

    char text[96] = {};
    const TargetPoint& expected = kTargets[currentTarget_];
    snprintf(text,
             sizeof(text),
             "BEKLENEN: %d,%d    OKUNAN: %d,%d",
             static_cast<int>(expected.x),
             static_cast<int>(expected.y),
             static_cast<int>(x),
             static_cast<int>(y));
    lv_label_set_text(coordinateLabel_, text);
}

void TouchTestScreen::showCurrentTarget() {
    const TargetPoint& target = kTargets[currentTarget_];
    lv_obj_set_pos(target_, target.x - 46, target.y - 46);
    lv_obj_clear_flag(target_, LV_OBJ_FLAG_HIDDEN);

    char counter[64] = {};
    snprintf(counter,
             sizeof(counter),
             "HEDEF %u/%u - %s",
             static_cast<unsigned>(currentTarget_ + 1U),
             static_cast<unsigned>(kTargetCount),
             target.name);
    lv_label_set_text(counterLabel_, counter);

    char coordinate[80] = {};
    snprintf(coordinate,
             sizeof(coordinate),
             "BEKLENEN: %d,%d    OKUNAN: --,--",
             static_cast<int>(target.x),
             static_cast<int>(target.y));
    lv_label_set_text(coordinateLabel_, coordinate);
}

void TouchTestScreen::finishTest() {
    complete_ = true;
    lv_obj_add_flag(target_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crossHorizontal_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crossVertical_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(captureOverlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(captureOverlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(continueButton_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(continueButton_);

    char counter[64] = {};
    snprintf(counter,
             sizeof(counter),
             "SONUÇ: %u/%u HEDEF BAŞARILI",
             static_cast<unsigned>(passedTargets_),
             static_cast<unsigned>(kTargetCount));
    lv_label_set_text(counterLabel_, counter);

    const bool allPassed = passedTargets_ == kTargetCount;
    lv_label_set_text(resultLabel_,
                      allPassed
                          ? "DOKUNMATİK DOĞRULANDI: eksen, yön ve konum doğru."
                          : "KOORDİNAT EŞLEMESİ HATALI: seri kaydını gönderin.");
    lv_obj_set_style_text_color(resultLabel_,
                                allPassed ? lv_color_hex(0x69D991)
                                          : lv_color_hex(0xFF6B5D),
                                LV_PART_MAIN);
    lv_label_set_text(instructionLabel_,
                      "Sonucu kontrol edin, sonra tarama ekranına geçin");

    Serial.printf("[TOUCHTEST][RESULT] passed=%u/%u status=%s\n",
                  static_cast<unsigned>(passedTargets_),
                  static_cast<unsigned>(kTargetCount),
                  allPassed ? "PASS" : "CALIBRATION_REQUIRED");
    Serial.flush();
}

}  // namespace mg::p4
