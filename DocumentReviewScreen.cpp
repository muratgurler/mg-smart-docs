#include "DocumentReviewScreen.h"

#include <Arduino.h>
#include <stdio.h>

#include "ConfigP4.h"
#include "MapWebEditor.h"
#include "DocumentUiLocalization.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

void flat(lv_obj_t* object) {
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

lv_obj_t* label(lv_obj_t* parent,
                const char* text,
                const lv_font_t* font,
                lv_color_t color) {
    lv_obj_t* item = lv_label_create(parent);
    lv_label_set_text(item, text);
    lv_obj_set_style_text_font(item, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(item, color, LV_PART_MAIN);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_CLICKABLE);
    return item;
}

const char* kindText(DocumentFileKind kind) {
    switch (kind) {
        case DocumentFileKind::Json: return "MG-JSON";
        default: return "?";
    }
}

lv_obj_t* makeBottomButton(lv_obj_t* parent,
                           int x,
                           int width,
                           lv_color_t color,
                           const char* text,
                           void* callbackContext) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, 516);
    lv_obj_set_size(button, width, 64);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(button, DocumentReviewScreen::bottomCallback, LV_EVENT_CLICKED, callbackContext);
    lv_obj_t* textLabel = label(button, text, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_center(textLabel);
    return button;
}

}  // namespace

void DocumentReviewScreen::begin(MapWebEditor& portal) {
    portal_ = &portal;
    action_ = DocumentReviewAction::None;
    if (!screen_) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    lv_scr_load(screen_);
    buildUi();
    rebuildList();
    refreshSummary();
}

void DocumentReviewScreen::activate() {
    action_ = DocumentReviewAction::None;
    if (screen_ && lv_scr_act() != screen_) {
        lv_scr_load(screen_);
    }
    rebuildList();
    refreshSummary();
}

