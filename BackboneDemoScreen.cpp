#include "BackboneDemoScreen.h"
#include "ProductionWorkflowBridge.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

bool tr() { return currentP4Language() == P4Language::Turkish; }

void flat(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text,
                    const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

const char* pairFrequency(uint8_t index) {
    static const char* values[] = {"20 kHz", "50 kHz", "100 kHz", "250 kHz"};
    return values[index % 4U];
}

uint16_t kelvinCurrent(uint8_t index) {
    static const uint16_t values[] = {10U, 50U, 100U};
    return values[index % 3U];
}

const char* peDsMode(uint8_t index, bool turkish) {
    static const char* en[] = {"SEPARATE", "BONDED", "A ONLY", "B ONLY"};
    static const char* tt[] = {"AYRI", "BONDED", "SADECE A", "SADECE B"};
    return (turkish ? tt : en)[index % 4U];
}

const char* componentName(uint8_t index, bool turkish) {
    static const char* en[] = {"DIODE", "LED", "CAPACITOR"};
    static const char* tt[] = {"DIYOT", "LED", "KAPASITOR"};
    return (turkish ? tt : en)[index % 3U];
}

const char* segmentName(uint8_t index) {
    static const char* values[] = {"A-B", "B-C", "C-D"};
    return values[index % 3U];
}

BackboneRunState toBackboneState(DigitalWorkflowState state) {
    switch (state) {
        case DigitalWorkflowState::Running: return BackboneRunState::Running;
        case DigitalWorkflowState::Paused: return BackboneRunState::Paused;
        case DigitalWorkflowState::Complete: return BackboneRunState::Complete;
        case DigitalWorkflowState::Fault: return BackboneRunState::Fault;
        case DigitalWorkflowState::Idle: default: return BackboneRunState::Idle;
    }
}

BackboneRunState toBackboneState(QualityWorkflowState state) {
    switch (state) {
        case QualityWorkflowState::Running: return BackboneRunState::Running;
        case QualityWorkflowState::Complete: return BackboneRunState::Complete;
        case QualityWorkflowState::Warning: return BackboneRunState::Complete;
        case QualityWorkflowState::Fault: return BackboneRunState::Fault;
        case QualityWorkflowState::Idle: default: return BackboneRunState::Idle;
    }
}

BackboneRunState toBackboneState(HighSpeedWorkflowState state) {
    switch (state) {
        case HighSpeedWorkflowState::Running: return BackboneRunState::Running;
        case HighSpeedWorkflowState::Paused: return BackboneRunState::Paused;
        case HighSpeedWorkflowState::Complete: return BackboneRunState::Complete;
        case HighSpeedWorkflowState::Warning: return BackboneRunState::Complete;
        case HighSpeedWorkflowState::Fault: return BackboneRunState::Fault;
        case HighSpeedWorkflowState::Idle: default: return BackboneRunState::Idle;
    }
}

BackboneRunState toBackboneState(SystemWorkflowState state) {
    switch (state) {
        case SystemWorkflowState::Running: return BackboneRunState::Running;
        case SystemWorkflowState::Complete: return BackboneRunState::Complete;
        case SystemWorkflowState::Warning: return BackboneRunState::Complete;
        case SystemWorkflowState::Fault: return BackboneRunState::Fault;
        case SystemWorkflowState::Idle: default: return BackboneRunState::Idle;
    }
}

void reportCopy(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0U) return;
    snprintf(dst, dstSize, "%s", src != nullptr ? src : "");
}

void reportAppend(char* dst, size_t dstSize, const char* text) {
    if (dst == nullptr || dstSize == 0U || text == nullptr) return;
    const size_t used = strlen(dst);
    if (used + 1U >= dstSize) return;
    snprintf(dst + used, dstSize - used, "%s", text);
}

void formatReportNode(char side, uint8_t index, char* output, size_t outputSize) {
    if (index == kPeTestIndex) snprintf(output, outputSize, "%cPE", side);
    else if (index == kDrainShieldTestIndex) snprintf(output, outputSize, "%cdS", side);
    else if (index <= kLastSignalTestIndex) snprintf(output, outputSize, "%c%u", side, static_cast<unsigned>(index));
    else snprintf(output, outputSize, "-");
}

void formatReportMask(uint64_t mask, char side, char* output, size_t outputSize) {
    if (output == nullptr || outputSize == 0U) return;
    output[0] = '\0';
    bool first = true;
    for (uint8_t index = 0U; index < kTestPointsPerSide; ++index) {
        if ((mask & (1ULL << index)) == 0ULL) continue;
        char node[12]{};
        formatReportNode(side, index, node, sizeof(node));
        if (!first) reportAppend(output, outputSize, "+");
        reportAppend(output, outputSize, node);
        first = false;
    }
    if (first) reportCopy(output, outputSize, "-");
}

AnalysisReportStatus reportStatus(ElectricalResult result) {
    switch (result) {
        case ElectricalResult::Ok: return AnalysisReportStatus::Pass;
        case ElectricalResult::Open: return AnalysisReportStatus::Open;
        case ElectricalResult::ShortCircuit: return AnalysisReportStatus::ShortCircuit;
        case ElectricalResult::WrongConnection: return AnalysisReportStatus::WrongConnection;
        case ElectricalResult::HighResistance: return AnalysisReportStatus::HighResistance;
        case ElectricalResult::NotMeasured: default: return AnalysisReportStatus::NotMeasured;
    }
}

const char* reportElectricalText(ElectricalResult result, bool turkish) {
    if (turkish) return electricalResultText(result);
    switch (result) {
        case ElectricalResult::Ok: return "OK";
        case ElectricalResult::Open: return "OPEN";
        case ElectricalResult::ShortCircuit: return "SHORT";
        case ElectricalResult::WrongConnection: return "WRONG";
        case ElectricalResult::HighResistance: return "HIGH-R";
        case ElectricalResult::NotMeasured: default: return "N/A";
    }
}

AnalysisReportStatus reportStatus(QualityVerdict verdict, bool highResistanceContext) {
    switch (verdict) {
        case QualityVerdict::Pass: return AnalysisReportStatus::Pass;
        case QualityVerdict::Warning: return highResistanceContext ? AnalysisReportStatus::HighResistance : AnalysisReportStatus::Warning;
        case QualityVerdict::Fail: return highResistanceContext ? AnalysisReportStatus::HighResistance : AnalysisReportStatus::Fail;
        case QualityVerdict::NotApplicable: return AnalysisReportStatus::Info;
        case QualityVerdict::NotRun: default: return AnalysisReportStatus::NotMeasured;
    }
}

AnalysisReportStatus reportStatus(HighSpeedVerdict verdict) {
    switch (verdict) {
        case HighSpeedVerdict::Pass: return AnalysisReportStatus::Pass;
        case HighSpeedVerdict::Warning: return AnalysisReportStatus::Warning;
        case HighSpeedVerdict::Fail: return AnalysisReportStatus::Fail;
        case HighSpeedVerdict::NotRun: default: return AnalysisReportStatus::NotMeasured;
    }
}

AnalysisReportStatus reportStatus(SystemVerdict verdict) {
    switch (verdict) {
        case SystemVerdict::Pass: return AnalysisReportStatus::Pass;
        case SystemVerdict::Warning: return AnalysisReportStatus::Warning;
        case SystemVerdict::Fail: return AnalysisReportStatus::Fail;
        case SystemVerdict::Blocked: return AnalysisReportStatus::Warning;
        case SystemVerdict::NotRun: default: return AnalysisReportStatus::NotMeasured;
    }
}

const char* selfTestResultText(SelfTestItemResult result, bool turkish) {
    switch (result) {
        case SelfTestItemResult::Pass: return "PASS";
        case SelfTestItemResult::Warning: return turkish ? "UYARI" : "WARNING";
        case SelfTestItemResult::Fail: return "FAIL";
        case SelfTestItemResult::Pending: default: return turkish ? "BEKLIYOR" : "PENDING";
    }
}

AnalysisReportStatus selfTestStatus(SelfTestItemResult result) {
    switch (result) {
        case SelfTestItemResult::Pass: return AnalysisReportStatus::Pass;
        case SelfTestItemResult::Warning: return AnalysisReportStatus::Warning;
        case SelfTestItemResult::Fail: return AnalysisReportStatus::Fail;
        case SelfTestItemResult::Pending: default: return AnalysisReportStatus::NotMeasured;
    }
}

bool isTerminal(BackboneRunState state) {
    return state == BackboneRunState::Complete || state == BackboneRunState::Fault;
}

void resetAnalysisReportData(AnalysisReportData& report) {
    // Avoid `report = AnalysisReportData{}` here. AnalysisReportData is ~41 KiB
    // and GCC 14.2.0 for ESP32-P4 can ICE while materializing the temporary.
    report.module = BackboneModuleId::Scan128;
    report.demoMode = true;
    report.liveSnapshot = false;
    report.rowCount = 0U;
    memset(report.title, 0, sizeof(report.title));
    memset(report.summary, 0, sizeof(report.summary));
    memset(report.columns, 0, sizeof(report.columns));
    for (size_t i = 0U; i < AnalysisReportData::kMaxRows; ++i) {
        memset(report.rows[i].cells, 0, sizeof(report.rows[i].cells));
        report.rows[i].status = AnalysisReportStatus::Info;
    }
}

}  // namespace

void BackboneDemoScreen::begin(BackboneModuleId module, const char* workflowTitle) {
    cableLearnCompletionPending_ = false;
    analysisReportPending_ = false;
    resetAnalysisReportData(analysisReport_);
    backRequested_ = false;
    haveSnapshot_ = false;
    selectedTdrPin_ = 17;
    selectedPair_ = 1;
    selectedPairFrequency_ = 2;
    selectedKelvinCurrent_ = 0;
    selectedPeDsMode_ = 0;
    selectedComponent_ = 0;
    selectedSegment_ = 0;
    scanAToB_ = true;
    probeTone_ = true;
    voiceEnabled_ = true;
    environmentLive_ = false;
    environmentLogging_ = false;
    contextText_[0] = '\0';
    lastWorkflowToken_[0] = '\0';
    digitalCore_.resetAll();
    qualityCore_.resetAll();
    highSpeedCore_.resetAll();
    systemCore_.resetAll();

    backbone_.select(module, workflowTitle);
    buildUi();
    refreshContext();
    refresh(true);
    Serial.printf("[BACKBONE-UI] role-aware module=%u workflow=%s\n",
                  static_cast<unsigned>(module),
                  workflowTitle != nullptr ? workflowTitle : "-");
    Serial.flush();
}

