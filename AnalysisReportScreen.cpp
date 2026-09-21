#include "AnalysisReportScreen.h"

#include <stdio.h>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

void flat(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

static constexpr lv_coord_t kAnalysisColumnWidths[6] = {42, 170, 180, 180, 180, 224};

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text,
                    const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text != nullptr ? text : "");
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

bool isFailureStatus(AnalysisReportStatus status) {
    return status == AnalysisReportStatus::Open ||
           status == AnalysisReportStatus::ShortCircuit ||
           status == AnalysisReportStatus::WrongConnection ||
           status == AnalysisReportStatus::HighResistance ||
           status == AnalysisReportStatus::Fail;
}

bool isWarningStatus(AnalysisReportStatus status) {
    return status == AnalysisReportStatus::Warning ||
           status == AnalysisReportStatus::NotMeasured;
}

}  // namespace

MgResultStatus AnalysisReportScreen::visualStatus(AnalysisReportStatus status) {
    switch (status) {
        case AnalysisReportStatus::Pass:            return MgResultStatus::Pass;
        case AnalysisReportStatus::Open:            return MgResultStatus::Open;
        case AnalysisReportStatus::ShortCircuit:    return MgResultStatus::ShortCircuit;
        case AnalysisReportStatus::WrongConnection: return MgResultStatus::WrongConnection;
        case AnalysisReportStatus::HighResistance:  return MgResultStatus::HighResistance;
        case AnalysisReportStatus::Warning:         return MgResultStatus::Warning;
        case AnalysisReportStatus::Fail:            return MgResultStatus::Fail;
        case AnalysisReportStatus::NotMeasured:     return MgResultStatus::NotMeasured;
        case AnalysisReportStatus::Info:
        default:                                    return MgResultStatus::Info;
    }
}

void AnalysisReportScreen::begin(const AnalysisReportData& report) {
    report_ = report;
    pendingAction_ = AnalysisReportAction::None;
    buildUi();
    refresh();
}

void AnalysisReportScreen::buildUi() {
    if (screen_ == nullptr) screen_ = lv_obj_create(nullptr); else lv_obj_clean(screen_);
    lv_scr_load(screen_);
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 70);
    flat(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    titleLabel_ = makeLabel(header, "", p4Font20(), lv_color_hex(0xEAF6FF));
    lv_obj_set_pos(titleLabel_, 24, 19);
    lv_obj_set_width(titleLabel_, 720);
    lv_label_set_long_mode(titleLabel_, LV_LABEL_LONG_DOT);

    verdictLabel_ = makeLabel(header, "", p4Font20(), lv_color_hex(0xDDEBF2));
    lv_obj_align(verdictLabel_, LV_ALIGN_TOP_RIGHT, -26, 19);

    modeLabel_ = makeLabel(screen_, "", p4Font14(), lv_color_hex(0xFFD36A));
    lv_obj_set_pos(modeLabel_, 24, 79);
    lv_obj_set_width(modeLabel_, 976);

    summaryLabel_ = makeLabel(screen_, "", p4Font14(), lv_color_hex(0xDDEBF2));
    lv_obj_set_pos(summaryLabel_, 24, 109);
    lv_obj_set_width(summaryLabel_, 976);
    lv_label_set_long_mode(summaryLabel_, LV_LABEL_LONG_DOT);

    resultTable_.create(screen_, 24, 142, 976, 40, 183, 310,
                        kColumnCount, kAnalysisColumnWidths, p4Font14());
    resultTable_.setHeader(0U, "#");
    for (uint16_t i = 0U; i < 5U; ++i) resultTable_.setHeader(i + 1U, report_.columns[i]);

    const lv_coord_t y = 510;
    buttons_[0] = makeButton(screen_, 18, y, 300,
                             p4SelectText("ÖLÇÜME DÖN", "BACK TO MEASUREMENT", "TERUG NAAR METING", "ZUR MESSUNG", "RETOUR À LA MESURE", "VOLVER A LA MEDICIÓN", "WRÓĆ DO POMIARU"),
                             lv_color_hex(0x1B8C63), 0U, AnalysisReportAction::ReturnToMeasurement);
    buttons_[1] = makeButton(screen_, 328, y, 330,
                             p4SelectText("ÖNCEKİ MENÜ", "PREVIOUS MENU", "VORIG MENU", "VORHERIGES MENÜ", "MENU PRÉCÉDENT", "MENÚ ANTERIOR", "POPRZEDNIE MENU"),
                             lv_color_hex(0x166E7A), 1U, AnalysisReportAction::ReturnToOriginMenu);
    buttons_[2] = makeButton(screen_, 668, y, 338,
                             p4SelectText("ANA MENÜ", "MAIN MENU", "HOOFDMENU", "HAUPTMENÜ", "MENU PRINCIPAL", "MENÚ PRINCIPAL", "MENU GŁÓWNE"),
                             lv_color_hex(0x4C5962), 2U, AnalysisReportAction::MainMenu);
}

