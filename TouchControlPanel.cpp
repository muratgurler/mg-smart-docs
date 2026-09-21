#include "TouchControlPanel.h"

#include <Arduino.h>

#include "P4Button3D.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {

namespace {

lv_obj_t* addPanelLabel(lv_obj_t* parent,
                        const char* text,
                        const lv_font_t* font) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label,
                                lv_color_hex(0xFFFFFF),
                                LV_PART_MAIN);
    return label;
}

void setButtonState(lv_obj_t* button, bool active) {
    if (button == nullptr) {
        return;
    }
    setP4Button3DTone(button,
                      active ? P4ButtonTone::Active
                             : P4ButtonTone::Neutral);
}

}  // namespace

void TouchControlPanel::begin(lv_obj_t* parent) {
    if (parent == nullptr) {
        return;
    }

    mainMenuButton_ = createButton(parent,
                                   8,
                                   4,
                                   60,
                                   40,
                                   "MENU",
                                   mainMenuCallback,
                                   this,
                                   &mainMenuLabel_);

    const ControlAction quickActions[4] = {
        ControlAction::AutoStartPause,
        ControlAction::Step,
        ControlAction::StopOnError,
        ControlAction::ContinuousMode,
    };
    // SCAN/PAUSE is deliberately larger and remains immediately to the left
    // of the speaker in both Sub-D and normal scan modes. Its caption stays
    // fixed; green means the automatic scanner is currently running.
    // The row uses 4 px gaps from MENU through SCAN/PAUSE.  NEXT CABLE is
    // widened for its translated caption; profile-specific controls own the
    // remaining x=526..846 strip. SCAN/PAUSE stays beside the speaker.
    const lv_coord_t quickX[4] = {850, 72, 168, 312};
    const lv_coord_t quickWidth[4] = {118, 92, 140, 96};
    for (uint8_t index = 0; index < 4; ++index) {
        quickBindings_[index].owner = this;
        quickBindings_[index].action = quickActions[index];
        const bool isSingleStep = index == 1U;
        quickActionButtons_[index] = createButton(
            parent,
            quickX[index],
            4,
            quickWidth[index],
            40,
            "--",
            isSingleStep ? stepButtonCallback : actionCallback,
            &quickBindings_[index],
            &quickActionLabels_[index],
            (index == 0U || index == 2U || index == 3U)
                ? p4Font10()
                : p4Font14(),
            isSingleStep ? LV_EVENT_ALL : LV_EVENT_CLICKED);
        if (isSingleStep) {
            // Keep the press captured if the finger drifts a few pixels while
            // the operator is deliberately holding TEK ADIM.
            lv_obj_add_flag(quickActionButtons_[index], LV_OBJ_FLAG_PRESS_LOCK);
        }
    }

    nextCableBinding_.owner = this;
    nextCableBinding_.action = ControlAction::NextCable;
    nextCableButton_ = createButton(parent,
                                    412,
                                    4,
                                    110,
                                    40,
                                    "SONRAKİ KABLO",
                                    actionCallback,
                                    &nextCableBinding_,
                                    &nextCableLabel_,
                                    p4Font10());

    refresh();
    Serial.println("[SCAN CONTROLS] Inline scan controls ready");
    Serial.flush();
}

lv_obj_t* TouchControlPanel::createButton(lv_obj_t* parent,
                                          lv_coord_t x,
                                          lv_coord_t y,
                                          lv_coord_t width,
                                          lv_coord_t height,
                                          const char* text,
                                          lv_event_cb_t callback,
                                          void* userData,
                                          lv_obj_t** labelOut,
                                          const lv_font_t* font,
                                          lv_event_code_t eventCode) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    applyP4Button3D(button, P4ButtonTone::Neutral, 9);
    lv_obj_add_event_cb(button, callback, eventCode, userData);

    lv_obj_t* label = addPanelLabel(button,
                                    text,
                                    font != nullptr ? font : p4Font14());
    lv_obj_set_width(label, static_cast<lv_coord_t>(width - 10));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label);
    if (labelOut != nullptr) {
        *labelOut = label;
    }
    return button;
}

void TouchControlPanel::mainMenuCallback(lv_event_t* event) {
    auto* self = static_cast<TouchControlPanel*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->mainMenuRequested_ = true;
        Serial.println("[SCAN CONTROLS] Main menu requested");
        Serial.flush();
    }
}

void TouchControlPanel::actionCallback(lv_event_t* event) {
    auto* binding = static_cast<ButtonBinding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->owner != nullptr) {
        binding->owner->handleAction(binding->action);
    }
}

