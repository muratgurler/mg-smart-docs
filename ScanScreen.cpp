#include "ScanScreen.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "P4Button3D.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {

namespace {

void uiCheckpoint(const char* message) {
    Serial.printf("%s | internal=%u, psram=%u\n",
                  message,
                  static_cast<unsigned>(
                      heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                              MALLOC_CAP_8BIT)),
                  static_cast<unsigned>(
                      heap_caps_get_free_size(MALLOC_CAP_SPIRAM |
                                              MALLOC_CAP_8BIT)));
    Serial.flush();
}

lv_obj_t* addLabel(lv_obj_t* parent,
                   const char* text,
                   const lv_font_t* font,
                   lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

void setScanButtonState(lv_obj_t* button, bool active) {
    if (button == nullptr) {
        return;
    }
    setP4Button3DTone(button,
                      active ? P4ButtonTone::Active
                             : P4ButtonTone::Neutral);
}


const char* editMapButtonText() {
    switch (currentP4Language()) {
        case P4Language::English: return "EDIT MAP";
        case P4Language::Dutch: return "BEWERK";
        case P4Language::German: return "BELEGUNG";
        case P4Language::French: return "MODIFIER";
        case P4Language::Spanish: return "EDITAR";
        case P4Language::Polish: return "EDYTUJ";
        case P4Language::Turkish:
        default: return "DÜZENLE";
    }
}

}  // namespace

void ScanScreen::begin() {
    uiCheckpoint("[UI][01] screen build begin");
    if (screen_ == nullptr) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    lv_scr_load(screen_);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x0B1118), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    createTopArea(screen_);
    uiCheckpoint("[UI][02] top area ready");
    createBottomArea(screen_);
    uiCheckpoint("[UI][03] bottom area ready");
    onProfileChanged();
    uiCheckpoint("[UI][08] profile objects ready");
    refresh();
    uiCheckpoint("[UI][09] initial values ready");
}

void ScanScreen::activate() {
    if (screen_ != nullptr && lv_scr_act() != screen_) {
        lv_scr_load(screen_);
    }
    soundButton_.refresh();
    refreshLegend();
}