lv_obj_t* AnalysisReportScreen::makeButton(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                            lv_coord_t w, const char* text, lv_color_t color,
                                            uint8_t bindingIndex, AnalysisReportAction action) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, 72);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    bindings_[bindingIndex] = {this, action};
    lv_obj_add_event_cb(button, buttonCallback, LV_EVENT_CLICKED, &bindings_[bindingIndex]);
    buttonLabels_[bindingIndex] = makeLabel(button, text, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_set_width(buttonLabels_[bindingIndex], w - 12);
    lv_obj_set_style_text_align(buttonLabels_[bindingIndex], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(buttonLabels_[bindingIndex]);
    return button;
}

void AnalysisReportScreen::buttonCallback(lv_event_t* event) {
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->owner == nullptr) return;
    binding->owner->pendingAction_ = binding->action;
}

void AnalysisReportScreen::refreshHeader() {
    char title[100]{};
    snprintf(title, sizeof(title), "%s - %s",
             p4SelectText("SONUÇ RAPORU", "RESULT REPORT", "RESULTAATRAPPORT", "ERGEBNISBERICHT", "RAPPORT DE RÉSULTATS", "INFORME DE RESULTADOS", "RAPORT WYNIKÓW"), report_.title);
    lv_label_set_text(titleLabel_, title);

    bool fail = false;
    bool warning = false;
    for (size_t i = 0U; i < report_.rowCount; ++i) {
        fail = fail || isFailureStatus(report_.rows[i].status);
        warning = warning || isWarningStatus(report_.rows[i].status);
    }
    const char* verdict = fail ? "FAIL" : warning ? p4SelectText("UYARI", "WARNING", "WAARSCHUWING", "WARNUNG", "AVERTISSEMENT", "ADVERTENCIA", "OSTRZEŻENIE") : "PASS";
    lv_label_set_text(verdictLabel_, verdict);
    lv_obj_set_style_text_color(verdictLabel_,
        fail ? lv_color_hex(0xFF7E86) : warning ? lv_color_hex(0xFFD36A) : lv_color_hex(0x77E08A),
        LV_PART_MAIN);

    lv_label_set_text(modeLabel_, report_.demoMode
        ? p4SelectText("DEMO MODU - gerçek PCB ölçüm sürücüsü bağlı değil", "DEMO MODE - real PCB measurement driver not attached", "DEMO-MODUS - echte PCB-meetdriver niet aangesloten", "DEMO-MODUS - realer PCB-Messtreiber nicht angeschlossen", "MODE DÉMO - pilote de mesure PCB réel non connecté", "MODO DEMO - controlador de medida PCB real no conectado", "TRYB DEMO - rzeczywisty sterownik pomiarowy PCB niepodłączony")
        : p4SelectText("DONANIM MODU", "HARDWARE MODE", "HARDWAREMODUS", "HARDWARE-MODUS", "MODE MATÉRIEL", "MODO HARDWARE", "TRYB SPRZĘTOWY"));
    lv_label_set_text(summaryLabel_, report_.summary);
}

void AnalysisReportScreen::refreshTable() {
    resultTable_.setRowCount(static_cast<uint16_t>(report_.rowCount));
    resultTable_.resetStatuses(MgResultStatus::Info);
    for (uint16_t row = 0U; row < report_.rowCount; ++row) {
        char index[12]{};
        snprintf(index, sizeof(index), "%u", static_cast<unsigned>(row + 1U));
        resultTable_.setCell(row, 0U, index);
        for (uint16_t col = 0U; col < 5U; ++col) {
            resultTable_.setCell(row, col + 1U, report_.rows[row].cells[col]);
        }
        resultTable_.setRowStatus(row, visualStatus(report_.rows[row].status));
    }
}

void AnalysisReportScreen::refresh() {
    if (screen_ == nullptr) return;
    refreshHeader();
    refreshTable();
}

void AnalysisReportScreen::activate() {
    pendingAction_ = AnalysisReportAction::None;
    if (screen_ != nullptr && lv_scr_act() != screen_) lv_scr_load(screen_);
    refresh();
}

AnalysisReportAction AnalysisReportScreen::consumeAction() {
    const AnalysisReportAction action = pendingAction_;
    pendingAction_ = AnalysisReportAction::None;
    return action;
}

}  // namespace mg::p4