void TouchControlPanel::stepButtonCallback(lv_event_t* event) {
    auto* binding = static_cast<ButtonBinding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->owner == nullptr) {
        return;
    }

    TouchControlPanel* self = binding->owner;
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            self->beginStepPress();
            break;
        case LV_EVENT_PRESSING:
            self->updateStepHold();
            break;
        case LV_EVENT_RELEASED:
            self->endStepPress(true);
            break;
        case LV_EVENT_PRESS_LOST:
            self->endStepPress(false);
            break;
        default:
            break;
    }
}

void TouchControlPanel::beginStepPress() {
    if (stepPressActive_) {
        return;
    }

    stepPressActive_ = true;
    stepHoldRunning_ = false;
    stepPressedAtMs_ = millis();
    stepSavedSpeedPercent_ = session_.scanSpeedPercent();

    // If automatic scan was already running, grabbing TEK ADIM first freezes
    // the current point. A short release then advances exactly one point; a
    // long hold resumes from here at the temporary jog speed.
    session_.pause();
    refreshButtonStates();
}

void TouchControlPanel::updateStepHold() {
    if (!stepPressActive_ || stepHoldRunning_ ||
        static_cast<uint32_t>(millis() - stepPressedAtMs_) <
            kSingleStepHoldThresholdMs) {
        return;
    }

    stepHoldRunning_ = true;
    session_.setScanSpeedPercent(kSingleStepHoldSpeedPercent);
    session_.start();
    refresh();
    Serial.printf("[SCAN CONTROLS] SINGLE STEP hold -> jog %u%%\n",
                  static_cast<unsigned>(kSingleStepHoldSpeedPercent));
    Serial.flush();
}

void TouchControlPanel::endStepPress(bool releasedNormally) {
    if (!stepPressActive_) {
        return;
    }

    const bool wasHoldRunning = stepHoldRunning_;
    if (wasHoldRunning) {
        // Releasing a long-held TEK ADIM always means PAUSE. The scan-speed
        // slider is an operator preference, so the temporary jog speed must
        // not overwrite it.
        session_.pause();
        session_.setScanSpeedPercent(stepSavedSpeedPercent_);
    } else if (releasedNormally) {
        // A normal tap keeps the established one-point STEP behaviour.
        controller_.handle(ControlAction::Step);
    }

    stepPressActive_ = false;
    stepHoldRunning_ = false;
    stepPressedAtMs_ = 0;
    refresh();

    if (wasHoldRunning) {
        Serial.printf("[SCAN CONTROLS] SINGLE STEP released -> PAUSE, speed restored %u%%\n",
                      static_cast<unsigned>(stepSavedSpeedPercent_));
        Serial.flush();
    }
}

void TouchControlPanel::handleAction(ControlAction action) {
    controller_.handle(action);
    refresh();
    Serial.printf("[SCAN CONTROLS] action=%u\n",
                  static_cast<unsigned>(action));
    Serial.flush();
}

void TouchControlPanel::refreshButtonStates() {
    setButtonState(quickActionButtons_[0],
                   session_.runState() == RunState::Running);
    setButtonState(quickActionButtons_[1], stepHoldRunning_);
    setButtonState(quickActionButtons_[2], session_.stopOnError());
    setButtonState(quickActionButtons_[3], session_.continuousMode());
    const bool nextCableReady = workflow_.snapshot().profileValid &&
                                workflow_.snapshot().state == TestWorkflowState::Complete;
    setButtonState(nextCableButton_, nextCableReady);
    if (nextCableButton_ != nullptr) {
        if (nextCableReady) lv_obj_clear_state(nextCableButton_, LV_STATE_DISABLED);
        else lv_obj_add_state(nextCableButton_, LV_STATE_DISABLED);
    }
}

void TouchControlPanel::refresh() {
    const P4UiTexts& text = p4Texts();
    if (quickActionLabels_[0] != nullptr) {
        lv_label_set_text(quickActionLabels_[0], text.scanPause);
    }
    if (quickActionLabels_[1] != nullptr) {
        lv_label_set_text(quickActionLabels_[1], text.singleStep);
    }
    if (quickActionLabels_[2] != nullptr) {
        lv_label_set_text(quickActionLabels_[2], text.stopOnError);
    }
    if (quickActionLabels_[3] != nullptr) {
        lv_label_set_text(quickActionLabels_[3], text.nonStop);
    }
    if (mainMenuLabel_ != nullptr) {
        lv_label_set_text(mainMenuLabel_, text.menu);
    }
    if (nextCableLabel_ != nullptr) {
        lv_label_set_text(nextCableLabel_, text.nextCable);
    }
    refreshButtonStates();
}

bool TouchControlPanel::consumeMainMenuRequest() {
    const bool requested = mainMenuRequested_;
    mainMenuRequested_ = false;
    return requested;
}

}  // namespace mg::p4
