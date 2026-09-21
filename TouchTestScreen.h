#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "SoundToggleButton.h"

namespace mg::p4 {

class TouchTestScreen {
public:
    void begin();
    bool continueRequested() const { return continueRequested_; }
    bool complete() const { return complete_; }
    uint8_t passedTargets() const { return passedTargets_; }

private:
    struct TargetPoint {
        lv_coord_t x;
        lv_coord_t y;
        const char* name;
    };

    static void touchEventCallback(lv_event_t* event);
    static void continueEventCallback(lv_event_t* event);

    void handleTouchEvent(lv_event_code_t code);
    void showTouchPoint(lv_coord_t x, lv_coord_t y);
    void showCurrentTarget();
    void finishTest();

    lv_obj_t* target_ = nullptr;
    lv_obj_t* targetPlus_ = nullptr;
    lv_obj_t* instructionLabel_ = nullptr;
    lv_obj_t* counterLabel_ = nullptr;
    lv_obj_t* coordinateLabel_ = nullptr;
    lv_obj_t* resultLabel_ = nullptr;
    lv_obj_t* crossHorizontal_ = nullptr;
    lv_obj_t* crossVertical_ = nullptr;
    lv_obj_t* captureOverlay_ = nullptr;
    lv_obj_t* continueButton_ = nullptr;

    lv_point_t pressedPoint_{0, 0};
    uint8_t currentTarget_ = 0;
    uint8_t passedTargets_ = 0;
    bool pressCaptured_ = false;
    bool complete_ = false;
    bool continueRequested_ = false;

    static constexpr uint8_t kTargetCount = 5;
    static constexpr lv_coord_t kToleranceX = 70;
    static constexpr lv_coord_t kToleranceY = 70;
    static const TargetPoint kTargets[kTargetCount];
    lv_obj_t* screen_ = nullptr;
    SoundToggleButton soundButton_;
};

}  // namespace mg::p4