void DocumentReviewScreen::buildUi() {
    flat(screen_);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 80);
    flat(header);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    lv_obj_t* title = label(header,
                            p4SelectText("MOBİL PROFİLİ İNCELE", "REVIEW MOBILE PROFILE", "MOBIEL PROFIEL CONTROLEREN", "MOBILPROFIL PRÜFEN", "VÉRIFIER LE PROFIL MOBILE", "REVISAR PERFIL MÓVIL", "SPRAWDŹ PROFIL MOBILNY"),
                            p4Font20(),
                            lv_color_hex(0xFFFFFF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t* subtitle = label(header,
                               p4SelectText("Dosya adını ve CRC bilgisini kontrol edin, sonra elektriksel doğrulamaya geçin.", "Check the file name and CRC, then continue to electrical validation.", "Controleer bestandsnaam en CRC en ga door naar elektrische validatie.", "Dateiname und CRC prüfen, dann elektrische Validierung starten.", "Vérifiez le nom et le CRC, puis passez à la validation électrique.", "Compruebe el nombre y CRC y continúe con la validación eléctrica.", "Sprawdź nazwę pliku i CRC, a następnie przejdź do walidacji elektrycznej."),
                               p4Font14(),
                               lv_color_hex(0x9CC6DC));
    lv_obj_align(subtitle, LV_ALIGN_BOTTOM_MID, 0, -9);

    lv_obj_t* summaryPanel = lv_obj_create(screen_);
    lv_obj_set_pos(summaryPanel, 24, 96);
    lv_obj_set_size(summaryPanel, 972, 76);
    flat(summaryPanel);
    lv_obj_clear_flag(summaryPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(summaryPanel, lv_color_hex(0x10202A), LV_PART_MAIN);
    lv_obj_set_style_border_width(summaryPanel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(summaryPanel, lv_color_hex(0x355565), LV_PART_MAIN);
    lv_obj_set_style_radius(summaryPanel, 12, LV_PART_MAIN);

    summaryLabel_ = label(summaryPanel, "", p4Font20(), lv_color_hex(0xD7EAF4));
    lv_obj_align(summaryLabel_, LV_ALIGN_LEFT_MID, 18, 0);

    listPanel_ = lv_obj_create(screen_);
    lv_obj_set_pos(listPanel_, 24, 188);
    lv_obj_set_size(listPanel_, 972, 308);
    lv_obj_set_style_bg_color(listPanel_, lv_color_hex(0x0C1921), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(listPanel_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(listPanel_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(listPanel_, lv_color_hex(0x294756), LV_PART_MAIN);
    lv_obj_set_style_radius(listPanel_, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(listPanel_, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(listPanel_, 6, LV_PART_MAIN);
    lv_obj_set_flex_flow(listPanel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(listPanel_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(listPanel_, LV_SCROLLBAR_MODE_AUTO);

    bottomContexts_[0] = {this, DocumentReviewAction::ReturnToUpload};
    bottomContexts_[1] = {this, DocumentReviewAction::PrepareAnalysis};
    bottomContexts_[2] = {this, DocumentReviewAction::Back};
    makeBottomButton(screen_, 24, 220, lv_color_hex(0x176B8F),
                     p4SelectText("AKTARIMA DÖN", "BACK TO IMPORT", "TERUG NAAR IMPORT", "ZUM IMPORT", "RETOUR À L'IMPORT", "VOLVER A IMPORTAR", "WRÓĆ DO IMPORTU"),
                     &bottomContexts_[0]);
    prepareButton_ = makeBottomButton(screen_, 260, 420, lv_color_hex(0x267047),
                                      p4SelectText("ELEKTRİKSEL DOĞRULA", "VALIDATE ELECTRICAL PROFILE", "ELEKTRISCH PROFIEL VALIDEREN", "ELEKTRISCHES PROFIL PRÜFEN", "VALIDER LE PROFIL ÉLECTRIQUE", "VALIDAR PERFIL ELÉCTRICO", "SPRAWDŹ PROFIL ELEKTRYCZNY"),
                                      &bottomContexts_[1]);
    makeBottomButton(screen_, 696, 300, lv_color_hex(0x5E426F),
                     p4Texts().back, &bottomContexts_[2]);
}

void DocumentReviewScreen::rebuildList() {
    if (!listPanel_ || !portal_) return;
    lv_obj_clean(listPanel_);
    const uint8_t count = portal_->documentItemCount();
    if (count == 0U) {
        lv_obj_t* empty = label(listPanel_,
                                p4SelectText("Profil dosyası yok.", "Profile file is missing.", "Profielbestand ontbreekt.", "Profildatei fehlt.", "Fichier profil manquant.", "Falta el archivo de perfil.", "Brak pliku profilu."),
                                p4Font20(), lv_color_hex(0xF1C96A));
        lv_obj_set_width(empty, 920);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        return;
    }

    for (uint8_t index = 0; index < count; ++index) {
        DocumentItemInfo item;
        if (!portal_->documentItem(index, item)) continue;

        lv_obj_t* row = lv_obj_create(listPanel_);
        lv_obj_set_width(row, 930);
        lv_obj_set_height(row, 64);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x132733), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_hex(0x355565), LV_PART_MAIN);
        lv_obj_set_style_radius(row, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);

        char info[180] = {};
        snprintf(info, sizeof(info), "%02u  %s   [%s | %lu KB | CRC %08lX]",
                 static_cast<unsigned>(index + 1U),
                 item.name.c_str(),
                 kindText(item.kind),
                 static_cast<unsigned long>(item.size / 1024U),
                 static_cast<unsigned long>(item.crc32));
        lv_obj_t* itemLabel = label(row, info, p4Font14(), lv_color_hex(0xE6F4FA));
        lv_obj_set_pos(itemLabel, 12, 21);
        lv_obj_set_width(itemLabel, 700);
        lv_label_set_long_mode(itemLabel, LV_LABEL_LONG_DOT);

        const RowAction actions[3] = {RowAction::Up, RowAction::Down, RowAction::Delete};
        const char* texts[3] = {"▲", "▼", "X"};
        const int xs[3] = {726, 790, 854};
        for (uint8_t buttonIndex = 0; buttonIndex < 3U; ++buttonIndex) {
            rowContexts_[index][buttonIndex].self = this;
            rowContexts_[index][buttonIndex].index = index;
            rowContexts_[index][buttonIndex].action = actions[buttonIndex];
            lv_obj_t* button = lv_btn_create(row);
            lv_obj_set_pos(button, xs[buttonIndex], 8);
            lv_obj_set_size(button, 54, 48);
            lv_obj_set_style_bg_color(button,
                                       buttonIndex == 2U ? lv_color_hex(0x7A3440)
                                                         : lv_color_hex(0x295B73),
                                       LV_PART_MAIN);
            lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
            lv_obj_add_event_cb(button, rowCallback, LV_EVENT_CLICKED,
                                &rowContexts_[index][buttonIndex]);
            lv_obj_t* buttonLabel = label(button, texts[buttonIndex], p4Font20(), lv_color_hex(0xFFFFFF));
            lv_obj_center(buttonLabel);
            if ((actions[buttonIndex] == RowAction::Up && index == 0U) ||
                (actions[buttonIndex] == RowAction::Down && index + 1U >= count)) {
                lv_obj_add_state(button, LV_STATE_DISABLED);
            }
        }
    }
}

void DocumentReviewScreen::refreshSummary() {
    if (!summaryLabel_ || !portal_) return;
    char text[480] = {};
    const char* manifestState = portal_->documentManifestExists() ? p4Texts().ready : "-";
    if (portal_->lastDocumentError().isEmpty()) {
        snprintf(text, sizeof(text), "%s: %u    |    %s: %lu KB    |    Manifest: %s",
                 p4SelectText("Profil", "Profile", "Profiel", "Profil", "Profil", "Perfil", "Profil"),
                 static_cast<unsigned>(portal_->documentItemCount()),
                 p4SelectText("Toplam", "Total", "Totaal", "Gesamt", "Total", "Total", "Razem"),
                 static_cast<unsigned long>(portal_->documentTotalBytes() / 1024U),
                 manifestState);
    } else {
        snprintf(text, sizeof(text), "%s: %u | %s: %lu KB | Manifest: %s | %s: %s",
                 p4SelectText("Profil", "Profile", "Profiel", "Profil", "Profil", "Perfil", "Profil"),
                 static_cast<unsigned>(portal_->documentItemCount()),
                 p4SelectText("Toplam", "Total", "Totaal", "Gesamt", "Total", "Total", "Razem"),
                 static_cast<unsigned long>(portal_->documentTotalBytes() / 1024U),
                 manifestState,
                 p4SelectText("Hata", "Error", "Fout", "Fehler", "Erreur", "Error", "Błąd"),
                 localizeDocumentUiMessage(portal_->lastDocumentError()).c_str());
    }
    lv_label_set_text(summaryLabel_, text);
    if (prepareButton_) {
        if (portal_->documentItemCount() == 0U) lv_obj_add_state(prepareButton_, LV_STATE_DISABLED);
        else lv_obj_clear_state(prepareButton_, LV_STATE_DISABLED);
    }
}

void DocumentReviewScreen::handleRow(uint8_t index, RowAction action) {
    if (!portal_) return;
    if (action == RowAction::Delete) {
        portal_->removeDocumentItem(index);
    } else if (action == RowAction::Up) {
        portal_->moveDocumentItem(index, -1);
    } else if (action == RowAction::Down) {
        portal_->moveDocumentItem(index, 1);
    }
    rebuildList();
    refreshSummary();
}

void DocumentReviewScreen::rowCallback(lv_event_t* event) {
    auto* context = static_cast<RowContext*>(lv_event_get_user_data(event));
    if (!context || !context->self) return;
    context->self->handleRow(context->index, context->action);
}

void DocumentReviewScreen::bottomCallback(lv_event_t* event) {
    auto* context = static_cast<BottomContext*>(lv_event_get_user_data(event));
    if (!context || !context->self) return;
    DocumentReviewScreen* self = context->self;
    const DocumentReviewAction action = context->action;
    if (action == DocumentReviewAction::PrepareAnalysis) {
        if (!self->portal_) return;
        if (self->portal_->documentItemCount() == 0U) {
            (void)self->portal_->recoverDocumentItemsFromStorage();
            self->rebuildList();
        }
        if (self->portal_->documentItemCount() == 0U ||
            !self->portal_->finalizeDocumentManifest()) {
            self->refreshSummary();
            return;
        }
    }
    self->action_ = action;
}

DocumentReviewAction DocumentReviewScreen::consumeAction() {
    const DocumentReviewAction value = action_;
    action_ = DocumentReviewAction::None;
    return value;
}

}  // namespace mg::p4