void BackboneDemoScreen::buildUi() {
    if (!screen_) screen_ = lv_obj_create(nullptr); else lv_obj_clean(screen_);
    lv_scr_load(screen_);
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 88);
    flat(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    const char* workflow = backbone_.workflowTitle();
    const bool hasWorkflow = workflow != nullptr && workflow[0] != '\0';
    titleLabel_ = makeLabel(header,
        hasWorkflow ? workflow : FeatureBackbone::moduleTitle(backbone_.selectedModule(), tr()),
        p4Font20(), lv_color_hex(0xEAF6FF));
    lv_obj_align(titleLabel_, LV_ALIGN_TOP_MID, 0, 8);

    moduleLabel_ = makeLabel(header,
        FeatureBackbone::moduleTitle(backbone_.selectedModule(), tr()),
        p4Font14(), lv_color_hex(0x9CC6DC));
    lv_obj_align(moduleLabel_, LV_ALIGN_BOTTOM_MID, 0, -8);

    modeLabel_ = makeLabel(screen_, "", p4Font14(), lv_color_hex(0xFFD36A));
    lv_obj_set_pos(modeLabel_, 24, 98);

    stateLabel_ = makeLabel(screen_, "", p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_align(stateLabel_, LV_ALIGN_TOP_RIGHT, -24, 99);

    contextLabel_ = makeLabel(screen_, "", p4Font14(), lv_color_hex(0x8FE4FF));
    lv_obj_set_pos(contextLabel_, 24, 128);
    lv_obj_set_width(contextLabel_, 976);
    lv_label_set_long_mode(contextLabel_, LV_LABEL_LONG_WRAP);

    lv_obj_t* phasePanel = lv_obj_create(screen_);
    lv_obj_set_pos(phasePanel, 24, 166);
    lv_obj_set_size(phasePanel, 976, 122);
    flat(phasePanel);
    lv_obj_set_style_bg_color(phasePanel, lv_color_hex(0x102431), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(phasePanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(phasePanel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(phasePanel, lv_color_hex(0x3E6579), LV_PART_MAIN);
    lv_obj_set_style_radius(phasePanel, 12, LV_PART_MAIN);

    phaseLabel_ = makeLabel(phasePanel, "", p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_align(phaseLabel_, LV_ALIGN_TOP_MID, 0, 18);

    progressBar_ = lv_bar_create(phasePanel);
    lv_obj_set_size(progressBar_, 820, 22);
    lv_obj_align(progressBar_, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_bar_set_range(progressBar_, 0, 100);
    lv_obj_set_style_bg_color(progressBar_, lv_color_hex(0x263A46), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar_, lv_color_hex(0x2A9D62), LV_PART_INDICATOR);

    progressLabel_ = makeLabel(phasePanel, "0%", p4Font14(), lv_color_hex(0xEAF6FF));
    lv_obj_align(progressLabel_, LV_ALIGN_BOTTOM_RIGHT, -22, -17);

    for (uint8_t i = 0; i < 3; ++i) {
        lv_obj_t* card = lv_obj_create(screen_);
        lv_obj_set_pos(card, static_cast<lv_coord_t>(24 + i * 326), 302);
        lv_obj_set_size(card, 302, 92);
        flat(card);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x132C39), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(card, lv_color_hex(0x355B6D), LV_PART_MAIN);
        lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
        metricLabels_[i] = makeLabel(card, "", p4Font14(), lv_color_hex(0xDDEBF2));
        lv_obj_set_width(metricLabels_[i], 278);
        lv_obj_set_style_text_align(metricLabels_[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_center(metricLabels_[i]);
    }

    constexpr lv_coord_t x0 = 24;
    constexpr lv_coord_t y = 414;
    constexpr lv_coord_t gap = 10;
    constexpr lv_coord_t w = 187;
    constexpr lv_coord_t h = 118;
    for (uint8_t i = 0; i < kButtonCount; ++i) {
        buttons_[i] = makeButton(screen_,
                                 static_cast<lv_coord_t>(x0 + i * (w + gap)), y,
                                 w, h, buttonColor(i), i,
                                 i == 4U ? Action::Back : static_cast<Action>(i));
    }
}

lv_obj_t* BackboneDemoScreen::makeButton(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                         lv_coord_t w, lv_coord_t h, lv_color_t color,
                                         uint8_t bindingIndex, Action action) {
    bindings_[bindingIndex] = {this, action};
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(0x79A0B2), LV_PART_MAIN);
    lv_obj_add_event_cb(button, buttonCallback, LV_EVENT_CLICKED, &bindings_[bindingIndex]);
    buttonLabels_[bindingIndex] = makeLabel(button, "", p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_set_width(buttonLabels_[bindingIndex], w - 18);
    lv_label_set_long_mode(buttonLabels_[bindingIndex], LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(buttonLabels_[bindingIndex], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(buttonLabels_[bindingIndex]);
    return button;
}

void BackboneDemoScreen::buttonCallback(lv_event_t* event) {
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->owner != nullptr) binding->owner->handle(binding->action);
}

void BackboneDemoScreen::setContext(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(contextText_, sizeof(contextText_), fmt, args);
    va_end(args);
    if (contextLabel_ != nullptr) lv_label_set_text(contextLabel_, contextText_);
}

void BackboneDemoScreen::startWorkflow(const char* turkish, const char* english) {
    const BackboneModuleId module = backbone_.selectedModule();
    backbone_.select(module, tr() ? turkish : english);
    backbone_.start();
    haveSnapshot_ = false;
}

void BackboneDemoScreen::startOrRestart() {
    const BackboneSnapshot snap = backbone_.snapshot();
    if (snap.state == BackboneRunState::Running || snap.state == BackboneRunState::Paused) {
        backbone_.pauseResume();
    } else {
        backbone_.start();
    }
}

void BackboneDemoScreen::toggleRun() {
    backbone_.pauseResume();
}

void BackboneDemoScreen::handle(Action action) {
    if (action == Action::Back) {
        backbone_.reset();
        backRequested_ = true;
        return;
    }

    const uint8_t a = static_cast<uint8_t>(action);
    switch (backbone_.selectedModule()) {
        case BackboneModuleId::Scan128:
            if (a == 0U) {
                digitalCore_.scan().pauseResume();
            } else if (a == 1U) {
                digitalCore_.scan().step();
            } else if (a == 2U) {
                scanAToB_ = !scanAToB_;
                setContext(tr() ? "Görsel yön tercihi: %s | fiziksel core iki yönü de tarar; sender LOW, kalan 127 node INPUT/read"
                                : "Display direction preference: %s | physical core sweeps both ways; sender LOW, remaining 127 nodes INPUT/read",
                           scanAToB_ ? "A -> B" : "B -> A");
            } else if (a == 3U) {
                digitalCore_.scan().reset();
                setContext(tr() ? "128-node dijital core yeni tarama için hazır. Gerçek MCP23S17 driver henüz bağlı değil."
                                : "128-node digital core ready for a new scan. Real MCP23S17 driver is not attached yet.");
            }
            break;

        case BackboneModuleId::Kelvin:
            if (a == 0U) {
                qualityCore_.kelvin().setCurrentMa(kelvinCurrent(selectedKelvinCurrent_));
                qualityCore_.kelvin().setAmbientC(qualityCore_.environment().sample().temperatureC);
                qualityCore_.kelvin().measure();
                setContext(tr() ? "Kelvin demo ölçümü başladı: %u mA | ADS122C04 yolu daha sonra hardware driver ile bağlanacak."
                                : "Kelvin demo measurement started: %u mA | ADS122C04 path will attach later through the hardware driver.",
                           static_cast<unsigned>(qualityCore_.kelvin().currentMa()));
            } else if (a == 1U) {
                qualityCore_.kelvin().zero();
                setContext(tr() ? "ZERO/OFFSET kaydı hazır: DUT %.1f µV / shunt %.1f µV. Gerçek kartta current-off + ADC short ortalaması kullanılacak."
                                : "ZERO/OFFSET record ready: DUT %.1f µV / shunt %.1f µV. Real hardware will average current-off + ADC-short paths.",
                           static_cast<double>(qualityCore_.kelvin().dutOffsetUv()),
                           static_cast<double>(qualityCore_.kelvin().shuntOffsetUv()));
            } else if (a == 2U) {
                selectedKelvinCurrent_ = static_cast<uint8_t>((selectedKelvinCurrent_ + 1U) % 3U);
                qualityCore_.kelvin().setCurrentMa(kelvinCurrent(selectedKelvinCurrent_));
                refreshContext();
            } else if (a == 3U) {
                char ref[128]{};
                qualityCore_.kelvin().formatReference(ref, sizeof(ref));
                setContext(tr() ? "Referans: %s | R_RAW saklanır; sıcaklık normalize değeri ayrı alan olur."
                                : "Reference: %s | R_RAW is preserved; temperature-normalized estimate is a separate field.", ref);
            }
            break;

        case BackboneModuleId::SelfTestCalibration:
            if (a == 0U) {
                digitalCore_.selfTest().start(SelfTestMode::Fast);
                backbone_.select(BackboneModuleId::SelfTestCalibration, tr() ? "HIZLI SELF TEST" : "FAST SELF TEST");
                setContext(tr() ? "FAST plan: güvenli çıkışlar -> I2C/SPI -> MCP23S17 readback -> Kelvin ZERO."
                                : "FAST plan: safe outputs -> I2C/SPI -> MCP23S17 readback -> Kelvin ZERO.");
            } else if (a == 1U) {
                digitalCore_.selfTest().start(SelfTestMode::Full);
                backbone_.select(BackboneModuleId::SelfTestCalibration, tr() ? "TAM SELF TEST" : "FULL SELF TEST");
                setContext(tr() ? "FULL plan: FAST + 128-node loop + glitch + TDR + Pair + USB-C güvenli kontrol."
                                : "FULL plan: FAST + 128-node loop + glitch + TDR + Pair + USB-C safe check.");
            } else if (a == 2U) {
                digitalCore_.selfTest().start(SelfTestMode::Calibration);
                backbone_.select(BackboneModuleId::SelfTestCalibration, tr() ? "KALİBRASYON" : "CALIBRATION");
                setContext(tr() ? "CAL plan: SHORT -> 10 mΩ -> 100 mΩ -> 1 Ω -> CRC-korumalı kalibrasyon verisi."
                                : "CAL plan: SHORT -> 10 mΩ -> 100 mΩ -> 1 Ω -> CRC-protected calibration data.");
            } else if (a == 3U) {
                const auto& st = digitalCore_.selfTest();
                setContext(tr() ? "Sonuç: PASS=%u WARNING=%u FAIL=%u | gerçek kartta arızalı fonksiyon ayrı kilitlenir."
                                : "Result: PASS=%u WARNING=%u FAIL=%u | real board locks only the failed function.",
                           static_cast<unsigned>(st.passCount()),
                           static_cast<unsigned>(st.warningCount()),
                           static_cast<unsigned>(st.failCount()));
                if (st.itemCount() > 0U) {
                    buildAnalysisReport();
                    analysisReportPending_ = true;
                }
            }
            break;

        case BackboneModuleId::Tdr:
            if (a == 0U) {
                selectedTdrPin_ = selectedTdrPin_ <= 1U ? 62U : static_cast<uint8_t>(selectedTdrPin_ - 1U);
                highSpeedCore_.tdr().setNode(selectedTdrPin_);
                refreshContext();
            } else if (a == 1U) {
                selectedTdrPin_ = selectedTdrPin_ >= 62U ? 1U : static_cast<uint8_t>(selectedTdrPin_ + 1U);
                highSpeedCore_.tdr().setNode(selectedTdrPin_);
                refreshContext();
            } else if (a == 2U || a == 3U) {
                backbone_.select(BackboneModuleId::Tdr, tr() ? "TDR ÖLÇÜM" : "TDR MEASUREMENT");
                highSpeedCore_.tdr().setNode(selectedTdrPin_);
                highSpeedCore_.tdr().start();
                setContext(tr() ? "A%u TDR demo: TDC arm -> pulse -> reflection -> VOP-kalibre mesafe. Gerçek RF/TDC sürücüsü PCB ile bağlanacak."
                                : "A%u TDR demo: TDC arm -> pulse -> reflection -> VOP-calibrated distance. Real RF/TDC driver attaches with PCB.",
                           static_cast<unsigned>(selectedTdrPin_));
            }
            break;

        case BackboneModuleId::PairIntegrity:
            if (a == 0U) {
                selectedPair_ = selectedPair_ <= 1U ? 32U : static_cast<uint8_t>(selectedPair_ - 1U);
                highSpeedCore_.pair().setPair(selectedPair_);
                refreshContext();
            } else if (a == 1U) {
                selectedPair_ = selectedPair_ >= 32U ? 1U : static_cast<uint8_t>(selectedPair_ + 1U);
                highSpeedCore_.pair().setPair(selectedPair_);
                refreshContext();
            } else if (a == 2U) {
                highSpeedCore_.pair().setPair(selectedPair_);
                static const uint32_t hz[] = {20000U, 50000U, 100000U, 250000U};
                highSpeedCore_.pair().setFrequencyHz(hz[selectedPairFrequency_ % 4U]);
                highSpeedCore_.pair().start();
                setContext(tr() ? "Pair %u @ %s: amplitude / phase / crosstalk ve split-pair karşılaştırması çalışıyor."
                                : "Pair %u @ %s: amplitude / phase / crosstalk and split-pair comparison is running.",
                           static_cast<unsigned>(selectedPair_), pairFrequency(selectedPairFrequency_));
            } else if (a == 3U) {
                selectedPairFrequency_ = static_cast<uint8_t>((selectedPairFrequency_ + 1U) % 4U);
                static const uint32_t hz[] = {20000U, 50000U, 100000U, 250000U};
                highSpeedCore_.pair().setFrequencyHz(hz[selectedPairFrequency_ % 4U]);
                refreshContext();
            }
            break;

        case BackboneModuleId::UsbC:
            if (a == 0U || a == 1U) {
                highSpeedCore_.usbC().readCable();
                setContext(tr() ? "Attach -> orientation -> güvenli düşük güçlü VBUS/VCONN -> SOP' Discover Identity. 5A/EPR yük testi yapılmaz."
                                : "Attach -> orientation -> safe low-power VBUS/VCONN -> SOP' Discover Identity. No 5A/EPR load test is applied.");
            } else if (a == 2U) {
                highSpeedCore_.usbC().compareProfile();
                char profile[96]{};
                highSpeedCore_.usbC().formatProfile(profile, sizeof(profile));
                setContext(tr() ? "Profil karşılaştırması: %s" : "Profile comparison: %s", profile);
                buildAnalysisReport();
                analysisReportPending_ = true;
            } else if (a == 3U) {
                char identity[112]{};
                highSpeedCore_.usbC().formatIdentity(identity, sizeof(identity));
                setContext(tr() ? "%s | yalnız cable identity; yüksek güç ve USB4 SI sertifikasyonu kapsam dışı."
                                : "%s | cable identity only; high-power loading and USB4 SI certification are out of scope.", identity);
                if (highSpeedCore_.usbC().result().valid) {
                    buildAnalysisReport();
                    analysisReportPending_ = true;
                }
            }
            break;

        case BackboneModuleId::PeDs:
            if (a == 0U) {
                qualityCore_.peDs().setMode(static_cast<PeDsProfileMode>(selectedPeDsMode_ % 4U));
                qualityCore_.peDs().startTest();
                refreshContext();
            } else if (a == 1U) {
                selectedPeDsMode_ = static_cast<uint8_t>((selectedPeDsMode_ + 1U) % 4U);
                qualityCore_.peDs().setMode(static_cast<PeDsProfileMode>(selectedPeDsMode_));
                refreshContext();
            } else if (a == 2U) {
                qualityCore_.peDs().setMode(static_cast<PeDsProfileMode>(selectedPeDsMode_ % 4U));
                qualityCore_.peDs().startKelvin();
                setContext(tr() ? "PE/dS Kelvin kalite akışı başladı. Tek uçlu dS profillerinde iki uç direnç sonucu N/A kalır."
                                : "PE/dS Kelvin-quality flow started. Two-ended resistance remains N/A for single-ended dS profiles.");
            } else if (a == 3U) {
                qualityCore_.peDs().cycleBondPolicy();
                char policy[24]{};
                qualityCore_.peDs().formatBondPolicy(policy, sizeof(policy));
                setContext(tr() ? "Bond politikası: %s | konnektör markasından PE-dS bağı tahmin edilmez."
                                : "Bond policy: %s | PE-dS bonding is never inferred from connector brand.", policy);
            }
            break;

        case BackboneModuleId::FixtureSpc:
            if (a == 0U) {
                qualityCore_.fixtureSpc().startFixtureCheck();
                setContext(tr() ? "Fixture master/reference karşılaştırması başladı. Production DUT kayıtları baseline'ı değiştiremez."
                                : "Fixture master/reference comparison started. Production DUT records cannot alter the baseline.");
            } else if (a == 1U) {
                qualityCore_.fixtureSpc().nextTrendPin();
                char trend[128]{};
                qualityCore_.fixtureSpc().formatTrend(trend, sizeof(trend));
                setContext(tr() ? "Trend: %s" : "Trend: %s", trend);
            } else if (a == 2U) {
                qualityCore_.fixtureSpc().toggleReportView();
                char report[128]{};
                qualityCore_.fixtureSpc().formatReport(report, sizeof(report));
                setContext(tr() ? "SPC raporu: %s" : "SPC report: %s", report);
                buildAnalysisReport();
                analysisReportPending_ = true;
            } else if (a == 3U) {
                char master[144]{};
                qualityCore_.fixtureSpc().formatMasterReference(master, sizeof(master));
                setContext("%s", master);
            }
            break;

        case BackboneModuleId::SmartProbe:
            if (a == 0U) {
                digitalCore_.probe().pauseResume();
                setContext(tr() ? "Probe taraması: bütün sürücüler high-Z; tek aday pull-up mantığı demo core içinde taranıyor."
                                : "Probe scan: all drivers high-Z; one-candidate pull-up logic is running in the demo core.");
            } else if (a == 1U) {
                digitalCore_.probe().nextEnd();
                char selected[24]{};
                digitalCore_.probe().formatSelected(selected, sizeof(selected));
                setContext(tr() ? "Yeni uç seçildi: %s | önceki net temizlendi."
                                : "New end selected: %s | previous net cleared.", selected);
            } else if (a == 2U) {
                char found[150]{};
                digitalCore_.probe().formatFoundNet(found, sizeof(found));
                setContext(tr() ? "Bulunan connected-component: %s"
                                : "Found connected component: %s", found);
                buildAnalysisReport();
                analysisReportPending_ = true;
            } else if (a == 3U) {
                probeTone_ = !probeTone_;
                refreshContext();
            }
            break;

        case BackboneModuleId::Network:
            if (a == 0U) {
                char status[144]{};
                systemCore_.network().formatStatus(status, sizeof(status));
                setContext(tr() ? "Ağ durumu: %s | Captive Portal şirket ağından bağımsızdır."
                                : "Network status: %s | Captive Portal stays independent from company network.", status);
            } else if (a == 1U) {
                backbone_.select(BackboneModuleId::Network, tr() ? "PR ARA" : "PR LOOKUP");
                systemCore_.network().lookupPr("PR123456");
                setContext(tr() ? "PR123456 -> HTTP GET -> müşteri referansı/revision -> profil doğrulama. Önce eski profil temizlenir."
                                : "PR123456 -> HTTP GET -> customer reference/revision -> profile validation. Old profile is cleared first.");
            } else if (a == 2U) {
                systemCore_.network().testServer();
                char status[144]{};
                systemCore_.network().formatStatus(status, sizeof(status));
                setContext(tr() ? "Sunucu testi: %s | serbest kablo testi ağ olmadan devam eder."
                                : "Server test: %s | free-cable testing remains available without network.", status);
            } else if (a == 3U) {
                char cache[144]{};
                systemCore_.network().formatCache(cache, sizeof(cache));
                setContext(tr() ? "Profil cache: %s" : "Profile cache: %s", cache);
            }
            break;

        case BackboneModuleId::Voice:
            if (a == 0U) {
                systemCore_.voice().toggleWakeWord();
                voiceEnabled_ = systemCore_.voice().wakeEnabled();
                refreshContext();
            } else if (a == 1U) {
                systemCore_.voice().micTest();
                char voice[144]{};
                systemCore_.voice().formatResult(voice, sizeof(voice));
                setContext(tr() ? "Mikrofon/codec demo testi: %s" : "Microphone/codec demo test: %s", voice);
                buildAnalysisReport();
                analysisReportPending_ = true;
            } else if (a == 2U) {
                systemCore_.voice().listenAndRecognize();
                char voice[144]{};
                systemCore_.voice().formatResult(voice, sizeof(voice));
                setContext(tr() ? "Hi MG demo komutu: %s | yalnız güvenli state-machine intent kuyruğa alınır."
                                : "Hi MG demo command: %s | only safe state-machine intents are queued.", voice);
                buildAnalysisReport();
                analysisReportPending_ = true;
                systemCore_.voice().nextDemoCommand();
            } else if (a == 3U) {
                systemCore_.voice().soundTest();
                setContext(tr() ? "Hoparlör/codec demo testi: PASS/FAIL/OPEN/SHORT ses çıkışı omurgası hazır."
                                : "Speaker/codec demo test: PASS/FAIL/OPEN/SHORT audio-output backbone ready.");
                buildAnalysisReport();
                analysisReportPending_ = true;
            }
            break;

        case BackboneModuleId::EnvironmentAwg:
            if (a == 0U) {
                qualityCore_.environment().measure();
                const auto& e = qualityCore_.environment().sample();
                setContext(tr() ? "SHT40 demo örneği: %.2f °C / %.1f%% RH | R_RAW hiçbir zaman üzerine yazılmaz."
                                : "SHT40 demo sample: %.2f °C / %.1f%% RH | R_RAW is never overwritten.",
                           static_cast<double>(e.temperatureC), static_cast<double>(e.humidityRh));
                buildAnalysisReport();
                analysisReportPending_ = true;
            } else if (a == 1U) {
                qualityCore_.environment().toggleLive();
                environmentLive_ = qualityCore_.environment().live();
                refreshContext();
            } else if (a == 2U) {
                qualityCore_.environment().toggleLogging();
                environmentLogging_ = qualityCore_.environment().logging();
                refreshContext();
            } else if (a == 3U) {
                qualityCore_.environment().cycleWireSpec();
                char wire[128]{};
                qualityCore_.environment().formatWireSpec(wire, sizeof(wire));
                setContext(tr() ? "WireSpec: %s | teorik R yalnız plausibility yardımcısıdır."
                                : "WireSpec: %s | theoretical R is plausibility-only.", wire);
            }
            break;

        case BackboneModuleId::BarcodeQr:
            if (a == 0U) {
                const auto& prev = systemCore_.barcode().result();
                if (prev.valid) systemCore_.barcode().nextDemoCode();
                systemCore_.barcode().scanCurrent();
                char code[96]{};
                systemCore_.barcode().formatCurrentCode(code, sizeof(code));
                setContext(tr() ? "Kod okunuyor: %s | eski profil önce temizlenir; otomatik START yok, READY olsa bile operatör beklenir."
                                : "Scanning code: %s | old profile is cleared first; no automatic START, operator confirmation remains required.", code);
            } else if (a == 1U) {
                systemCore_.barcode().scanAgain();
                setContext(tr() ? "Aynı kod yeniden okunuyor; önceki READY profili geçici olarak temizlendi."
                                : "Re-scanning the same code; previous READY profile was cleared before validation.");
            } else if (a == 2U) {
                systemCore_.barcode().manualEntry("MG-MANUAL-01");
                setContext(tr() ? "Manuel MG profil kimliği doğrulanıyor. Barkod zorunlu değildir; serbest test akışları korunur."
                                : "Manual MG profile ID is being validated. Barcode is optional; free-test workflows remain available.");
            } else if (a == 3U) {
                char profile[144]{};
                systemCore_.barcode().formatLastProfile(profile, sizeof(profile));
                setContext(tr() ? "Son profil: %s" : "Last profile: %s", profile);
            }
            break;

        case BackboneModuleId::ComponentTest:
            if (a == 0U) {
                systemCore_.component().cycleType();
                selectedComponent_ = static_cast<uint8_t>(systemCore_.component().type());
                refreshContext();
            } else if (a == 1U) {
                systemCore_.component().setType(static_cast<BasicComponentType>(selectedComponent_ % 3U));
                systemCore_.component().test();
                setContext(tr() ? "%s temel testi başladı; precision LCR/ESR/LED parlaklık iddiası yok."
                                : "%s basic test started; no precision LCR/ESR/LED brightness claim.",
                           componentName(selectedComponent_, tr()));
            } else if (a == 2U) {
                systemCore_.component().repeat();
                setContext(tr() ? "Seçili komponent testi tekrarlandı." : "Selected component test repeated.");
            } else if (a == 3U) {
                char detail[144]{};
                systemCore_.component().formatDetails(detail, sizeof(detail));
                setContext("%s", detail);
                if (systemCore_.component().result().valid) {
                    buildAnalysisReport();
                    analysisReportPending_ = true;
                }
            }
            break;

        case BackboneModuleId::HardwareValidation:
            if (a == 0U) {
                systemCore_.hardwareValidation().run(HardwareValidationDomain::Gpio);
                setContext(tr() ? "GPIO doğrulama prosedürü DEMO: gerçek continuity/boot-state ölçümü yapılmadığı için Pin Freeze Candidate kalır."
                                : "GPIO validation procedure DEMO: Pin Freeze stays Candidate because real continuity/boot-state is not measured.");
            } else if (a == 1U) {
                systemCore_.hardwareValidation().run(HardwareValidationDomain::Bus);
                setContext(tr() ? "BUS doğrulama DEMO: MCP23S17 SPI 10/8/5 MHz + I2C kanal/adres prosedürü; gerçek kart ölçümü beklenir."
                                : "BUS validation DEMO: MCP23S17 SPI 10/8/5 MHz + I2C channel/address procedure; real-board measurement is required.");
            } else if (a == 2U) {
                systemCore_.hardwareValidation().run(HardwareValidationDomain::Boot);
                setContext(tr() ? "BOOT doğrulama DEMO: GPIO1/2/3/4/20/32/33/45/46/47 başlangıç seviyeleri gerçek JC1060 üzerinde ölçülmeden freeze yok."
                                : "BOOT validation DEMO: no freeze until GPIO1/2/3/4/20/32/33/45/46/47 startup levels are measured on real JC1060.");
            } else if (a == 3U) {
                char report[180]{};
                systemCore_.hardwareValidation().formatReport(report, sizeof(report));
                setContext("%s", report);
                if (systemCore_.hardwareValidation().result().valid) {
                    buildAnalysisReport();
                    analysisReportPending_ = true;
                }
            }
            break;

        case BackboneModuleId::CableLearn:
            if (a == 0U) {
                digitalCore_.learn().pauseResume();
                setContext(tr() ? "Elektriksel net öğrenme aktif: her sender LOW iken aynı taraftaki diğer 63 + karşı taraftaki 64 node okunur."
                                : "Electrical-net learning active: for each sender LOW, the other 63 same-side + all 64 opposite-side nodes are read.");
            } else if (a == 1U) {
                digitalCore_.learn().reset();
                digitalCore_.learn().start();
                setContext(tr() ? "Öğrenme 128 fiziksel node gözleminden baştan oluşturuluyor; A->B ve B->A aynı pasif neti doğrular."
                                : "Learning is rebuilt from 128 physical-node observations; A->B and B->A confirm the same passive net.");
            } else if (a == 2U) {
                char summary[120]{};
                digitalCore_.learn().formatSummary(summary, sizeof(summary));
                if (digitalCore_.learn().hasReviewableMap()) {
                    setContext(tr() ? "%s | örnek ortak net: A7+A9 <-> B9+B12+B14. Harita operatör incelemesine hazır."
                                    : "%s | example common net: A7+A9 <-> B9+B12+B14. Map is ready for operator review.", summary);
                } else {
                    setContext(tr() ? "%s | harita ancak tam çift-yön öğrenme bitince incelenebilir."
                                    : "%s | map can be reviewed only after the complete two-way learn sweep.", summary);
                }
            } else if (a == 3U) {
                if (digitalCore_.learn().hasReviewableMap()) {
                    if (workflowController_ != nullptr) {
                        workflowController_->prepareCableLearn(digitalCore_.fullProfile(),
                                                               digitalCore_.learn().learnedMap());
                    }
                    setContext(tr()
                        ? "Öğrenilen electrical-net profil omurgasına READY olarak aktarıldı. Demo modunda sahte map yazılmaz; Preferences yazılmaz ve otomatik START yok."
                        : "Learned electrical-net profile moved to the workflow as READY. Demo mode does not write Preferences and never auto-starts.");
                } else {
                    setContext(tr() ? "KAYDET BLOKE: öğrenme tamamlanmadı."
                                    : "SAVE BLOCKED: learning is not complete.");
                }
            }
            break;

        case BackboneModuleId::MultiConnector:
            if (a == 0U) {
                digitalCore_.multi().nextSegment();
                selectedSegment_ = static_cast<uint8_t>(digitalCore_.multi().selected());
                refreshContext();
            } else if (a == 1U) {
                digitalCore_.multi().startSelected();
                setContext(tr() ? "%s segmenti test ediliyor; sadece komşu segment aktif, bypass yolu yok."
                                : "%s segment is under test; only the adjacent segment is active, no bypass route.",
                           digitalCore_.multi().segmentName(digitalCore_.multi().selected()));
            } else if (a == 2U) {
                digitalCore_.multi().nextSegment();
                selectedSegment_ = static_cast<uint8_t>(digitalCore_.multi().selected());
                refreshContext();
            } else if (a == 3U) {
                char results[120]{};
                digitalCore_.multi().formatAllResults(results, sizeof(results));
                setContext(tr() ? "%s | segment pin sayıları birbirinden bağımsızdır; A-C/C-A bypass test edilmez."
                                : "%s | segment pin counts are independent; A-C/C-A bypass is never tested.", results);
                buildAnalysisReport();
                analysisReportPending_ = true;
            }
            break;

        case BackboneModuleId::FlexGlitch:
            if (a == 0U) {
                highSpeedCore_.flexGlitch().pauseResume();
                setContext(tr() ? "Comparator + donanım latch demo izleme aktif. PCB'de TLV3601->74LVC1G74->GPIO46 yolu kullanılacak."
                                : "Comparator + hardware-latch demo monitoring active. PCB path will be TLV3601->74LVC1G74->GPIO46.");
            } else if (a == 1U) {
                highSpeedCore_.flexGlitch().clearLatch();
                setContext(tr() ? "GLITCH latch temizlendi; olay geçmişi silinmedi."
                                : "GLITCH latch cleared; event history was not erased.");
            } else if (a == 2U) {
                char result[112]{};
                highSpeedCore_.flexGlitch().formatLastResult(result, sizeof(result));
                setContext(tr() ? "Son olay: %s" : "Last event: %s", result);
                buildAnalysisReport();
                analysisReportPending_ = true;
            } else if (a == 3U) {
                char events[112]{};
                highSpeedCore_.flexGlitch().formatEventList(events, sizeof(events));
                setContext(tr() ? "Olay listesi özeti: %s" : "Event-list summary: %s", events);
                buildAnalysisReport();
                analysisReportPending_ = true;
            }
            break;

        case BackboneModuleId::Count:
            break;
    }

    refresh(true);
}

void BackboneDemoScreen::applyDigitalCoreSnapshot(BackboneSnapshot& snap) {
    const BackboneModuleId module = backbone_.selectedModule();
    auto copyMetric = [](char* dst, size_t size, const char* text) {
        snprintf(dst, size, "%s", text != nullptr ? text : "");
    };

    if (module == BackboneModuleId::Scan128) {
        const auto& runner = digitalCore_.scan();
        const auto& st = runner.stats();
        snap.state = toBackboneState(runner.state());
        snap.progressPercent = runner.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = static_cast<uint8_t>((static_cast<uint16_t>(snap.progressPercent) * 4U) / 100U);
        snprintf(snap.metric1, sizeof(snap.metric1), "%u/%u observations",
                 static_cast<unsigned>(st.completed), static_cast<unsigned>(st.total));
        snprintf(snap.metric2, sizeof(snap.metric2), "OK:%u ERR:%u SAME:%u",
                 static_cast<unsigned>(st.ok), static_cast<unsigned>(st.errorCount()),
                 static_cast<unsigned>(st.sameSideMismatch));
        snprintf(snap.metric3, sizeof(snap.metric3), "sender LOW / remaining 127 INPUT-read");
        return;
    }

    if (module == BackboneModuleId::CableLearn) {
        const auto& learn = digitalCore_.learn();
        snap.state = toBackboneState(learn.state());
        snap.progressPercent = learn.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = static_cast<uint8_t>((static_cast<uint16_t>(snap.progressPercent) * 4U) / 100U);
        snprintf(snap.metric1, sizeof(snap.metric1), "%u/%u raw observations",
                 static_cast<unsigned>(learn.completedObservations()),
                 static_cast<unsigned>(learn.totalObservations()));
        snprintf(snap.metric2, sizeof(snap.metric2), "%u connected electrical nets",
                 static_cast<unsigned>(learn.learnedNetCount()));
        copyMetric(snap.metric3, sizeof(snap.metric3),
                   learn.hasReviewableMap() ? "A7+A9 <-> B9+B12+B14 learned"
                                            : "A/B two-way net discovery active");
        return;
    }

    if (module == BackboneModuleId::SmartProbe) {
        const auto& probe = digitalCore_.probe();
        char selected[24]{};
        char found[64]{};
        probe.formatSelected(selected, sizeof(selected));
        probe.formatFoundNet(found, sizeof(found));
        snap.state = toBackboneState(probe.state());
        snap.progressPercent = probe.progressPercent();
        snap.phaseCount = 4U;
        snap.phaseIndex = static_cast<uint8_t>((static_cast<uint16_t>(snap.progressPercent) * 3U) / 100U);
        snprintf(snap.metric1, sizeof(snap.metric1), "Probe end: %s", selected);
        copyMetric(snap.metric2, sizeof(snap.metric2), found);
        copyMetric(snap.metric3, sizeof(snap.metric3), "all drivers high-Z / one pull-up candidate");
        return;
    }

    if (module == BackboneModuleId::SelfTestCalibration) {
        const auto& self = digitalCore_.selfTest();
        snap.state = toBackboneState(self.state());
        snap.progressPercent = self.progressPercent();
        snap.phaseCount = 6U;
        snap.phaseIndex = static_cast<uint8_t>((static_cast<uint16_t>(snap.progressPercent) * 5U) / 100U);
        copyMetric(snap.metric1, sizeof(snap.metric1), self.currentItemName());
        snprintf(snap.metric2, sizeof(snap.metric2), "PASS:%u WARN:%u FAIL:%u",
                 static_cast<unsigned>(self.passCount()),
                 static_cast<unsigned>(self.warningCount()),
                 static_cast<unsigned>(self.failCount()));
        const char* mode = self.mode() == SelfTestMode::Fast ? "FAST"
                         : self.mode() == SelfTestMode::Full ? "FULL" : "CALIBRATION";
        snprintf(snap.metric3, sizeof(snap.metric3), "Mode: %s | function-level lockout seam", mode);
        return;
    }

    if (module == BackboneModuleId::MultiConnector) {
        const auto& multi = digitalCore_.multi();
        const auto& st = multi.currentStats();
        char summary[64]{};
        multi.formatAllResults(summary, sizeof(summary));
        snap.state = toBackboneState(multi.state());
        snap.progressPercent = multi.progressPercent();
        snap.phaseCount = 6U;
        snap.phaseIndex = static_cast<uint8_t>((static_cast<uint16_t>(snap.progressPercent) * 5U) / 100U);
        snprintf(snap.metric1, sizeof(snap.metric1), "%s | %u signal pins",
                 multi.segmentName(multi.selected()),
                 static_cast<unsigned>(multi.segmentSignalCount(multi.selected())));
        snprintf(snap.metric2, sizeof(snap.metric2), "OK:%u ERR:%u / %u",
                 static_cast<unsigned>(st.ok), static_cast<unsigned>(st.errorCount()),
                 static_cast<unsigned>(st.total));
        copyMetric(snap.metric3, sizeof(snap.metric3), summary);
        return;
    }
}

void BackboneDemoScreen::applyQualityCoreSnapshot(BackboneSnapshot& snap) {
    const BackboneModuleId module = backbone_.selectedModule();

    if (module == BackboneModuleId::Kelvin) {
        const auto& engine = qualityCore_.kelvin();
        const auto& r = engine.result();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = engine.phaseIndex();
        if (r.valid) {
            snprintf(snap.metric1, sizeof(snap.metric1), "I=%u mA | R=%.2f mOhm",
                     static_cast<unsigned>(r.currentMa), static_cast<double>(r.resistanceMilliOhm));
            snprintf(snap.metric2, sizeof(snap.metric2), "Master %.2f | dR %+.2f mOhm",
                     static_cast<double>(r.masterMilliOhm), static_cast<double>(r.deltaMilliOhm));
            snprintf(snap.metric3, sizeof(snap.metric3), "T=%.1f C | R20~%.2f | %s",
                     static_cast<double>(r.ambientC), static_cast<double>(r.normalizedEstimateMilliOhm),
                     qualityVerdictText(r.verdict));
        } else {
            snprintf(snap.metric1, sizeof(snap.metric1), "I=%u mA | ZERO=%s",
                     static_cast<unsigned>(engine.currentMa()), engine.zeroValid() ? "VALID" : "REQUIRED");
            snprintf(snap.metric2, sizeof(snap.metric2), "ADS122C04 ratiometric seam");
            snprintf(snap.metric3, sizeof(snap.metric3), "R_RAW + dR + temperature context");
        }
        return;
    }

    if (module == BackboneModuleId::PeDs) {
        const auto& engine = qualityCore_.peDs();
        const auto& r = engine.result();
        char mode[24]{};
        char policy[24]{};
        engine.formatMode(mode, sizeof(mode));
        engine.formatBondPolicy(policy, sizeof(policy));
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = engine.phaseIndex();
        snprintf(snap.metric1, sizeof(snap.metric1), "Mode=%s | bond=%s", mode, policy);
        if (r.valid) {
            snprintf(snap.metric2, sizeof(snap.metric2), "PE=%.2f mOhm | dS=%s",
                     static_cast<double>(r.peMilliOhm),
                     (r.mode == PeDsProfileMode::AOnly || r.mode == PeDsProfileMode::BOnly) ? "N/A single-ended" : "measured");
            snprintf(snap.metric3, sizeof(snap.metric3), "Bond detected=%s | %s",
                     r.bondDetected ? "YES" : "NO", qualityVerdictText(r.verdict));
        } else {
            snprintf(snap.metric2, sizeof(snap.metric2), "PE and dS are separate physical nodes");
            snprintf(snap.metric3, sizeof(snap.metric3), "profile controls bond expectation");
        }
        return;
    }

    if (module == BackboneModuleId::FixtureSpc) {
        const auto& engine = qualityCore_.fixtureSpc();
        const auto& q = engine.snapshot();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = engine.phaseIndex();
        snprintf(snap.metric1, sizeof(snap.metric1), "Fixture %.0f%% | %s drift %+.2f",
                 static_cast<double>(q.fixtureHealthPercent), q.selectedPin,
                 static_cast<double>(q.selectedPinDriftMilliOhm));
        snprintf(snap.metric2, sizeof(snap.metric2), "mean %.2f | sigma %.2f | Cpk %.2f",
                 static_cast<double>(q.processMeanMilliOhm),
                 static_cast<double>(q.processStdDevMilliOhm),
                 static_cast<double>(q.cpk));
        snprintf(snap.metric3, sizeof(snap.metric3), "PRODUCT=%s | PROCESS=%s",
                 q.productPass ? "PASS" : "FAIL", q.processWarning ? "WARNING" : "OK");
        return;
    }

    if (module == BackboneModuleId::EnvironmentAwg) {
        const auto& engine = qualityCore_.environment();
        const auto& e = engine.sample();
        snap.state = BackboneRunState::Idle;
        snap.progressPercent = e.valid ? 100U : 0U;
        snap.phaseCount = 4U;
        snap.phaseIndex = e.valid ? 3U : 0U;
        snprintf(snap.metric1, sizeof(snap.metric1), "SHT40 %.2f C / %.1f%% RH",
                 static_cast<double>(e.temperatureC), static_cast<double>(e.humidityRh));
        snprintf(snap.metric2, sizeof(snap.metric2), "LIVE=%s | LOG=%s | #%lu",
                 engine.live() ? "ON" : "OFF", engine.logging() ? "ON" : "OFF",
                 static_cast<unsigned long>(engine.logCount()));
        const float theoretical = engine.theoreticalWireMilliOhm();
        if (theoretical < 0.0f) {
            snprintf(snap.metric3, sizeof(snap.metric3), "WireSpec optional | R_RAW preserved");
        } else {
            snprintf(snap.metric3, sizeof(snap.metric3), "Wire rhoL/A~%.1f mOhm | plausibility",
                     static_cast<double>(theoretical));
        }
        return;
    }
}

void BackboneDemoScreen::applyHighSpeedCoreSnapshot(BackboneSnapshot& snap) {
    const BackboneModuleId module = backbone_.selectedModule();

    if (module == BackboneModuleId::Tdr) {
        const auto& engine = highSpeedCore_.tdr();
        const auto& r = engine.result();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = engine.phaseIndex();
        snprintf(snap.metric1, sizeof(snap.metric1), "A%u | VOP=%.2f | CAL=%s",
                 static_cast<unsigned>(engine.node()), static_cast<double>(r.vop),
                 r.calibrationValid ? "VALID" : "REQUIRED");
        if (r.valid) {
            const char* fault = r.fault == TdrFaultType::Open ? "OPEN" :
                                r.fault == TdrFaultType::Short ? "SHORT" : "UNKNOWN";
            snprintf(snap.metric2, sizeof(snap.metric2), "%s | RT=%.1f ns | conf=%u%%",
                     fault, static_cast<double>(r.roundTripNs),
                     static_cast<unsigned>(r.confidencePercent));
            snprintf(snap.metric3, sizeof(snap.metric3), "distance≈%.2f m | A-side reference",
                     static_cast<double>(r.distanceM));
        } else {
            snprintf(snap.metric2, sizeof(snap.metric2), "ADG904 tree -> pulse -> reflection -> TDC7200");
            snprintf(snap.metric3, sizeof(snap.metric3), "approximate fault distance; VOP calibration required");
        }
        return;
    }

    if (module == BackboneModuleId::PairIntegrity) {
        const auto& engine = highSpeedCore_.pair();
        const auto& r = engine.result();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = engine.phaseIndex();
        snprintf(snap.metric1, sizeof(snap.metric1), "Pair %u | %lu kHz",
                 static_cast<unsigned>(engine.pair()),
                 static_cast<unsigned long>(engine.frequencyHz() / 1000U));
        if (r.valid) {
            snprintf(snap.metric2, sizeof(snap.metric2), "A=%.3f | phase=%+.1f deg",
                     static_cast<double>(r.amplitudeRatio), static_cast<double>(r.phaseDegrees));
            snprintf(snap.metric3, sizeof(snap.metric3), "XT=%.1f dB | split=%s | %s",
                     static_cast<double>(r.crosstalkDb), r.splitPair ? "YES" : "NO",
                     highSpeedVerdictText(r.verdict));
        } else {
            snprintf(snap.metric2, sizeof(snap.metric2), "AD9833/THS4551 TX -> routed pair -> ADS8681 RX");
            snprintf(snap.metric3, sizeof(snap.metric3), "amplitude / phase / crosstalk vs master; not CAT6 cert");
        }
        return;
    }

    if (module == BackboneModuleId::UsbC) {
        const auto& engine = highSpeedCore_.usbC();
        const auto& r = engine.result();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 6U;
        snap.phaseIndex = engine.phaseIndex();
        if (r.valid) {
            const char* orientation = r.orientation == UsbCOrientation::Cc1 ? "CC1" :
                                      r.orientation == UsbCOrientation::Cc2 ? "CC2" : "?";
            snprintf(snap.metric1, sizeof(snap.metric1), "%s | E-Marker=%s | VID:%04X",
                     orientation, r.emarkerPresent ? "YES" : "NO", static_cast<unsigned>(r.vendorId));
            snprintf(snap.metric2, sizeof(snap.metric2), "%uA | EPR=%s | VCONN=%s | passive=%s",
                     static_cast<unsigned>(r.declaredCurrentA), r.eprCapable ? "YES" : "NO",
                     r.vconnRequired ? "YES" : "NO", r.passiveCable ? "YES" : "NO");
            snprintf(snap.metric3, sizeof(snap.metric3), "%s | low-power identity only",
                     r.profileCompared ? (r.profileMatch ? "PROFILE MATCH" : "PROFILE MISMATCH")
                                       : "PROFILE COMPARE PENDING");
        } else {
            snprintf(snap.metric1, sizeof(snap.metric1), "FUSB302B | VBUS safe default OFF");
            snprintf(snap.metric2, sizeof(snap.metric2), "orientation -> VCONN -> SOP' Discover Identity");
            snprintf(snap.metric3, sizeof(snap.metric3), "no 5A load / no 48V EPR load / no USB4 SI cert");
        }
        return;
    }

    if (module == BackboneModuleId::FlexGlitch) {
        const auto& engine = highSpeedCore_.flexGlitch();
        const auto& g = engine.snapshot();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = engine.phaseIndex();
        snprintf(snap.metric1, sizeof(snap.metric1), "A17-B17 | ARMED=%s",
                 g.armed ? "YES" : "NO");
        snprintf(snap.metric2, sizeof(snap.metric2), "LATCH=%s | events=%lu",
                 g.latched ? "SET" : "CLEAR", static_cast<unsigned long>(g.eventCount));
        if (g.lastEvent.valid) {
            snprintf(snap.metric3, sizeof(snap.metric3), "last=%lu us @ %lu ms | shortest=%lu us",
                     static_cast<unsigned long>(g.lastEvent.durationUs),
                     static_cast<unsigned long>(g.lastEvent.timestampMs),
                     static_cast<unsigned long>(g.shortestDurationUs));
        } else {
            snprintf(snap.metric3, sizeof(snap.metric3), "TLV3601 -> 74LVC1G74 latch -> GPIO46");
        }
        return;
    }
}


void BackboneDemoScreen::applySystemCoreSnapshot(BackboneSnapshot& snap) {
    const BackboneModuleId module = backbone_.selectedModule();

    if (module == BackboneModuleId::Network) {
        const auto& engine = systemCore_.network();
        const auto& r = engine.result();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = engine.phaseIndex();
        char status[96]{};
        engine.formatStatus(status, sizeof(status));
        snprintf(snap.metric1, sizeof(snap.metric1), "%s", status);
        if (r.valid && r.profileReady) {
            snprintf(snap.metric2, sizeof(snap.metric2), "%s -> %s | rev=%s",
                     r.pr, r.customerReference, r.revision);
            snprintf(snap.metric3, sizeof(snap.metric3), "%s | READY=YES | AUTO-START=NO",
                     r.profileId);
        } else if (r.valid) {
            snprintf(snap.metric2, sizeof(snap.metric2), "server=%s | profile READY=NO",
                     r.serverReachable ? "YES" : "NO");
            snprintf(snap.metric3, sizeof(snap.metric3), "free/manual cable test remains available");
        } else {
            snprintf(snap.metric2, sizeof(snap.metric2), "PR -> HTTP GET -> reference/revision -> profile");
            snprintf(snap.metric3, sizeof(snap.metric3), "stale profile cleared first | no auto-start");
        }
        return;
    }

    if (module == BackboneModuleId::BarcodeQr) {
        const auto& engine = systemCore_.barcode();
        const auto& r = engine.result();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = engine.phaseIndex();
        char current[96]{};
        engine.formatCurrentCode(current, sizeof(current));
        snprintf(snap.metric1, sizeof(snap.metric1), "%s", current);
        if (r.valid) {
            snprintf(snap.metric2, sizeof(snap.metric2), "accepted=%s | old-profile-cleared=%s",
                     r.accepted ? "YES" : "NO", r.oldProfileCleared ? "YES" : "NO");
            snprintf(snap.metric3, sizeof(snap.metric3), "%s | READY=%s | AUTO-START=NO",
                     r.profileId[0] ? r.profileId : "NO PROFILE", r.profileReady ? "YES" : "NO");
        } else {
            snprintf(snap.metric2, sizeof(snap.metric2), "PR / customer-ASML / MG profile / unknown");
            snprintf(snap.metric3, sizeof(snap.metric3), "scan optional | free test remains available");
        }
        return;
    }

    if (module == BackboneModuleId::Voice) {
        const auto& engine = systemCore_.voice();
        const auto& r = engine.result();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 1U;
        snap.phaseIndex = 0U;
        snprintf(snap.metric1, sizeof(snap.metric1), "Hi MG=%s | MIC=%s %u%%",
                 r.wakeWordEnabled ? "ON" : "OFF", r.micHealthy ? "PASS" : "NOT TESTED",
                 static_cast<unsigned>(r.micLevelPercent));
        if (r.commandRecognized) {
            snprintf(snap.metric2, sizeof(snap.metric2), "heard: %s", r.recognizedText);
            snprintf(snap.metric3, sizeof(snap.metric3), "dispatch=%s | critical-block=%s",
                     r.dispatchAllowed ? "QUEUED" : "BLOCKED",
                     r.criticalActionBlocked ? "YES" : "NO");
        } else {
            snprintf(snap.metric2, sizeof(snap.metric2), "wake word opens listening window only");
            snprintf(snap.metric3, sizeof(snap.metric3), "critical config/delete/calibration changes blocked");
        }
        return;
    }

    if (module == BackboneModuleId::ComponentTest) {
        const auto& engine = systemCore_.component();
        const auto& r = engine.result();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 4U;
        snap.phaseIndex = engine.phaseIndex();
        const char* type = r.type == BasicComponentType::Diode ? "DIODE" :
                           r.type == BasicComponentType::Led ? "LED" : "CAPACITOR";
        snprintf(snap.metric1, sizeof(snap.metric1), "TYPE=%s | BASIC ONLY", type);
        if (r.valid && r.type == BasicComponentType::Capacitor) {
            snprintf(snap.metric2, sizeof(snap.metric2), "present=%s | short=%s | rough≈%.1f uF",
                     r.present ? "YES" : "NO", r.shorted ? "YES" : "NO",
                     static_cast<double>(r.roughCapacitanceUf));
            snprintf(snap.metric3, sizeof(snap.metric3), "%s | no precision LCR/ESR/leakage",
                     systemVerdictText(r.verdict));
        } else if (r.valid) {
            snprintf(snap.metric2, sizeof(snap.metric2), "present=%s | Vf≈%.2f V | polarity=%s",
                     r.present ? "YES" : "NO", static_cast<double>(r.forwardVoltageV),
                     r.polarityKnown ? (r.forwardPolarity ? "FORWARD" : "REVERSE") : "?");
            snprintf(snap.metric3, sizeof(snap.metric3), "%s | LED brightness not measured",
                     systemVerdictText(r.verdict));
        } else {
            snprintf(snap.metric2, sizeof(snap.metric2), "presence/open/short/direction where practical");
            snprintf(snap.metric3, sizeof(snap.metric3), "no extra precision analog subsystem");
        }
        return;
    }

    if (module == BackboneModuleId::HardwareValidation) {
        const auto& engine = systemCore_.hardwareValidation();
        const auto& r = engine.result();
        snap.state = toBackboneState(engine.state());
        snap.progressPercent = engine.progressPercent();
        snap.phaseCount = 5U;
        snap.phaseIndex = engine.phaseIndex();
        snprintf(snap.metric1, sizeof(snap.metric1), "PIN FREEZE=CANDIDATE | REAL HW=%s",
                 r.measuredOnRealHardware ? "YES" : "NO");
        if (r.valid) {
            char pins[128]{};
            engine.formatPinSet(pins, sizeof(pins));
            snprintf(snap.metric2, sizeof(snap.metric2), "%s", pins);
        } else {
            snprintf(snap.metric2, sizeof(snap.metric2), "GPIO 1/2/3/4/20/32/33/45/46/47");
        }
        snprintf(snap.metric3, sizeof(snap.metric3), "GPIO5=LCD_RESET RESERVED | Gerber release=BLOCKED");
        return;
    }
}

void BackboneDemoScreen::refreshContext() {
    switch (backbone_.selectedModule()) {
        case BackboneModuleId::Scan128:
            setContext(tr() ? "Tarama yönü: %s | sender LOW, kalan 127 node INPUT/read"
                            : "Scan direction: %s | sender LOW, remaining 127 nodes INPUT/read",
                       scanAToB_ ? "A -> B" : "B -> A");
            break;
        case BackboneModuleId::Kelvin:
            setContext(tr() ? "Seçili akım: %u mA | ZERO ve ÖLÇ ayrı operatör işlemleri"
                            : "Selected current: %u mA | ZERO and MEASURE are separate operator actions",
                       static_cast<unsigned>(kelvinCurrent(selectedKelvinCurrent_)));
            break;
        case BackboneModuleId::SelfTestCalibration:
            setContext(tr() ? "Hızlı test, tam test ve kalibrasyon birbirinden ayrı iş akışlarıdır."
                            : "Fast test, full test and calibration are separate workflows.");
            break;
        case BackboneModuleId::Tdr:
            setContext(tr() ? "Seçili TDR hattı: A%u | VOP-kalibre yaklaşık mesafe; mekanik uzunluk sertifikasyonu değildir."
                            : "Selected TDR line: A%u | VOP-calibrated approximate distance; not mechanical-length certification.",
                       static_cast<unsigned>(selectedTdrPin_));
            break;
        case BackboneModuleId::PairIntegrity:
            setContext(tr() ? "Seçili çift: %u | frekans: %s | amplitude/phase/crosstalk ve split-pair, master ile karşılaştırılır."
                            : "Selected pair: %u | frequency: %s | amplitude/phase/crosstalk and split-pair are compared with master.",
                       static_cast<unsigned>(selectedPair_), pairFrequency(selectedPairFrequency_));
            break;
        case BackboneModuleId::UsbC:
            setContext(tr() ? "FUSB302B cable identity: orientation / E-Marker / 3A-5A / EPR / VCONN; yüksek güç yükü yok."
                            : "FUSB302B cable identity: orientation / E-Marker / 3A-5A / EPR / VCONN; no high-power load.");
            break;
        case BackboneModuleId::PeDs:
            setContext(tr() ? "PE/dS profil modu: %s" : "PE/dS profile mode: %s",
                       peDsMode(selectedPeDsMode_, tr()));
            break;
        case BackboneModuleId::FixtureSpc:
            setContext(tr() ? "Fixture baseline ve üretim SPC aynı veriyi kullanır fakat ürün sonucu ayrı tutulur."
                            : "Fixture baseline and production SPC share evidence but product result stays separate.");
            break;
        case BackboneModuleId::SmartProbe:
            setContext(tr() ? "Probe sesi: %s | tüm sürücüler high-Z"
                            : "Probe tone: %s | all drivers high-Z",
                       probeTone_ ? (tr() ? "AÇIK" : "ON") : (tr() ? "KAPALI" : "OFF"));
            break;
        case BackboneModuleId::Network: {
            char status[144]{};
            systemCore_.network().formatStatus(status, sizeof(status));
            setContext(tr() ? "Şirket ağı yalnız PR/profile lookup için: %s"
                            : "Company network is only for PR/profile lookup: %s", status);
            break;
        }
        case BackboneModuleId::Voice:
            voiceEnabled_ = systemCore_.voice().wakeEnabled();
            setContext(tr() ? "Hi MG: %s | wake-word yalnız dinleme penceresini açar; kritik ayarlar sesle değişmez"
                            : "Hi MG: %s | wake word only opens listening; critical settings cannot change by voice",
                       voiceEnabled_ ? (tr() ? "AÇIK" : "ON") : (tr() ? "KAPALI" : "OFF"));
            break;
        case BackboneModuleId::EnvironmentAwg:
            setContext(tr() ? "Canlı: %s | kayıt: %s | R_RAW korunur"
                            : "Live: %s | logging: %s | R_RAW preserved",
                       environmentLive_ ? (tr() ? "AÇIK" : "ON") : (tr() ? "KAPALI" : "OFF"),
                       environmentLogging_ ? (tr() ? "AÇIK" : "ON") : (tr() ? "KAPALI" : "OFF"));
            break;
        case BackboneModuleId::BarcodeQr: {
            char code[96]{};
            systemCore_.barcode().formatCurrentCode(code, sizeof(code));
            setContext(tr() ? "Kod kaynağı: %s | profil READY olsa bile START operatörden beklenir."
                            : "Code source: %s | even when profile is READY, START waits for operator.", code);
            break;
        }
        case BackboneModuleId::ComponentTest:
            setContext(tr() ? "Seçili komponent: %s | yalnız presence/short/direction/rough-C temel seviyesi"
                            : "Selected component: %s | basic presence/short/direction/rough-C level only",
                       componentName(selectedComponent_, tr()));
            break;
        case BackboneModuleId::HardwareValidation:
            setContext(tr() ? "PIN FREEZE=CANDIDATE | GPIO5 rezerve (LCD_RESET) | gerçek JC1060+PCB ölçümü olmadan Gerber release yok."
                            : "PIN FREEZE=CANDIDATE | GPIO5 reserved (LCD_RESET) | No Gerber freeze until real JC1060+PCB validation.");
            break;
        case BackboneModuleId::CableLearn:
            setContext(tr() ? "Öğrenme sonucu electrical-net taslağıdır; operatör inceleyip kaydeder."
                            : "Learning result is an electrical-net draft; operator reviews and saves it.");
            break;
        case BackboneModuleId::MultiConnector:
            setContext(tr() ? "Aktif segment: %s | yalnız bitişik segment test edilir"
                            : "Active segment: %s | only adjacent segment is tested",
                       segmentName(selectedSegment_));
            break;
        case BackboneModuleId::FlexGlitch:
            setContext(tr() ? "Comparator + donanım latch sürekli izleme; kısa olaylar P4 polling hızına bırakılmaz."
                            : "Comparator + hardware latch continuous monitoring; short events are not left to P4 polling speed.");
            break;
        case BackboneModuleId::Count:
            break;
    }
}

const char* BackboneDemoScreen::buttonText(uint8_t index, const BackboneSnapshot& snap) const {
    const bool turkish = tr();
    const BackboneModuleId m = backbone_.selectedModule();
    if (index == 4U) return p4Texts().back;

    switch (m) {
        case BackboneModuleId::Scan128:
            if (index == 0U) {
                if (snap.state == BackboneRunState::Running) return turkish ? "DURAKLAT" : "PAUSE";
                if (snap.state == BackboneRunState::Paused) return turkish ? "DEVAM" : "RESUME";
                return turkish ? "BAŞLAT" : "START";
            }
            if (index == 1U) return turkish ? "TEK ADIM" : "SINGLE STEP";
            if (index == 2U) return scanAToB_ ? "YÖN A->B" : "YÖN B->A";
            return turkish ? "YENİ TARAMA" : "NEW SCAN";

        case BackboneModuleId::Kelvin:
            if (index == 0U) return turkish ? "ÖLÇ" : "MEASURE";
            if (index == 1U) return "ZERO / OFFSET";
            if (index == 2U) return turkish ? "AKIM SEÇ" : "SELECT CURRENT";
            return turkish ? "REFERANS / ΔR" : "REFERENCE / ΔR";

        case BackboneModuleId::SelfTestCalibration:
            if (index == 0U) return turkish ? "HIZLI TEST" : "FAST TEST";
            if (index == 1U) return turkish ? "TAM TEST" : "FULL TEST";
            if (index == 2U) return turkish ? "KALİBRASYON" : "CALIBRATION";
            return turkish ? "SONUÇLAR" : "RESULTS";

        case BackboneModuleId::Tdr:
            if (index == 0U) return turkish ? "PİN -" : "PIN -";
            if (index == 1U) return turkish ? "PİN +" : "PIN +";
            if (index == 2U) return turkish ? "ÖLÇ" : "MEASURE";
            return turkish ? "TEKRAR" : "REPEAT";

        case BackboneModuleId::PairIntegrity:
            if (index == 0U) return turkish ? "ÇİFT -" : "PAIR -";
            if (index == 1U) return turkish ? "ÇİFT +" : "PAIR +";
            if (index == 2U) return turkish ? "TEST ET" : "TEST";
            return turkish ? "FREKANS" : "FREQUENCY";

        case BackboneModuleId::UsbC:
            if (index == 0U) return turkish ? "KABLOYU OKU" : "READ CABLE";
            if (index == 1U) return turkish ? "YENİDEN OKU" : "READ AGAIN";
            if (index == 2U) return turkish ? "PROFİLLE KARŞILAŞTIR" : "COMPARE PROFILE";
            return turkish ? "DETAYLAR" : "DETAILS";

        case BackboneModuleId::PeDs:
            if (index == 0U) return turkish ? "TEST ET" : "TEST";
            if (index == 1U) return turkish ? "MOD / PROFİL" : "MODE / PROFILE";
            if (index == 2U) return "KELVIN";
            return turkish ? "BOND KURALI" : "BOND RULE";

        case BackboneModuleId::FixtureSpc:
            if (index == 0U) return turkish ? "FİKSTÜR TESTİ" : "FIXTURE TEST";
            if (index == 1U) return turkish ? "TRENDLER" : "TRENDS";
            if (index == 2U) return turkish ? "RAPOR" : "REPORT";
            return "MASTER REF";

        case BackboneModuleId::SmartProbe:
            if (index == 0U) {
                return snap.state == BackboneRunState::Running ? (turkish ? "PROBU DURAKLAT" : "PAUSE PROBE")
                                                               : (turkish ? "PROBU BAŞLAT" : "START PROBE");
            }
            if (index == 1U) return turkish ? "YENİ UÇ" : "NEW END";
            if (index == 2U) return turkish ? "NETİ GÖSTER" : "SHOW NET";
            return turkish ? "SES AÇ/KAPAT" : "TONE ON/OFF";

        case BackboneModuleId::Network:
            if (index == 0U) return turkish ? "AĞ DURUMU" : "NETWORK STATUS";
            if (index == 1U) return turkish ? "PR ARA" : "PR LOOKUP";
            if (index == 2U) return turkish ? "SUNUCU TESTİ" : "SERVER TEST";
            return turkish ? "PROFİL CACHE" : "PROFILE CACHE";

        case BackboneModuleId::Voice:
            if (index == 0U) return systemCore_.voice().wakeEnabled() ? "HI MG: ON" : "HI MG: OFF";
            if (index == 1U) return turkish ? "MİKROFON TESTİ" : "MIC TEST";
            if (index == 2U) return turkish ? "KOMUT DENE" : "TRY COMMAND";
            return turkish ? "SES TESTİ" : "SOUND TEST";

        case BackboneModuleId::EnvironmentAwg:
            if (index == 0U) return turkish ? "ÖLÇ" : "MEASURE";
            if (index == 1U) return environmentLive_ ? (turkish ? "CANLI: AÇIK" : "LIVE: ON")
                                                     : (turkish ? "CANLI: KAPALI" : "LIVE: OFF");
            if (index == 2U) return environmentLogging_ ? (turkish ? "KAYIT: AÇIK" : "LOG: ON")
                                                        : (turkish ? "KAYIT: KAPALI" : "LOG: OFF");
            return "WIRE SPEC";

        case BackboneModuleId::BarcodeQr:
            if (index == 0U) return turkish ? "KOD OKU" : "SCAN CODE";
            if (index == 1U) return turkish ? "TEKRAR OKU" : "SCAN AGAIN";
            if (index == 2U) return turkish ? "MANUEL GİR" : "MANUAL ENTRY";
            return turkish ? "SON PROFİL" : "LAST PROFILE";

        case BackboneModuleId::ComponentTest:
            if (index == 0U) return turkish ? "TÜR SEÇ" : "SELECT TYPE";
            if (index == 1U) return turkish ? "TEST ET" : "TEST";
            if (index == 2U) return turkish ? "TEKRAR" : "REPEAT";
            return turkish ? "DETAY / POLARİTE" : "DETAIL / POLARITY";

        case BackboneModuleId::HardwareValidation:
            if (index == 0U) return turkish ? "GPIO TESTİ" : "GPIO TEST";
            if (index == 1U) return turkish ? "BUS TESTİ" : "BUS TEST";
            if (index == 2U) return turkish ? "BOOT TESTİ" : "BOOT TEST";
            return turkish ? "PIN FREEZE RAPOR" : "PIN FREEZE REPORT";

        case BackboneModuleId::CableLearn:
            if (index == 0U) {
                if (snap.state == BackboneRunState::Running) return turkish ? "DURAKLAT" : "PAUSE";
                if (snap.state == BackboneRunState::Paused) return turkish ? "DEVAM" : "RESUME";
                return turkish ? "ÖĞRENMEYİ BAŞLAT" : "START LEARNING";
            }
            if (index == 1U) return turkish ? "YENİDEN TARA" : "RESCAN";
            if (index == 2U) return turkish ? "HARİTAYI İNCELE" : "REVIEW MAP";
            return turkish ? "KAYDET" : "SAVE";

        case BackboneModuleId::MultiConnector:
            if (index == 0U) return turkish ? "SEGMENT SEÇ" : "SELECT SEGMENT";
            if (index == 1U) return turkish ? "TEST ET" : "TEST";
            if (index == 2U) return turkish ? "SONRAKİ SEGMENT" : "NEXT SEGMENT";
            return turkish ? "SONUÇ" : "RESULT";

        case BackboneModuleId::FlexGlitch:
            if (index == 0U) {
                return snap.state == BackboneRunState::Running ? (turkish ? "İZLEMEYİ DURAKLAT" : "PAUSE MONITOR")
                                                               : (turkish ? "İZLEMEYİ BAŞLAT" : "START MONITOR");
            }
            if (index == 1U) return turkish ? "LATCH TEMİZLE" : "CLEAR LATCH";
            if (index == 2U) return turkish ? "SONUÇ" : "RESULT";
            return turkish ? "OLAY LİSTESİ" : "EVENT LIST";

        case BackboneModuleId::Count:
            break;
    }
    return "-";
}

lv_color_t BackboneDemoScreen::buttonColor(uint8_t index) const {
    static const uint32_t colors[kButtonCount] = {
        0x147A4B, 0x315B8A, 0x8A4F17, 0x166E7A, 0x5E426F
    };
    return lv_color_hex(colors[index < kButtonCount ? index : 4U]);
}

void BackboneDemoScreen::activate() {
    backRequested_ = false;
    if (screen_ != nullptr && lv_scr_act() != screen_) lv_scr_load(screen_);
    refresh(true);
}

void BackboneDemoScreen::update() {
    const uint32_t now = millis();
    backbone_.tick(now);
    digitalCore_.tick(now);
    qualityCore_.tick(now);
    highSpeedCore_.tick(now);
    systemCore_.tick(now);
    syncWorkflowBridge();
    refresh(false);
}

void BackboneDemoScreen::syncWorkflowBridge() {
    if (workflowController_ == nullptr) return;

    if (backbone_.selectedModule() == BackboneModuleId::BarcodeQr) {
        const auto& r = systemCore_.barcode().result();
        if (!r.valid || r.code[0] == '\0' || strcmp(lastWorkflowToken_, r.code) == 0) return;
        snprintf(lastWorkflowToken_, sizeof(lastWorkflowToken_), "%s", r.code);
        workflowController_->acceptBarcodeClassification(
            r.code,
            r.kind == CodeKind::ProductionPr,
            r.kind == CodeKind::CustomerReference,
            r.kind == CodeKind::MgProfile,
            r.accepted,
            r.profileId,
            r.revision);
        Serial.printf("[WORKFLOW] barcode source=%s ready=%u autoStart=0\n",
                      r.code, workflowController_->snapshot().state == TestWorkflowState::Ready ? 1U : 0U);
        Serial.flush();
        return;
    }

    if (backbone_.selectedModule() == BackboneModuleId::Network) {
        const auto& r = systemCore_.network().result();
        if (!r.valid || !r.profileReady || r.pr[0] == '\0' || strcmp(lastWorkflowToken_, r.pr) == 0) return;
        snprintf(lastWorkflowToken_, sizeof(lastWorkflowToken_), "%s", r.pr);

        // Even the deterministic UI demo now enters the same production
        // identity/topology contract that a later workplace HTTP adapter will
        // fill. This prevents a separate, weaker PR -> READY shortcut from
        // surviving in the application path. ProductionWorkflowBridge then
        // reaches TestWorkflowController::acceptNetworkResolvedProfileForSource
        // (the source-preserving successor of acceptNetworkResolvedProfile).
        ProductionProfileRecord record{};
        snprintf(record.productionPr, sizeof(record.productionPr), "%s", r.pr);
        snprintf(record.customerReference, sizeof(record.customerReference), "%s", r.customerReference);
        snprintf(record.revision, sizeof(record.revision), "%s", r.revision);
        snprintf(record.profileId, sizeof(record.profileId), "%s", r.profileId);
        record.signalPinCount = 25U;
        record.includePeA = true;
        record.includePeB = true;
        record.includeDrainShieldA = true;
        record.includeDrainShieldB = true;
        record.connectorKindA = ConnectorKind::Custom;
        record.connectorKindB = ConnectorKind::Custom;
        record.connectorGenderA = ConnectorGender::Neutral;
        record.connectorGenderB = ConnectorGender::Neutral;
        record.connectorContactsA = 25U;
        record.connectorContactsB = 25U;
        snprintf(record.connectorAName, sizeof(record.connectorAName), "X1");
        snprintf(record.connectorBName, sizeof(record.connectorBName), "X2");
        record.aToB[kPeTestIndex] = 1ULL << kPeTestIndex;
        for (uint8_t pin = 1U; pin <= record.signalPinCount; ++pin) {
            record.aToB[pin] = 1ULL << pin;
        }
        record.aToB[kDrainShieldTestIndex] = 1ULL << kDrainShieldTestIndex;

        const ProductionBridgeResult bridge =
            ProductionWorkflowBridge::apply(*workflowController_, record, r.pr, nullptr);
        Serial.printf("[WORKFLOW] PR=%s profile=%s %s autoStart=%u\n",
                      r.pr, r.profileId, bridge.message,
                      bridge.autoStartRequested ? 1U : 0U);
        Serial.flush();
    }
}

void BackboneDemoScreen::buildAnalysisReport() {
    resetAnalysisReportData(analysisReport_);
    analysisReport_.module = backbone_.selectedModule();
    analysisReport_.demoMode = backbone_.snapshot().usingDemo;
    const bool turkish = tr();
    const char* workflow = backbone_.workflowTitle();
    reportCopy(analysisReport_.title, sizeof(analysisReport_.title),
               (workflow != nullptr && workflow[0] != '\0')
                   ? workflow
                   : FeatureBackbone::moduleTitle(backbone_.selectedModule(), turkish));

    auto setColumns = [&](const char* c0, const char* c1, const char* c2,
                          const char* c3, const char* c4) {
        reportCopy(analysisReport_.columns[0], sizeof(analysisReport_.columns[0]), c0);
        reportCopy(analysisReport_.columns[1], sizeof(analysisReport_.columns[1]), c1);
        reportCopy(analysisReport_.columns[2], sizeof(analysisReport_.columns[2]), c2);
        reportCopy(analysisReport_.columns[3], sizeof(analysisReport_.columns[3]), c3);
        reportCopy(analysisReport_.columns[4], sizeof(analysisReport_.columns[4]), c4);
    };
    auto addRow = [&](AnalysisReportStatus status, const char* c0, const char* c1,
                      const char* c2, const char* c3, const char* c4) {
        if (analysisReport_.rowCount >= AnalysisReportData::kMaxRows) return;
        auto& row = analysisReport_.rows[analysisReport_.rowCount++];
        row.status = status;
        reportCopy(row.cells[0], sizeof(row.cells[0]), c0);
        reportCopy(row.cells[1], sizeof(row.cells[1]), c1);
        reportCopy(row.cells[2], sizeof(row.cells[2]), c2);
        reportCopy(row.cells[3], sizeof(row.cells[3]), c3);
        reportCopy(row.cells[4], sizeof(row.cells[4]), c4);
    };

    switch (backbone_.selectedModule()) {
        case BackboneModuleId::Scan128: {
            setColumns(turkish ? "KAYNAK" : "SOURCE",
                       turkish ? "BEKLENEN" : "EXPECTED",
                       turkish ? "BULUNAN" : "MEASURED",
                       "R (mOhm)", turkish ? "SONUÇ" : "RESULT");
            const auto& runner = digitalCore_.scan();
            const auto& stats = runner.stats();
            for (size_t i = 0U; i < runner.reportEntryCount(); ++i) {
                const auto& entry = runner.reportEntry(i);
                const char sourceSide = entry.direction == ScanDirection::AtoB ? 'A' : 'B';
                const char receiverSide = entry.direction == ScanDirection::AtoB ? 'B' : 'A';
                char source[16]{}, expected[64]{}, measured[64]{}, resistance[32]{};
                formatReportNode(sourceSide, entry.source, source, sizeof(source));
                formatReportMask(entry.expectedReceiverMask, receiverSide, expected, sizeof(expected));
                formatReportMask(entry.measurement.actualReceiverMask, receiverSide, measured, sizeof(measured));
                if (entry.measurement.resistanceValid &&
                    (entry.measurement.result == ElectricalResult::Ok ||
                     entry.measurement.result == ElectricalResult::HighResistance)) {
                    snprintf(resistance, sizeof(resistance), "%.2f",
                             static_cast<double>(entry.measurement.resistanceMilliOhm));
                } else {
                    reportCopy(resistance, sizeof(resistance), "N/A");
                }
                addRow(reportStatus(entry.measurement.result), source, expected, measured, resistance,
                       reportElectricalText(entry.measurement.result, turkish));
            }
            snprintf(analysisReport_.summary, sizeof(analysisReport_.summary),
                     turkish ? "Toplam %u | OK %u | OPEN %u | SHORT %u | WRONG %u | HIGH-R %u"
                             : "Total %u | OK %u | OPEN %u | SHORT %u | WRONG %u | HIGH-R %u",
                     static_cast<unsigned>(stats.total), static_cast<unsigned>(stats.ok),
                     static_cast<unsigned>(stats.open), static_cast<unsigned>(stats.shortCircuit),
                     static_cast<unsigned>(stats.wrongConnection), static_cast<unsigned>(stats.highResistance));
            break;
        }

        case BackboneModuleId::Kelvin: {
            setColumns(turkish ? "HAT / ÖLÇÜM" : "LINE / MEASURE",
                       "R (mOhm)", turkish ? "REFERANS" : "REFERENCE",
                       "Delta-R", turkish ? "SONUÇ" : "RESULT");
            const auto& r = qualityCore_.kelvin().result();
            char raw[32]{}, master[32]{}, delta[32]{};
            if (r.valid) {
                snprintf(raw, sizeof(raw), "%.2f", static_cast<double>(r.resistanceMilliOhm));
                snprintf(master, sizeof(master), "%.2f", static_cast<double>(r.masterMilliOhm));
                snprintf(delta, sizeof(delta), "%+.2f", static_cast<double>(r.deltaMilliOhm));
                const char* verdict = r.verdict == QualityVerdict::Pass ? "OK"
                    : r.verdict == QualityVerdict::Warning ? (turkish ? "YÜKSEK-R UYARI" : "HIGH-R WARN")
                    : r.verdict == QualityVerdict::Fail ? (turkish ? "YÜKSEK DİRENÇ" : "HIGH RESISTANCE")
                    : qualityVerdictText(r.verdict);
                addRow(reportStatus(r.verdict, true), turkish ? "SEÇİLİ HAT" : "SELECTED LINE",
                       raw, master, delta, verdict);
            } else {
                addRow(AnalysisReportStatus::NotMeasured,
                       turkish ? "SEÇİLİ HAT" : "SELECTED LINE", "N/A", "N/A", "N/A", "N/A");
            }
            snprintf(analysisReport_.summary, sizeof(analysisReport_.summary),
                     turkish ? "Kelvin: %u mA | ortam %.1f C | ZERO=%s | gerçek donanımda her hat ayrı satır olacaktır"
                             : "Kelvin: %u mA | ambient %.1f C | ZERO=%s | real hardware will create one row per line",
                     static_cast<unsigned>(r.valid ? r.currentMa : qualityCore_.kelvin().currentMa()),
                     static_cast<double>(r.valid ? r.ambientC : qualityCore_.environment().sample().temperatureC),
                     qualityCore_.kelvin().zeroValid() ? "VALID" : "REQUIRED");
            break;
        }

        case BackboneModuleId::SelfTestCalibration: {
            setColumns(turkish ? "KONTROL" : "CHECK",
                       turkish ? "DURUM" : "STATE",
                       turkish ? "DEĞER" : "VALUE",
                       turkish ? "REFERANS" : "REFERENCE",
                       turkish ? "SONUÇ" : "RESULT");
            const auto& engine = digitalCore_.selfTest();
            for (size_t i = 0U; i < engine.itemCount(); ++i) {
                const auto& item = engine.item(i);
                const char* result = selfTestResultText(item.result, turkish);
                addRow(selfTestStatus(item.result), item.name != nullptr ? item.name : "-",
                       result, "-", "-", result);
            }
            const char* mode = engine.mode() == SelfTestMode::Fast ? "FAST"
                             : engine.mode() == SelfTestMode::Full ? "FULL" : "CALIBRATION";
            snprintf(analysisReport_.summary, sizeof(analysisReport_.summary),
                     turkish ? "%s | PASS %u | UYARI %u | FAIL %u"
                             : "%s | PASS %u | WARNING %u | FAIL %u",
                     mode, static_cast<unsigned>(engine.passCount()),
                     static_cast<unsigned>(engine.warningCount()),
                     static_cast<unsigned>(engine.failCount()));
            break;
        }

        case BackboneModuleId::Tdr: {
            setColumns(turkish ? "HAT" : "LINE",
                       turkish ? "ARIZA" : "FAULT",
                       turkish ? "MESAFE" : "DISTANCE",
                       turkish ? "GÜVEN / CAL" : "CONF / CAL",
                       turkish ? "SONUÇ" : "RESULT");
            const auto& r = highSpeedCore_.tdr().result();
            char line[16]{}, distance[32]{}, confidence[40]{};
            snprintf(line, sizeof(line), "A%u", static_cast<unsigned>(r.valid ? r.aNode : highSpeedCore_.tdr().node()));
            const char* fault = r.fault == TdrFaultType::Open ? (turkish ? "AÇIK" : "OPEN")
                              : r.fault == TdrFaultType::Short ? (turkish ? "KISA" : "SHORT")
                              : (turkish ? "BELİRSİZ" : "UNKNOWN");
            if (r.valid) {
                snprintf(distance, sizeof(distance), "%.2f m", static_cast<double>(r.distanceM));
                snprintf(confidence, sizeof(confidence), "%u%% / %s",
                         static_cast<unsigned>(r.confidencePercent), r.calibrationValid ? "VALID" : "REQUIRED");
                AnalysisReportStatus status = reportStatus(r.verdict);
                if (!r.calibrationValid && status == AnalysisReportStatus::Pass) status = AnalysisReportStatus::Warning;
                addRow(status, line, fault, distance, confidence, highSpeedVerdictText(r.verdict));
            } else {
                addRow(AnalysisReportStatus::NotMeasured, line, "N/A", "N/A", "N/A", "N/A");
            }
            snprintf(analysisReport_.summary, sizeof(analysisReport_.summary),
                     turkish ? "TDR A-tarafı referansı | VOP %.2f | yaklaşık arıza mesafesi"
                             : "TDR A-side reference | VOP %.2f | approximate fault distance",
                     static_cast<double>(r.vop));
            break;
        }

        case BackboneModuleId::PairIntegrity: {
            setColumns(turkish ? "ÇİFT / FREKANS" : "PAIR / FREQ",
                       turkish ? "GENLİK" : "AMPLITUDE",
                       turkish ? "FAZ" : "PHASE",
                       "CROSSTALK", turkish ? "SONUÇ" : "RESULT");
            const auto& r = highSpeedCore_.pair().result();
            char pair[32]{}, amp[24]{}, phase[24]{}, xtalk[28]{};
            snprintf(pair, sizeof(pair), "P%u / %lu kHz",
                     static_cast<unsigned>(r.valid ? r.pairIndex : highSpeedCore_.pair().pair()),
                     static_cast<unsigned long>((r.valid ? r.frequencyHz : highSpeedCore_.pair().frequencyHz()) / 1000U));
            if (r.valid) {
                snprintf(amp, sizeof(amp), "%.3f", static_cast<double>(r.amplitudeRatio));
                snprintf(phase, sizeof(phase), "%+.1f deg", static_cast<double>(r.phaseDegrees));
                snprintf(xtalk, sizeof(xtalk), "%.1f dB", static_cast<double>(r.crosstalkDb));
                AnalysisReportStatus status = r.splitPair ? AnalysisReportStatus::WrongConnection : reportStatus(r.verdict);
                const char* verdict = r.splitPair ? (turkish ? "SPLIT-PAIR / YANLIŞ" : "SPLIT-PAIR / WRONG")
                                                  : highSpeedVerdictText(r.verdict);
                addRow(status, pair, amp, phase, xtalk, verdict);
            } else {
                addRow(AnalysisReportStatus::NotMeasured, pair, "N/A", "N/A", "N/A", "N/A");
            }
            snprintf(analysisReport_.summary, sizeof(analysisReport_.summary),
                     turkish ? "Master imzasına göre amplitude / phase / crosstalk; CAT sertifikasyonu değildir"
                             : "Amplitude / phase / crosstalk vs master signature; not CAT certification");
            break;
        }

        case BackboneModuleId::UsbC: {
            setColumns(turkish ? "KONTROL" : "CHECK",
                       turkish ? "DEGER" : "VALUE",
                       turkish ? "PROFIL / BEKLENTI" : "PROFILE / EXPECTED",
                       turkish ? "DURUM" : "STATE",
                       turkish ? "SONUC" : "RESULT");
            const auto& r = highSpeedCore_.usbC().result();
            if (!r.valid) {
                addRow(AnalysisReportStatus::NotMeasured, "USB-C", "N/A", "-", "-", "N/A");
            } else {
                const char* orientation = r.orientation == UsbCOrientation::Cc1 ? "CC1" :
                                          r.orientation == UsbCOrientation::Cc2 ? "CC2" : "UNKNOWN";
                char current[24]{}, ids[40]{};
                snprintf(current, sizeof(current), "%u A", static_cast<unsigned>(r.declaredCurrentA));
                snprintf(ids, sizeof(ids), "VID %04X / PID %04X",
                         static_cast<unsigned>(r.vendorId), static_cast<unsigned>(r.productId));
                addRow(r.attached ? AnalysisReportStatus::Pass : AnalysisReportStatus::Open,
                       turkish ? "BAGLANTI / YON" : "ATTACH / ORIENTATION",
                       orientation, turkish ? "KABLO TAKILI" : "CABLE ATTACHED",
                       r.attached ? "OK" : "OPEN", r.attached ? "OK" : "OPEN");
                addRow(r.emarkerPresent ? AnalysisReportStatus::Pass : AnalysisReportStatus::Warning,
                       "E-MARKER", r.emarkerPresent ? "YES" : "NO", ids,
                       current, r.emarkerPresent ? "OK" : (turkish ? "UYARI" : "WARNING"));
                addRow(AnalysisReportStatus::Info, "EPR / VCONN",
                       r.eprCapable ? "EPR YES" : "EPR NO",
                       r.vconnRequired ? "VCONN REQUIRED" : "VCONN N/A",
                       r.passiveCable ? "PASSIVE" : "ACTIVE", "INFO");
                if (r.profileCompared) {
                    addRow(r.profileMatch ? AnalysisReportStatus::Pass : AnalysisReportStatus::WrongConnection,
                           turkish ? "PROFIL KARSILASTIRMA" : "PROFILE COMPARE",
                           r.profileMatch ? "MATCH" : "MISMATCH",
                           turkish ? "KIMLIK BEKLENTISI" : "IDENTITY EXPECTATION",
                           highSpeedVerdictText(r.verdict),
                           r.profileMatch ? "OK" : (turkish ? "YANLIS" : "WRONG"));
                } else {
                    addRow(AnalysisReportStatus::Info,
                           turkish ? "PROFIL KARSILASTIRMA" : "PROFILE COMPARE",
                           turkish ? "BEKLIYOR" : "PENDING", "-", "-", "INFO");
                }
            }
            reportCopy(analysisReport_.summary, sizeof(analysisReport_.summary),
                       turkish ? "Dusuk guclu USB-C Cable Identity / E-Marker sonucu; 5A/EPR yuk testi ve USB4 SI sertifikasi degildir"
                               : "Low-power USB-C Cable Identity / E-Marker result; not a 5A/EPR load test or USB4 SI certification");
            break;
        }

        case BackboneModuleId::PeDs: {
            setColumns(turkish ? "HAT / KONTROL" : "LINE / CHECK",
                       turkish ? "BAGLANTI" : "CONTINUITY",
                       "R (mOhm)", turkish ? "BEKLENTI" : "EXPECTED",
                       turkish ? "SONUC" : "RESULT");
            const auto& engine = qualityCore_.peDs();
            const auto& r = engine.result();
            char mode[24]{}, policy[24]{}, peR[24]{}, dsR[24]{}, bondR[24]{};
            engine.formatMode(mode, sizeof(mode));
            engine.formatBondPolicy(policy, sizeof(policy));
            if (!r.valid) {
                addRow(AnalysisReportStatus::NotMeasured, "PE", "N/A", "N/A", mode, "N/A");
            } else {
                snprintf(peR, sizeof(peR), "%.2f", static_cast<double>(r.peMilliOhm));
                addRow(r.peContinuity ? AnalysisReportStatus::Pass : AnalysisReportStatus::Open,
                       "PE", r.peContinuity ? "CONNECTED" : "OPEN", peR, mode,
                       r.peContinuity ? "OK" : "OPEN");
                const bool singleEnded = r.mode == PeDsProfileMode::AOnly || r.mode == PeDsProfileMode::BOnly;
                if (singleEnded) {
                    addRow(AnalysisReportStatus::Info, "dS",
                           r.dsAConnected ? "A ONLY" : "B ONLY", "N/A", mode, "N/A");
                } else {
                    snprintf(dsR, sizeof(dsR), "%.2f", static_cast<double>(r.dsMilliOhm));
                    addRow((r.dsAConnected && r.dsBConnected) ? AnalysisReportStatus::Pass : AnalysisReportStatus::Open,
                           "dS", (r.dsAConnected && r.dsBConnected) ? "CONNECTED" : "OPEN",
                           dsR, mode, (r.dsAConnected && r.dsBConnected) ? "OK" : "OPEN");
                }
                snprintf(bondR, sizeof(bondR), "%.2f", static_cast<double>(r.bondMilliOhm));
                bool bondOk = true;
                if (r.bondPolicy == BondPolicy::Required) bondOk = r.bondDetected;
                if (r.bondPolicy == BondPolicy::Forbidden) bondOk = !r.bondDetected;
                addRow(bondOk ? AnalysisReportStatus::Pass : AnalysisReportStatus::WrongConnection,
                       "PE-dS BOND", r.bondDetected ? "DETECTED" : "NONE", bondR, policy,
                       bondOk ? "OK" : (turkish ? "YANLIS" : "WRONG"));
            }
            snprintf(analysisReport_.summary, sizeof(analysisReport_.summary),
                     "%s | %s | %s", mode, policy, qualityVerdictText(r.verdict));
            break;
        }

        case BackboneModuleId::FixtureSpc: {
            setColumns(turkish ? "METRIK" : "METRIC",
                       turkish ? "DEGER" : "VALUE",
                       turkish ? "REFERANS" : "REFERENCE",
                       turkish ? "DURUM" : "STATE",
                       turkish ? "SONUC" : "RESULT");
            const auto& engine = qualityCore_.fixtureSpc();
            const auto& q = engine.snapshot();
            char value[32]{}, reference[40]{}, state[32]{};
            if (!q.valid) {
                addRow(AnalysisReportStatus::NotMeasured, "FIXTURE", "N/A", "-", "-", "N/A");
            } else {
                snprintf(value, sizeof(value), "%.0f%%", static_cast<double>(q.fixtureHealthPercent));
                addRow(q.fixtureHealthPercent >= 90.0f ? AnalysisReportStatus::Pass : AnalysisReportStatus::Warning,
                       "FIXTURE HEALTH", value, "MASTER", q.productPass ? "PASS" : "FAIL",
                       q.productPass ? "OK" : "FAIL");
                snprintf(value, sizeof(value), "%+.2f mOhm", static_cast<double>(q.selectedPinDriftMilliOhm));
                snprintf(reference, sizeof(reference), "%s / rev %lu", q.selectedPin,
                         static_cast<unsigned long>(engine.baselineRevision()));
                addRow(q.repeatedAnomalyCount > 0U ? AnalysisReportStatus::Warning : AnalysisReportStatus::Pass,
                       turkish ? "SECILI PIN DRIFT" : "SELECTED PIN DRIFT", value, reference,
                       q.repeatedAnomalyCount > 0U ? (turkish ? "TEKRARLI" : "REPEATED") : "OK",
                       q.repeatedAnomalyCount > 0U ? (turkish ? "UYARI" : "WARNING") : "OK");
                snprintf(value, sizeof(value), "%.2f mOhm", static_cast<double>(q.processMeanMilliOhm));
                snprintf(reference, sizeof(reference), "sigma %.2f", static_cast<double>(q.processStdDevMilliOhm));
                snprintf(state, sizeof(state), "Cpk %.2f", static_cast<double>(q.cpk));
                addRow(q.processWarning ? AnalysisReportStatus::Warning : AnalysisReportStatus::Pass,
                       "PROCESS", value, reference, state,
                       q.processWarning ? (turkish ? "UYARI" : "WARNING") : "OK");
            }
            snprintf(analysisReport_.summary, sizeof(analysisReport_.summary),
                     turkish ? "Fixture/master baseline rev %lu | production DUT baseline'i otomatik degistirmez"
                             : "Fixture/master baseline rev %lu | production DUT never auto-updates baseline",
                     static_cast<unsigned long>(engine.baselineRevision()));
            break;
        }

        case BackboneModuleId::SmartProbe: {
            analysisReport_.liveSnapshot = true;
            setColumns(turkish ? "SECILI UC" : "SELECTED END",
                       turkish ? "BULUNAN NET" : "FOUND NET",
                       turkish ? "TARAF" : "SIDE",
                       turkish ? "SES" : "TONE",
                       turkish ? "SONUC" : "RESULT");
            char selected[24]{}, found[64]{};
            digitalCore_.probe().formatSelected(selected, sizeof(selected));
            digitalCore_.probe().formatFoundNet(found, sizeof(found));
            addRow(digitalCore_.probe().foundNet() ? AnalysisReportStatus::Pass : AnalysisReportStatus::NotMeasured,
                   selected, digitalCore_.probe().foundNet() ? found : "N/A",
                   digitalCore_.probe().selectedSide() == CableMapSide::A ? "A" : "B",
                   probeTone_ ? "ON" : "OFF",
                   digitalCore_.probe().foundNet() ? (turkish ? "BULUNDU" : "FOUND") : "N/A");
            reportCopy(analysisReport_.summary, sizeof(analysisReport_.summary),
                       turkish ? "Smart Probe anlik connected-component sonucu; elektriksel PASS/FAIL kablo testi degildir"
                               : "Smart Probe connected-component snapshot; not a cable PASS/FAIL test");
            break;
        }

        case BackboneModuleId::EnvironmentAwg: {
            analysisReport_.liveSnapshot = qualityCore_.environment().live();
            setColumns(turkish ? "OLCUM" : "MEASUREMENT",
                       turkish ? "DEGER" : "VALUE",
                       turkish ? "WIRE SPEC" : "WIRE SPEC",
                       turkish ? "KAYIT" : "LOG",
                       turkish ? "SONUC" : "RESULT");
            const auto& e = qualityCore_.environment().sample();
            char value[32]{}, wire[64]{}, log[32]{};
            qualityCore_.environment().formatWireSpec(wire, sizeof(wire));
            snprintf(log, sizeof(log), "#%lu", static_cast<unsigned long>(qualityCore_.environment().logCount()));
            if (e.valid) {
                snprintf(value, sizeof(value), "%.2f C", static_cast<double>(e.temperatureC));
                addRow(AnalysisReportStatus::Info, "TEMPERATURE", value, wire, log, "INFO");
                snprintf(value, sizeof(value), "%.1f %%RH", static_cast<double>(e.humidityRh));
                addRow(AnalysisReportStatus::Info, "HUMIDITY", value, wire, log, "INFO");
                const float theoretical = qualityCore_.environment().theoreticalWireMilliOhm();
                if (theoretical >= 0.0f) {
                    snprintf(value, sizeof(value), "%.1f mOhm", static_cast<double>(theoretical));
                    addRow(AnalysisReportStatus::Info, "RHO L/A", value, wire,
                           turkish ? "YAKLASIK" : "PLAUSIBILITY", "INFO");
                }
            } else {
                addRow(AnalysisReportStatus::NotMeasured, "SHT40", "N/A", wire, log, "N/A");
            }
            reportCopy(analysisReport_.summary, sizeof(analysisReport_.summary),
                       turkish ? "Ortam/AWG yardimci sonucu; R_RAW degerinin yerine gecmez"
                               : "Environment/AWG helper result; never replaces R_RAW");
            break;
        }

        case BackboneModuleId::ComponentTest: {
            setColumns(turkish ? "KOMPONENT" : "COMPONENT",
                       turkish ? "VARLIK / POLARITE" : "PRESENCE / POLARITY",
                       turkish ? "OLCULEN" : "MEASURED",
                       turkish ? "KAPSAM" : "SCOPE",
                       turkish ? "SONUC" : "RESULT");
            const auto& r = systemCore_.component().result();
            const char* type = r.type == BasicComponentType::Diode ? "DIODE" :
                               r.type == BasicComponentType::Led ? "LED" : "CAPACITOR";
            char measured[32]{};
            if (!r.valid) {
                addRow(AnalysisReportStatus::NotMeasured, type, "N/A", "N/A", "BASIC", "N/A");
            } else if (r.type == BasicComponentType::Capacitor) {
                snprintf(measured, sizeof(measured), "%.1f uF", static_cast<double>(r.roughCapacitanceUf));
                addRow(reportStatus(r.verdict), type, r.present ? "PRESENT" : "MISSING",
                       measured, "ROUGH C", systemVerdictText(r.verdict));
            } else {
                snprintf(measured, sizeof(measured), "Vf %.2f V", static_cast<double>(r.forwardVoltageV));
                const char* polarity = !r.polarityKnown ? "UNKNOWN" : r.forwardPolarity ? "FORWARD" : "REVERSE";
                addRow(r.shorted ? AnalysisReportStatus::ShortCircuit : reportStatus(r.verdict),
                       type, polarity, measured, "BASIC", r.shorted ? "SHORT" : systemVerdictText(r.verdict));
            }
            reportCopy(analysisReport_.summary, sizeof(analysisReport_.summary),
                       turkish ? "Temel komponent testi; precision LCR/ESR/leakage/LED parlaklik karakterizasyonu degildir"
                               : "Basic component test; not precision LCR/ESR/leakage/LED brightness characterization");
            break;
        }

        case BackboneModuleId::HardwareValidation: {
            setColumns(turkish ? "KONTROL ALANI" : "DOMAIN",
                       turkish ? "GECEN" : "PASSED",
                       turkish ? "GERCEK DONANIM" : "REAL HARDWARE",
                       "PIN FREEZE", turkish ? "SONUC" : "RESULT");
            const auto& r = systemCore_.hardwareValidation().result();
            const char* domain = r.domain == HardwareValidationDomain::Gpio ? "GPIO" :
                                 r.domain == HardwareValidationDomain::Bus ? "BUS" : "BOOT";
            char checks[24]{}, freeze[32]{}, real[16]{};
            snprintf(checks, sizeof(checks), "%u/%u", static_cast<unsigned>(r.checksPassed),
                     static_cast<unsigned>(r.checksTotal));
            reportCopy(real, sizeof(real), r.measuredOnRealHardware ? "YES" : "NO");
            reportCopy(freeze, sizeof(freeze), r.freezeEligible ? "ELIGIBLE" : "BLOCKED");
            addRow(r.valid ? reportStatus(r.verdict) : AnalysisReportStatus::NotMeasured,
                   domain, r.valid ? checks : "N/A", real, freeze,
                   r.valid ? systemVerdictText(r.verdict) : "N/A");
            addRow(r.gpio5Reserved ? AnalysisReportStatus::Pass : AnalysisReportStatus::Fail,
                   "GPIO5", "LCD_RESET", "RESERVED", "DO NOT USE",
                   r.gpio5Reserved ? "OK" : "FAIL");
            if (r.domain == HardwareValidationDomain::Bus && r.valid) {
                char spi[28]{};
                snprintf(spi, sizeof(spi), "%lu MHz", static_cast<unsigned long>(r.selectedSpiHz / 1000000U));
                addRow(AnalysisReportStatus::Info, "MCP23S17 SPI", spi, "DEMO", "VERIFY PCB", "INFO");
            }
            reportCopy(analysisReport_.summary, sizeof(analysisReport_.summary),
                       turkish ? "Demo sonucu Gerber/Pin Freeze serbest birakmaz; gercek JC1060+carrier olcumu zorunlu"
                               : "Demo result never releases Gerber/Pin Freeze; real JC1060+carrier validation is mandatory");
            break;
        }

        case BackboneModuleId::MultiConnector: {
            setColumns(turkish ? "SEGMENT" : "SEGMENT",
                       turkish ? "OLCUM" : "MEASURED",
                       "OK / ERROR",
                       turkish ? "PIN SAYISI" : "PIN COUNT",
                       turkish ? "SONUC" : "RESULT");
            for (uint8_t i = 0U; i < 3U; ++i) {
                const auto segment = static_cast<MultiSegmentId>(i);
                const auto& r = digitalCore_.multi().result(segment);
                char measured[28]{}, okError[32]{}, pins[20]{};
                snprintf(pins, sizeof(pins), "%u", static_cast<unsigned>(digitalCore_.multi().segmentSignalCount(segment)));
                if (r.state == DigitalWorkflowState::Idle) {
                    addRow(AnalysisReportStatus::NotMeasured,
                           digitalCore_.multi().segmentName(segment), "N/A", "N/A", pins, "N/A");
                    continue;
                }
                snprintf(measured, sizeof(measured), "%u/%u", static_cast<unsigned>(r.stats.completed),
                         static_cast<unsigned>(r.stats.total));
                snprintf(okError, sizeof(okError), "%u / %u", static_cast<unsigned>(r.stats.ok),
                         static_cast<unsigned>(r.stats.errorCount()));
                const AnalysisReportStatus status = r.stats.errorCount() == 0U ? AnalysisReportStatus::Pass : AnalysisReportStatus::Fail;
                addRow(status, digitalCore_.multi().segmentName(segment), measured, okError, pins,
                       status == AnalysisReportStatus::Pass ? "PASS" : "FAIL");
            }
            reportCopy(analysisReport_.summary, sizeof(analysisReport_.summary),
                       turkish ? "Yalniz komsu A-B / B-C / C-D segmentleri; bypass A-C/A-D test edilmez"
                               : "Adjacent A-B / B-C / C-D segments only; bypass A-C/A-D is never tested");
            break;
        }

        case BackboneModuleId::Voice: {
            analysisReport_.liveSnapshot = true;
            setColumns(turkish ? "KONTROL" : "CHECK",
                       turkish ? "DEGER" : "VALUE",
                       turkish ? "KOMUT" : "COMMAND",
                       turkish ? "GUVENLIK" : "SAFETY",
                       turkish ? "SONUC" : "RESULT");
            const auto& r = systemCore_.voice().result();
            char mic[24]{};
            snprintf(mic, sizeof(mic), "%u%%", static_cast<unsigned>(r.micLevelPercent));
            addRow(r.micHealthy ? AnalysisReportStatus::Pass : AnalysisReportStatus::Warning,
                   "MIC / CODEC", mic, r.recognizedText[0] != '\0' ? r.recognizedText : "-",
                   r.criticalActionBlocked ? "BLOCKED" : "SAFE",
                   r.micHealthy ? "OK" : (turkish ? "UYARI" : "WARNING"));
            addRow(r.dispatchAllowed ? AnalysisReportStatus::Pass : AnalysisReportStatus::Info,
                   "HI MG", r.wakeWordEnabled ? "ON" : "OFF",
                   r.commandRecognized ? "RECOGNIZED" : "-",
                   r.dispatchAllowed ? "ALLOWED" : "NO DISPATCH", "INFO");
            reportCopy(analysisReport_.summary, sizeof(analysisReport_.summary),
                       turkish ? "Ses/komut servis testi; kritik kalibrasyon veya silme islemleri sesle calistirilmaz"
                               : "Voice/command service test; critical calibration/delete actions are never voice-dispatched");
            break;
        }

        case BackboneModuleId::FlexGlitch: {
            analysisReport_.liveSnapshot = true;
            setColumns(turkish ? "HAT" : "LINE",
                       turkish ? "OLAY" : "EVENT",
                       turkish ? "SÜRE" : "DURATION",
                       turkish ? "ZAMAN" : "TIME",
                       turkish ? "SONUÇ" : "RESULT");
            const auto& g = highSpeedCore_.flexGlitch().snapshot();
            if (g.lastEvent.valid) {
                char eventNo[24]{}, duration[24]{}, time[28]{};
                snprintf(eventNo, sizeof(eventNo), "#%lu", static_cast<unsigned long>(g.lastEvent.eventNumber));
                snprintf(duration, sizeof(duration), "%lu us", static_cast<unsigned long>(g.lastEvent.durationUs));
                snprintf(time, sizeof(time), "%lu ms", static_cast<unsigned long>(g.lastEvent.timestampMs));
                addRow(AnalysisReportStatus::Fail, g.lastEvent.netName, eventNo, duration, time,
                       turkish ? "TEMASSIZLIK" : "DROPOUT");
            } else {
                addRow(AnalysisReportStatus::Pass, "A17-B17", "0", "-", "-",
                       turkish ? "OLAY YOK" : "NO EVENT");
            }
            snprintf(analysisReport_.summary, sizeof(analysisReport_.summary),
                     turkish ? "Canlı olay özeti | toplam %lu | en kısa %lu us | latch %s"
                             : "Live event snapshot | total %lu | shortest %lu us | latch %s",
                     static_cast<unsigned long>(g.eventCount),
                     static_cast<unsigned long>(g.shortestDurationUs), g.latched ? "SET" : "CLEAR");
            break;
        }

        default:
            setColumns(turkish ? "ÖLÇÜM" : "MEASUREMENT", "-", "-", "-", turkish ? "SONUÇ" : "RESULT");
            addRow(AnalysisReportStatus::Info,
                   FeatureBackbone::moduleTitle(backbone_.selectedModule(), turkish), "-", "-", "-",
                   turkish ? "RAPOR YOK" : "NO REPORT");
            reportCopy(analysisReport_.summary, sizeof(analysisReport_.summary),
                       turkish ? "Bu modül için tablo raporu tanımlı değil." : "No table report is defined for this module.");
            break;
    }
}

void BackboneDemoScreen::refreshButtons(const BackboneSnapshot& snap) {
    for (uint8_t i = 0; i < kButtonCount; ++i) {
        if (buttonLabels_[i] != nullptr) lv_label_set_text(buttonLabels_[i], buttonText(i, snap));
    }
}

void BackboneDemoScreen::refresh(bool force) {
    BackboneSnapshot snap = backbone_.snapshot();
    applyDigitalCoreSnapshot(snap);
    applyQualityCoreSnapshot(snap);
    applyHighSpeedCoreSnapshot(snap);
    applySystemCoreSnapshot(snap);
    if (backbone_.selectedModule() == BackboneModuleId::CableLearn &&
        snap.state == BackboneRunState::Complete &&
        (!haveSnapshot_ || lastSnapshot_.state != BackboneRunState::Complete)) {
        cableLearnCompletionPending_ = true;
    }

    const BackboneModuleId reportModule = backbone_.selectedModule();
    const bool finiteAnalysisModule =
        reportModule == BackboneModuleId::Scan128 ||
        reportModule == BackboneModuleId::Kelvin ||
        reportModule == BackboneModuleId::SelfTestCalibration ||
        reportModule == BackboneModuleId::Tdr ||
        reportModule == BackboneModuleId::PairIntegrity ||
        reportModule == BackboneModuleId::UsbC ||
        reportModule == BackboneModuleId::PeDs ||
        reportModule == BackboneModuleId::FixtureSpc ||
        reportModule == BackboneModuleId::ComponentTest ||
        reportModule == BackboneModuleId::HardwareValidation;
    if (finiteAnalysisModule && isTerminal(snap.state) &&
        (!haveSnapshot_ || !isTerminal(lastSnapshot_.state))) {
        // Build before the application shell changes screens so the measured
        // core data and natural row order remain frozen for operator review.
        buildAnalysisReport();
        analysisReportPending_ = true;
    }
    const bool changed = force || !haveSnapshot_ ||
        snap.state != lastSnapshot_.state ||
        snap.phaseIndex != lastSnapshot_.phaseIndex ||
        snap.progressPercent != lastSnapshot_.progressPercent ||
        strcmp(snap.metric1, lastSnapshot_.metric1) != 0 ||
        strcmp(snap.metric2, lastSnapshot_.metric2) != 0 ||
        strcmp(snap.metric3, lastSnapshot_.metric3) != 0;
    if (!changed) return;

    const char* workflow = backbone_.workflowTitle();
    lv_label_set_text(titleLabel_, (workflow != nullptr && workflow[0] != '\0')
        ? workflow : FeatureBackbone::moduleTitle(backbone_.selectedModule(), tr()));
    lv_label_set_text(moduleLabel_, FeatureBackbone::moduleTitle(backbone_.selectedModule(), tr()));
    lv_label_set_text(modeLabel_, snap.usingDemo
        ? (tr() ? "DEMO MODE - gerçek PCB çıkışları kapalı" : "DEMO MODE - real PCB outputs disabled")
        : (tr() ? "HARDWARE MODE" : "HARDWARE MODE"));
    lv_label_set_text(stateLabel_, FeatureBackbone::stateText(snap.state, tr()));
    lv_label_set_text(phaseLabel_, FeatureBackbone::phaseText(snap.module, snap.phaseIndex, tr()));
    lv_bar_set_value(progressBar_, snap.progressPercent, LV_ANIM_OFF);

    char progress[20] = {};
    snprintf(progress, sizeof(progress), "%u%%", static_cast<unsigned>(snap.progressPercent));
    lv_label_set_text(progressLabel_, progress);
    lv_label_set_text(metricLabels_[0], snap.metric1);
    lv_label_set_text(metricLabels_[1], snap.metric2);
    lv_label_set_text(metricLabels_[2], snap.metric3);
    refreshButtons(snap);

    lastSnapshot_ = snap;
    haveSnapshot_ = true;
}

bool BackboneDemoScreen::consumeCableLearnCompletion() {
    const bool value = cableLearnCompletionPending_;
    cableLearnCompletionPending_ = false;
    return value;
}

bool BackboneDemoScreen::consumeAnalysisReportRequest() {
    const bool value = analysisReportPending_;
    analysisReportPending_ = false;
    return value;
}

bool BackboneDemoScreen::consumeBackRequest() {
    const bool value = backRequested_;
    backRequested_ = false;
    return value;
}

}  // namespace mg::p4