void ScanScreen::createTopArea(lv_obj_t* screen) {
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 50);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142330), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);

    auto createHeaderButton = [&](lv_coord_t x,
                                  lv_coord_t width,
                                  const char* text,
                                  ProfileCommand command,
                                  uint8_t bindingIndex,
                                  const lv_font_t* font) {
        lv_obj_t* button = lv_btn_create(header);
        lv_obj_set_pos(button, x, 4);
        lv_obj_set_size(button, width, 40);
        applyP4Button3D(button, P4ButtonTone::Neutral, 8);
        profileBindings_[bindingIndex] = {this, command};
        lv_obj_add_event_cb(button,
                            profileButtonCallback,
                            LV_EVENT_CLICKED,
                            &profileBindings_[bindingIndex]);
        lv_obj_t* label = addLabel(button, text, font, lv_color_hex(0xFFFFFF));
        lv_obj_center(label);
        return button;
    };

    // Profile-specific controls own the complete free header strip between
    // NEXT CABLE (ends at x=522) and SCAN/PAUSE (starts at x=850).  Keep a
    // 4 px gap at both ends and between buttons so removing PE/dS from the
    // header never leaves a dead-looking hole again.
    profileButton_ = createHeaderButton(526,
                                        320,
                                        "Sub-D25",
                                        ProfileCommand::NextSubD,
                                        0,
                                        p4Font14());
    profileLabel_ = lv_obj_get_child(profileButton_, 0);

    normalMinusButton_ = createHeaderButton(650,
                                            54,
                                            "-",
                                            ProfileCommand::DecreasePins,
                                            1,
                                            p4Font20());
    normalCountButton_ = createHeaderButton(708,
                                             80,
                                             "25 PIN",
                                             ProfileCommand::NextPinPreset,
                                             5,
                                             p4Font14());
    normalCountLabel_ = lv_obj_get_child(normalCountButton_, 0);
    normalPlusButton_ = createHeaderButton(792,
                                           54,
                                           "+",
                                           ProfileCommand::IncreasePins,
                                           2,
                                           p4Font20());
    // The custom-map editor now belongs to NORMAL CABLE TEST.  It receives
    // extra width because its translated captions are longer than +/- and the
    // freed PE/dS header space should be used rather than left blank.
    editMapButton_ = createHeaderButton(526,
                                        120,
                                        editMapButtonText(),
                                        ProfileCommand::EditMap,
                                        6,
                                        p4Font10());
    editMapLabel_ = lv_obj_get_child(editMapButton_, 0);

    soundButton_.create(header, 974, 4, 42);

    stateLabel_ = addLabel(screen,
                           "HAZIR",
                           p4Font14(),
                           lv_color_hex(0x69D991));
    lv_obj_set_width(stateLabel_, 180);
    lv_obj_set_style_text_align(stateLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(stateLabel_, 422, 55);

    connectorA_.create(screen, 10, 58);
    connectorB_.create(screen, 826, 58);

    // PE and drain/shield are side-specific profile properties. Put their
    // controls directly below the corresponding connector instead of using
    // scarce header space. This also supports intentionally single-ended
    // PE/shield terminations.
    auto createSideSpecialButton = [&](lv_coord_t x,
                                       const char* text,
                                       ProfileCommand command,
                                       uint8_t bindingIndex) {
        lv_obj_t* button = lv_btn_create(screen);
        lv_obj_set_pos(button, x, 255);
        lv_obj_set_size(button, 90, 28);
        applyP4Button3D(button, P4ButtonTone::Neutral, 6);
        profileBindings_[bindingIndex] = {this, command};
        lv_obj_add_event_cb(button,
                            profileButtonCallback,
                            LV_EVENT_CLICKED,
                            &profileBindings_[bindingIndex]);
        lv_obj_t* label = addLabel(button, text, p4Font10(), lv_color_hex(0xFFFFFF));
        lv_obj_center(label);
        return button;
    };

    peToggleButtonA_ = createSideSpecialButton(10, "PE ON", ProfileCommand::TogglePeA, 3);
    peToggleLabelA_ = lv_obj_get_child(peToggleButtonA_, 0);
    dsToggleButtonA_ = createSideSpecialButton(108, "dS ON", ProfileCommand::ToggleDrainShieldA, 4);
    dsToggleLabelA_ = lv_obj_get_child(dsToggleButtonA_, 0);
    peToggleButtonB_ = createSideSpecialButton(826, "PE ON", ProfileCommand::TogglePeB, 7);
    peToggleLabelB_ = lv_obj_get_child(peToggleButtonB_, 0);
    dsToggleButtonB_ = createSideSpecialButton(924, "dS ON", ProfileCommand::ToggleDrainShieldB, 8);
    dsToggleLabelB_ = lv_obj_get_child(dsToggleButtonB_, 0);

    lv_obj_t* fixedA = addLabel(screen,
                                "A",
                                p4Font20(),
                                lv_color_hex(0x55C8FF));
    lv_obj_set_pos(fixedA, 274, 86);
    lv_obj_t* fixedB = addLabel(screen,
                                "B",
                                p4Font20(),
                                lv_color_hex(0x55C8FF));
    lv_obj_set_pos(fixedB, 730, 86);

    pinAValue_ = addLabel(screen,
                          "PE",
                          p4PinFont84(),
                          lv_color_hex(0xFFFFFF));
    lv_obj_set_pos(pinAValue_, 224, 119);
    lv_obj_set_width(pinAValue_, 166);
    lv_obj_set_style_text_align(pinAValue_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(pinAValue_, -2, LV_PART_MAIN);

    pinBValue_ = addLabel(screen,
                          "PE",
                          p4PinFont84(),
                          lv_color_hex(0xFFFFFF));
    lv_obj_set_pos(pinBValue_, 634, 119);
    lv_obj_set_width(pinBValue_, 166);
    lv_obj_set_style_text_align(pinBValue_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(pinBValue_, -2, LV_PART_MAIN);

    directionArrow_.create(screen,
                           394,
                           113,
                           directionArrowCallback,
                           this);

    phaseLabel_ = addLabel(screen,
                           "A -> B TARAMASI",
                           p4Font14(),
                           lv_color_hex(0xB8C9D5));
    lv_obj_set_width(phaseLabel_, 460);
    lv_obj_set_style_text_align(phaseLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(phaseLabel_, 282, 207);

    // Live electrical path summary. It is deliberately separate from the
    // large A/B pin glyphs so 1:N/N:1/common-net and SHORT groups can show
    // every active pin without shrinking the approved 84 px pin display.
    liveStatusLabel_ = addLabel(screen,
                                "A -- -> B --",
                                p4Font14(),
                                lv_color_hex(0x35434E));
    lv_obj_set_width(liveStatusLabel_, 620);
    lv_obj_set_style_text_align(liveStatusLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(liveStatusLabel_, 202, 229);

    speedLabel_ = addLabel(screen,
                           "TEST HIZI: 80%",
                           p4Font14(),
                           lv_color_hex(0xD6E4EC));
    // A/B PE+dS controls occupy the outer zones directly below the two
    // connector drawings. Keep scan-speed control in the free centre zone.
    lv_obj_set_width(speedLabel_, 180);
    lv_obj_set_style_text_align(speedLabel_, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_pos(speedLabel_, 230, 253);

    speedSlider_ = lv_slider_create(screen);
    lv_obj_set_pos(speedSlider_, 420, 256);
    lv_obj_set_size(speedSlider_, 390, 24);
    lv_slider_set_range(speedSlider_,
                        kMinimumTestSpeedPercent,
                        kMaximumTestSpeedPercent);
    lv_slider_set_value(speedSlider_, session_.scanSpeedPercent(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(speedSlider_,
                              lv_color_hex(0x263746),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(speedSlider_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(speedSlider_, 9, LV_PART_MAIN);
    lv_obj_set_style_bg_color(speedSlider_,
                              lv_color_hex(0x2DA9E8),
                              LV_PART_INDICATOR);
    lv_obj_set_style_radius(speedSlider_, 9, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(speedSlider_,
                              lv_color_hex(0xFFD34D),
                              LV_PART_KNOB);
    lv_obj_set_style_border_color(speedSlider_,
                                  lv_color_hex(0xFFFFFF),
                                  LV_PART_KNOB);
    lv_obj_set_style_border_width(speedSlider_, 2, LV_PART_KNOB);
    lv_obj_set_style_pad_all(speedSlider_, 5, LV_PART_KNOB);
    lv_obj_add_event_cb(speedSlider_,
                        speedSliderCallback,
                        LV_EVENT_VALUE_CHANGED,
                        this);
    refreshSpeedControl();
}

void ScanScreen::directionArrowCallback(void* userData) {
    auto* self = static_cast<ScanScreen*>(userData);
    if (self != nullptr && self->directionHandler_ != nullptr) {
        self->directionHandler_(self->directionUserData_);
    }
}

void ScanScreen::profileButtonCallback(lv_event_t* event) {
    auto* binding = static_cast<ProfileBinding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->owner != nullptr &&
        binding->owner->profileHandler_ != nullptr) {
        binding->owner->profileHandler_(binding->command,
                                        binding->owner->profileUserData_);
    }
}

void ScanScreen::speedSliderCallback(lv_event_t* event) {
    auto* self = static_cast<ScanScreen*>(lv_event_get_user_data(event));
    if (self == nullptr || self->speedSlider_ == nullptr) {
        return;
    }
    self->session_.setScanSpeedPercent(
        static_cast<uint8_t>(lv_slider_get_value(self->speedSlider_)));
    self->refreshSpeedControl();
}

void ScanScreen::refreshSpeedControl() {
    if (speedLabel_ == nullptr || speedSlider_ == nullptr) {
        return;
    }
    char label[48] = {};
    snprintf(label,
             sizeof(label),
             "%s: %u%%",
             p4Texts().testSpeed,
             session_.scanSpeedPercent());
    lv_label_set_text(speedLabel_, label);
    lv_slider_set_value(speedSlider_,
                        session_.scanSpeedPercent(),
                        LV_ANIM_OFF);
}

void ScanScreen::refreshLegend() {
    const P4UiTexts& text = p4Texts();
    const char* const labels[] = {
        text.legendWaiting,
        text.legendScanning,
        text.legendOk,
        text.legendOpen,
        text.legendShort,
        text.legendWrong,
        text.legendResistance,
    };
    static_assert(sizeof(labels) / sizeof(labels[0]) == 7U,
                  "Legend text and label counts must match");
    for (uint8_t index = 0; index < 7U; ++index) {
        if (legendLabels_[index] != nullptr) {
            lv_label_set_text_fmt(legendLabels_[index], ": %s", labels[index]);
        }
    }
}

void ScanScreen::createBottomArea(lv_obj_t* screen) {
    lv_obj_t* separator = lv_obj_create(screen);
    lv_obj_set_pos(separator, 0, kBottomAreaY);
    lv_obj_set_size(separator, kDisplayWidth, 2);
    lv_obj_set_style_bg_color(separator, lv_color_hex(0x315064), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(separator, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(separator, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(separator, 0, LV_PART_MAIN);

    progressLabel_ = addLabel(screen,
                              "0/54  0%",
                              p4Font14(),
                              lv_color_hex(0xC5D2DB));
    lv_obj_set_pos(progressLabel_, 14, 296);

    progressBar_ = lv_bar_create(screen);
    lv_obj_set_pos(progressBar_, 155, 300);
    lv_obj_set_size(progressBar_, 855, 14);
    lv_bar_set_range(progressBar_, 0, 100);
    lv_obj_set_style_bg_color(progressBar_,
                              lv_color_hex(0x263746),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar_,
                              lv_color_hex(0x2BC875),
                              LV_PART_INDICATOR);

    lv_obj_t* labelA = addLabel(screen,
                                "A",
                                p4Font20(),
                                lv_color_hex(0x55C8FF));
    lv_obj_set_pos(labelA, 15, 366);
    barSideBLabel_ = addLabel(screen,
                              "B",
                              p4Font20(),
                              lv_color_hex(0x55C8FF));
    lv_obj_set_pos(barSideBLabel_,
                   15,
                   session_.profile().activePointCount() <= 27U ? 489 : 463);

    barsCanvas_ = lv_canvas_create(screen);
    lv_obj_set_pos(barsCanvas_, kBarAreaLeft, kBarsCanvasY);
    const size_t canvasBytes =
        static_cast<size_t>(kBarsCanvasWidth) * kBarsCanvasHeight *
        sizeof(lv_color_t);
    barsCanvasBuffer_ = static_cast<lv_color_t*>(
        heap_caps_calloc(1,
                         canvasBytes,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (barsCanvasBuffer_ != nullptr) {
        lv_canvas_set_buffer(barsCanvas_,
                             barsCanvasBuffer_,
                             kBarsCanvasWidth,
                             kBarsCanvasHeight,
                             LV_IMG_CF_TRUE_COLOR);
    } else {
        Serial.printf("[UI][ERR] LED canvas allocation failed: %u bytes\n",
                      static_cast<unsigned>(canvasBytes));
        Serial.flush();
        lv_obj_del(barsCanvas_);
        barsCanvas_ = nullptr;
    }

    legendRow_ = lv_obj_create(screen);
    lv_obj_set_pos(legendRow_, 6, 566);
    lv_obj_set_size(legendRow_, 1012, 34);
    lv_obj_clear_flag(legendRow_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(legendRow_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(legendRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(legendRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(legendRow_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(legendRow_, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(legendRow_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legendRow_,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    constexpr PointVisualStatus legendStatuses[] = {
        PointVisualStatus::Untested,
        PointVisualStatus::Scanning,
        PointVisualStatus::Ok,
        PointVisualStatus::Open,
        PointVisualStatus::ShortCircuit,
        PointVisualStatus::WrongConnection,
        PointVisualStatus::HighResistance,
    };
    static_assert(sizeof(legendStatuses) / sizeof(legendStatuses[0]) == 7U,
                  "Legend status and label counts must match");

    for (uint8_t index = 0; index < 7U; ++index) {
        lv_obj_t* item = lv_obj_create(legendRow_);
        lv_obj_set_size(item, LV_SIZE_CONTENT, 28);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_column(item, 4, LV_PART_MAIN);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        lv_obj_t* colorSquare = lv_obj_create(item);
        lv_obj_set_size(colorSquare, 22, 22);
        lv_obj_clear_flag(colorSquare, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(colorSquare,
                                  visualColor(legendStatuses[index]),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(colorSquare, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(colorSquare,
                                      lv_color_hex(0xC6D1D8),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_width(colorSquare, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(colorSquare, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_all(colorSquare, 0, LV_PART_MAIN);

        legendLabels_[index] = addLabel(item,
                                        ": --",
                                        p4Font14(),
                                        lv_color_hex(0xEDF4F8));
    }
    refreshLegend();
}

void ScanScreen::onProfileChanged() {
    connectorA_.setConnector(session_.profile().sideA,
                             session_.profile().includePeA,
                             session_.profile().includeDrainShieldA);
    uiCheckpoint("[UI][04] connector A ready");
    connectorB_.setConnector(session_.profile().sideB,
                             session_.profile().includePeB,
                             session_.profile().includeDrainShieldB);
    uiCheckpoint("[UI][05] connector B ready");
    rebuildBars();
    uiCheckpoint("[UI][06] LED bars ready");

    refreshProfileControls();
    refreshLegend();
    uiCheckpoint("[UI][07] profile label ready");
    firstRefresh_ = true;
}

void ScanScreen::refreshProfileControls() {
    const CableProfile& profile = session_.profile();
    const bool subD = profile.isSubD();

    if (subD) {
        lv_obj_clear_flag(profileButton_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(profileLabel_, profile.name);
        setScanButtonState(profileButton_,
                           strcmp(profile.name, "Sub-D25") != 0);
    } else {
        lv_obj_add_flag(profileButton_, LV_OBJ_FLAG_HIDDEN);
    }

    // Normal-only header controls disappear in Sub-D mode, but the dS switch
    // remains available on both connector sides. Sub-D shells are the drain/
    // shield path; PE stays hidden because it is not a Sub-D contact.
    lv_obj_t* const normalOnlyObjects[] = {
        normalMinusButton_, normalCountButton_, normalPlusButton_, editMapButton_,
        peToggleButtonA_, peToggleButtonB_};
    for (lv_obj_t* object : normalOnlyObjects) {
        if (subD) {
            lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // dS is meaningful in BOTH Normal and Sub-D modes. In Sub-D there is only
    // one special control per side, so centre it below the connector face.
    lv_obj_clear_flag(dsToggleButtonA_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(dsToggleButtonB_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(dsToggleButtonA_, subD ? 59 : 108, 255);
    lv_obj_set_pos(dsToggleButtonB_, subD ? 875 : 924, 255);

    lv_label_set_text(dsToggleLabelA_,
                      profile.includeDrainShieldA ? "dS ON" : "dS OFF");
    lv_label_set_text(dsToggleLabelB_,
                      profile.includeDrainShieldB ? "dS ON" : "dS OFF");
    setScanButtonState(dsToggleButtonA_, profile.includeDrainShieldA);
    setScanButtonState(dsToggleButtonB_, profile.includeDrainShieldB);

    if (subD) {
        return;
    }

    char pinCount[20] = {};
    snprintf(pinCount, sizeof(pinCount), "%u PIN", profile.signalPinCount);
    lv_label_set_text(normalCountLabel_, pinCount);
    lv_label_set_text(peToggleLabelA_, profile.includePeA ? "PE ON" : "PE OFF");
    lv_label_set_text(peToggleLabelB_, profile.includePeB ? "PE ON" : "PE OFF");
    if (editMapLabel_ != nullptr) {
        lv_label_set_text(editMapLabel_, editMapButtonText());
    }
    // Side controls are direct state switches: active/green means that the
    // corresponding PE or dS point participates on that connector side.
    setScanButtonState(peToggleButtonA_, profile.includePeA);
    setScanButtonState(peToggleButtonB_, profile.includePeB);
    // Active tone means that the normal scan currently evaluates against the
    // saved special map; neutral tone means ordinary 1:1 expectation.
    setScanButtonState(editMapButton_, session_.usesCustomMap());
}

bool ScanScreen::update(bool force) {
    const uint32_t nowMs = millis();
    if (!force && !firstRefresh_ &&
        session_.runState() == RunState::Running &&
        lastUiRefreshMs_ != 0U &&
        nowMs - lastUiRefreshMs_ < kUiFrameIntervalMs) {
        // Do not consume the dirty edge. The scan engine may advance again;
        // the next visual frame will present its newest coherent state rather
        // than replaying obsolete positions in a burst.
        return false;
    }
    if (session_.consumeDirty()) {
        refresh();
        lastUiRefreshMs_ = nowMs;
        return true;
    }
    return false;
}

void ScanScreen::clearBars() {
    if (barsCanvasBuffer_ != nullptr) {
        fillBarsRect(0,
                     0,
                     kBarsCanvasWidth,
                     kBarsCanvasHeight,
                     lv_color_hex(0x0B1118));
        lv_obj_invalidate(barsCanvas_);
    }
    renderedPointCount_ = 0;
}

void ScanScreen::rebuildBars() {
    clearBars();
    renderedPointCount_ = session_.profile().activePointCount();
    if (renderedPointCount_ == 0) {
        return;
    }

    if (barsCanvasBuffer_ == nullptr || barsCanvas_ == nullptr) {
        return;
    }

    barPitch_ = kBarAreaWidth / renderedPointCount_;
    const lv_coord_t gap = 1;
    barWidth_ = barPitch_ > gap + 3 ? barPitch_ - gap : 3;
    barHeight_ = renderedPointCount_ <= 27U ? 96 : 76;
    barYA_ = static_cast<lv_coord_t>(
        (renderedPointCount_ <= 27U ? 328 : 338) - kBarsCanvasY);
    barYB_ = static_cast<lv_coord_t>(
        (renderedPointCount_ <= 27U ? 451 : 435) - kBarsCanvasY);
    if (barSideBLabel_ != nullptr) {
        lv_obj_set_y(barSideBLabel_, renderedPointCount_ <= 27U ? 489 : 463);
    }
    const lv_font_t* numberFont = renderedPointCount_ <= 27U
                                      ? p4Font20()
                                      : p4Font10();

    for (uint8_t slot = 0; slot < renderedPointCount_; ++slot) {
        drawBar(slot, true, PointVisualStatus::Untested);
        drawBar(slot, false, PointVisualStatus::Untested);

        char label[4] = {};
        session_.profile().formatSlotLabel(slot, label, sizeof(label));
        drawNumber(slot, label, numberFont);

        cachedA_[slot] = PointVisualStatus::Untested;
        cachedB_[slot] = PointVisualStatus::Untested;
    }
    lv_obj_invalidate(barsCanvas_);
}

void ScanScreen::drawBar(uint8_t slot,
                         bool sideA,
                         PointVisualStatus status) {
    if (barsCanvasBuffer_ == nullptr || slot >= renderedPointCount_) {
        return;
    }
    const lv_coord_t cellLeft = static_cast<lv_coord_t>(slot * barPitch_);
    const lv_coord_t x = static_cast<lv_coord_t>(
        cellLeft + (barPitch_ - barWidth_) / 2);
    const lv_coord_t y = sideA ? barYA_ : barYB_;
    const uint8_t testIndex = session_.profile().slotToTestIndex(slot);
    const bool enabled = session_.profile().pointEnabledOnSide(sideA, testIndex);
    fillBarsRect(x,
                 y,
                 barWidth_,
                 barHeight_,
                 enabled ? lv_color_hex(0x647481) : lv_color_hex(0x25313A));
    if (barWidth_ > 2 && barHeight_ > 2) {
        fillBarsRect(static_cast<lv_coord_t>(x + 1),
                     static_cast<lv_coord_t>(y + 1),
                     static_cast<lv_coord_t>(barWidth_ - 2),
                     static_cast<lv_coord_t>(barHeight_ - 2),
                     enabled ? visualColor(status) : lv_color_hex(0x0B1118));
    }
}

void ScanScreen::drawNumber(uint8_t slot,
                            const char* text,
                            const lv_font_t* font) {
    if (barsCanvas_ == nullptr || text == nullptr || font == nullptr) {
        return;
    }
    lv_draw_label_dsc_t labelDescriptor;
    lv_draw_label_dsc_init(&labelDescriptor);
    labelDescriptor.font = font;
    labelDescriptor.color = lv_color_hex(0xE0E8ED);
    labelDescriptor.align = LV_TEXT_ALIGN_CENTER;
    const lv_coord_t cellLeft = static_cast<lv_coord_t>(slot * barPitch_);
    const lv_coord_t numberY = renderedPointCount_ <= 27U ? 426 : 416;
    lv_canvas_draw_text(barsCanvas_,
                        static_cast<lv_coord_t>(cellLeft - 2),
                        static_cast<lv_coord_t>(numberY - kBarsCanvasY),
                        static_cast<lv_coord_t>(barPitch_ + 4),
                        &labelDescriptor,
                        text);
}

void ScanScreen::fillBarsRect(lv_coord_t x,
                              lv_coord_t y,
                              lv_coord_t width,
                              lv_coord_t height,
                              lv_color_t color) {
    for (lv_coord_t row = 0; row < height; ++row) {
        for (lv_coord_t column = 0; column < width; ++column) {
            setBarsPixel(static_cast<lv_coord_t>(x + column),
                         static_cast<lv_coord_t>(y + row),
                         color);
        }
    }
}

void ScanScreen::setBarsPixel(lv_coord_t x,
                              lv_coord_t y,
                              lv_color_t color) {
    if (barsCanvasBuffer_ == nullptr || x < 0 || y < 0 ||
        x >= kBarsCanvasWidth || y >= kBarsCanvasHeight) {
        return;
    }
    barsCanvasBuffer_[static_cast<size_t>(y) * kBarsCanvasWidth + x] = color;
}

void ScanScreen::refresh() {
    const P4UiTexts& text = p4Texts();
    char pinAText[4] = {};
    char pinBText[4] = {};
    formatTestIndex(session_.displayTestIndexA(), pinAText, sizeof(pinAText));
    formatTestIndex(session_.displayTestIndexB(), pinBText, sizeof(pinBText));
    lv_label_set_text(pinAValue_, pinAText);
    lv_label_set_text(pinBValue_, pinBText);

    const bool aToB = session_.displayDirection() == ScanDirection::AtoB;
    lv_label_set_text(phaseLabel_, aToB ? text.scanAtoB : text.scanBtoA);
    const PointVisualStatus liveStatus = liveVisualStatus();
    const lv_color_t liveColor = liveStatus == PointVisualStatus::Untested
                                     ? lv_color_hex(0xFFFFFF)
                                     : visualColor(liveStatus);
    lv_obj_set_style_text_color(pinAValue_, liveColor, LV_PART_MAIN);
    lv_obj_set_style_text_color(pinBValue_, liveColor, LV_PART_MAIN);
    lv_obj_set_style_text_color(phaseLabel_,
                                liveStatus == PointVisualStatus::Untested
                                    ? lv_color_hex(0xB8C9D5)
                                    : liveColor,
                                LV_PART_MAIN);

    const char* stateText = text.ready;
    if (session_.runState() == RunState::Running) stateText = text.scanning;
    else if (session_.runState() == RunState::Paused) stateText = text.paused;
    else if (session_.runState() == RunState::Complete) stateText = text.complete;
    lv_label_set_text(stateLabel_, stateText);
    lv_obj_set_style_text_color(
        stateLabel_,
        session_.runState() == RunState::Paused ? lv_color_hex(0xFFD34D)
                                                : lv_color_hex(0x69D991),
        LV_PART_MAIN);

    lv_color_t arrowColor = lv_color_hex(0x677681);
    if (liveStatus != PointVisualStatus::Untested) {
        arrowColor = visualColor(liveStatus);
    } else if (session_.runState() == RunState::Paused) {
        arrowColor = lv_color_hex(0xFFD34D);
    } else if (session_.runState() == RunState::Complete) {
        arrowColor = lv_color_hex(0x35D273);
    }
    directionArrow_.setDirection(session_.displayDirection(), arrowColor);

    // Full live path: the large pin fields keep the first endpoint while this
    // line exposes all members of a common/short group.  Its text and arrow
    // use the exact same electrical status color as the bars/report table.
    char aGroup[128] = {};
    char bGroup[128] = {};
    if (aToB) {
        formatLiveMask(session_.displaySenderMask(), 'A', aGroup, sizeof(aGroup));
        formatLiveMask(session_.displayReceiverMask(), 'B', bGroup, sizeof(bGroup));
    } else {
        formatLiveMask(session_.displayReceiverMask(), 'A', aGroup, sizeof(aGroup));
        formatLiveMask(session_.displaySenderMask(), 'B', bGroup, sizeof(bGroup));
    }
    if (session_.displayResult() == ElectricalResult::Open &&
        session_.displayReceiverMask() == 0ULL) {
        snprintf(aToB ? bGroup : aGroup,
                 aToB ? sizeof(bGroup) : sizeof(aGroup),
                 "%c %s",
                 aToB ? 'B' : 'A',
                 text.legendOpen);
    }

    const char* liveStateText = session_.displayResult() == ElectricalResult::NotMeasured
                                    ? stateText
                                    : localizedResultText(session_.displayResult());
    char liveLine[360] = {};
    if (session_.displayResistanceValid()) {
        snprintf(liveLine, sizeof(liveLine),
                 "%s  %s  %s  |  %s  |  R %.2f mOhm",
                 aGroup, aToB ? "->" : "<-", bGroup, liveStateText,
                 static_cast<double>(session_.displayResistanceMilliOhm()));
    } else {
        snprintf(liveLine, sizeof(liveLine),
                 "%s  %s  %s  |  %s",
                 aGroup, aToB ? "->" : "<-", bGroup, liveStateText);
    }
    if (liveStatusLabel_ != nullptr) {
        lv_label_set_text(liveStatusLabel_, liveLine);
        lv_obj_set_style_text_color(liveStatusLabel_,
                                    liveStatus == PointVisualStatus::Untested
                                        ? lv_color_hex(0xB8C9D5)
                                        : liveColor,
                                    LV_PART_MAIN);
    }

    connectorA_.setActiveTestIndex(session_.displayTestIndexA(),
                                   session_.activeStatusA(),
                                   aToB);
    connectorB_.setActiveTestIndex(session_.displayTestIndexB(),
                                   session_.activeStatusB(),
                                   !aToB);

    char progress[48] = {};
    snprintf(progress,
             sizeof(progress),
             "%u/%u   %u%%",
             session_.completedMeasurements(),
             session_.totalMeasurements(),
             session_.progressPercent());
    lv_label_set_text(progressLabel_, progress);
    lv_bar_set_value(progressBar_, session_.progressPercent(), LV_ANIM_OFF);
    refreshSpeedControl();
    refreshBars();
    firstRefresh_ = false;
}

void ScanScreen::refreshBars() {
    uint8_t changedAreaCount = 0;
    for (uint8_t slot = 0; slot < renderedPointCount_; ++slot) {
        if (firstRefresh_ || session_.statusA(slot) != cachedA_[slot]) {
            ++changedAreaCount;
        }
        if (firstRefresh_ || session_.statusB(slot) != cachedB_[slot]) {
            ++changedAreaCount;
        }
    }

    const bool invalidateWholeCanvas = firstRefresh_ || changedAreaCount > 8U;
    for (uint8_t slot = 0; slot < renderedPointCount_; ++slot) {
        const PointVisualStatus statusA = session_.statusA(slot);
        const PointVisualStatus statusB = session_.statusB(slot);
        if (firstRefresh_ || statusA != cachedA_[slot]) {
            drawBar(slot, true, statusA);
            cachedA_[slot] = statusA;
            if (!invalidateWholeCanvas) {
                invalidateBar(slot, true);
            }
        }
        if (firstRefresh_ || statusB != cachedB_[slot]) {
            drawBar(slot, false, statusB);
            cachedB_[slot] = statusB;
            if (!invalidateWholeCanvas) {
                invalidateBar(slot, false);
            }
        }
    }
    if (changedAreaCount != 0U && invalidateWholeCanvas &&
        barsCanvas_ != nullptr) {
        lv_obj_invalidate(barsCanvas_);
    }
}

void ScanScreen::invalidateBar(uint8_t slot, bool sideA) {
    if (barsCanvas_ == nullptr || slot >= renderedPointCount_) {
        return;
    }
    const lv_coord_t cellLeft = static_cast<lv_coord_t>(slot * barPitch_);
    const lv_coord_t x = static_cast<lv_coord_t>(
        cellLeft + (barPitch_ - barWidth_) / 2);
    const lv_coord_t y = sideA ? barYA_ : barYB_;
    lv_area_t area;
    area.x1 = x;
    area.y1 = y;
    area.x2 = static_cast<lv_coord_t>(x + barWidth_ - 1);
    area.y2 = static_cast<lv_coord_t>(y + barHeight_ - 1);
    lv_obj_invalidate_area(barsCanvas_, &area);
}

void ScanScreen::formatLiveMask(uint64_t mask,
                                char side,
                                char* output,
                                size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) {
        return;
    }
    output[0] = '\0';
    for (uint8_t index = 0U; index < kTestPointsPerSide; ++index) {
        if ((mask & (1ULL << index)) == 0ULL) {
            continue;
        }
        char pin[8] = {};
        if (index == kPeTestIndex) {
            snprintf(pin, sizeof(pin), "PE");
        } else if (index == kDrainShieldTestIndex) {
            snprintf(pin, sizeof(pin), "dS");
        } else {
            snprintf(pin, sizeof(pin), "%u", static_cast<unsigned>(index));
        }
        const size_t used = strlen(output);
        if (used + 1U >= outputSize) {
            break;
        }
        snprintf(output + used,
                 outputSize - used,
                 "%s%c%s",
                 used == 0U ? "" : "+",
                 side,
                 pin);
    }
    if (output[0] == '\0') {
        snprintf(output, outputSize, "%c--", side);
    }
}

const char* ScanScreen::localizedResultText(ElectricalResult result) const {
    const P4UiTexts& text = p4Texts();
    switch (result) {
        case ElectricalResult::Ok:              return text.legendOk;
        case ElectricalResult::Open:            return text.legendOpen;
        case ElectricalResult::ShortCircuit:    return text.legendShort;
        case ElectricalResult::WrongConnection: return text.legendWrong;
        case ElectricalResult::HighResistance:  return text.legendResistance;
        case ElectricalResult::NotMeasured:
        default:                                return text.legendWaiting;
    }
}

PointVisualStatus ScanScreen::liveVisualStatus() const {
    switch (session_.displayResult()) {
        case ElectricalResult::Ok:              return PointVisualStatus::Ok;
        case ElectricalResult::Open:            return PointVisualStatus::Open;
        case ElectricalResult::ShortCircuit:    return PointVisualStatus::ShortCircuit;
        case ElectricalResult::WrongConnection: return PointVisualStatus::WrongConnection;
        case ElectricalResult::HighResistance:  return PointVisualStatus::HighResistance;
        case ElectricalResult::NotMeasured:
        default:
            if (session_.activeStatusA() == PointVisualStatus::Scanning ||
                session_.activeStatusB() == PointVisualStatus::Scanning) {
                return PointVisualStatus::Scanning;
            }
            return PointVisualStatus::Untested;
    }
}

lv_color_t ScanScreen::visualColor(PointVisualStatus status) {
    switch (status) {
        case PointVisualStatus::Scanning:
            return lv_color_hex(0x28BCE8);
        case PointVisualStatus::Ok:
            return lv_color_hex(0x35D273);
        case PointVisualStatus::Open:
            return lv_color_hex(0xE9EEF2);
        case PointVisualStatus::ShortCircuit:
            return lv_color_hex(0xFFD34D);
        case PointVisualStatus::WrongConnection:
            return lv_color_hex(0xF04444);
        case PointVisualStatus::HighResistance:
            return lv_color_hex(0xFF922E);
        case PointVisualStatus::Untested:
        default:
            return lv_color_hex(0x35434E);
    }
}

void ScanScreen::formatTestIndex(uint8_t testIndex,
                                 char* output,
                                 size_t outputSize) const {
    if (testIndex == kPeTestIndex && session_.profile().includePe) {
        snprintf(output, outputSize, "PE");
    } else if (testIndex == kDrainShieldTestIndex &&
               session_.profile().includeDrainShield) {
        snprintf(output, outputSize, "dS");
    } else if (testIndex >= kFirstSignalTestIndex &&
               testIndex <= kLastSignalTestIndex) {
        snprintf(output, outputSize, "%u", testIndex);
    } else {
        snprintf(output, outputSize, "--");
    }
}

}  // namespace mg::p4
