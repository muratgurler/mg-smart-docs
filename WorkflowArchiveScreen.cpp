#include "WorkflowArchiveScreen.h"

#include <stdio.h>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

const char* localizedArchiveSource(TestProfileSource source) {
    switch (source) {
        case TestProfileSource::ManualNormal: return p4SelectText("SERBEST / NORMAL", "FREE / NORMAL", "VRIJ / NORMAAL", "FREI / NORMAL", "LIBRE / NORMAL", "LIBRE / NORMAL", "DOWOLNY / NORMALNY");
        case TestProfileSource::ManualSubD: return "SUB-D";
        case TestProfileSource::CustomMap: return p4SelectText("ÖZEL HARİTA", "CUSTOM MAP", "AANGEPASTE KAART", "SONDERBELEGUNG", "CARTE PERSONNALISÉE", "MAPA PERSONALIZADO", "MAPA NIESTANDARDOWA");
        case TestProfileSource::CableLearn: return p4SelectText("KABLO ÖĞRENME", "CABLE LEARN", "KABEL LEREN", "KABEL LERNEN", "APPRENTISSAGE CÂBLE", "APRENDER CABLE", "UCZENIE KABLA");
        case TestProfileSource::BarcodeMgProfile: return "MG BARCODE / QR";
        case TestProfileSource::BarcodeCustomerReference: return p4SelectText("MÜŞTERİ KODU", "CUSTOMER CODE", "KLANTCODE", "KUNDENCODE", "CODE CLIENT", "CÓDIGO CLIENTE", "KOD KLIENTA");
        case TestProfileSource::ProductionPr: return "PR / SERVER";
        case TestProfileSource::DocumentImport: return p4SelectText("BELGE AKTARIM", "DOCUMENT IMPORT", "DOCUMENTIMPORT", "DOKUMENTIMPORT", "IMPORT DE DOCUMENTS", "IMPORTAR DOCUMENTOS", "IMPORT DOKUMENTÓW");
        case TestProfileSource::ProfileCache: return p4SelectText("PROFİL CACHE", "PROFILE CACHE", "PROFIELCACHE", "PROFIL-CACHE", "CACHE PROFIL", "CACHÉ DE PERFIL", "CACHE PROFILU");
        case TestProfileSource::None:
        default: return p4SelectText("YOK", "NONE", "GEEN", "KEINE", "AUCUNE", "NINGUNO", "BRAK");
    }
}

void flat(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* item = lv_label_create(parent);
    lv_label_set_text(item, text);
    lv_obj_set_style_text_font(item, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(item, color, LV_PART_MAIN);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_CLICKABLE);
    return item;
}

}  // namespace

void WorkflowArchiveScreen::begin(WorkflowArchiveCore& archive) {
    archive_ = &archive;
    mode_ = ViewMode::Summary;
    selectedOffset_ = 0U;
    pendingAction_ = WorkflowArchiveAction::None;
    buildUi();
    refresh();
}

void WorkflowArchiveScreen::setStorageStatus(const char* text) {
    snprintf(storageStatus_, sizeof(storageStatus_), "%s", text != nullptr ? text : "");
    if (storageLabel_ != nullptr) lv_label_set_text(storageLabel_, storageStatus_);
}

void WorkflowArchiveScreen::buildUi() {
    if (screen_ == nullptr) screen_ = lv_obj_create(nullptr); else lv_obj_clean(screen_);
    lv_scr_load(screen_);
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 82);
    flat(header);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x173345), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t* title = label(header,
                            p4SelectText("RAPOR / KALICI ARŞİV", "REPORTS / PERSISTENT ARCHIVE", "RAPPORTEN / PERMANENT ARCHIEF", "BERICHTE / DAUERARCHIV", "RAPPORTS / ARCHIVE PERSISTANTE", "INFORMES / ARCHIVO PERSISTENTE", "RAPORTY / TRWAŁE ARCHIWUM"),
                            p4Font20(), lv_color_hex(0xF1FAFF));
    lv_obj_center(title);

    modeLabel_ = label(screen_, "", p4Font20(), lv_color_hex(0x8FE4FF));
    lv_obj_set_pos(modeLabel_, 32, 100);
    counterLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0xB8CBD5));
    lv_obj_set_pos(counterLabel_, 800, 106);

    primaryLabel_ = label(screen_, "", p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_set_pos(primaryLabel_, 32, 148);
    lv_obj_set_width(primaryLabel_, 960);
    secondaryLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0xDDEBF2));
    lv_obj_set_pos(secondaryLabel_, 32, 198);
    lv_obj_set_width(secondaryLabel_, 960);
    identityLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0xB8CBD5));
    lv_obj_set_pos(identityLabel_, 32, 240);
    lv_obj_set_width(identityLabel_, 960);
    detailLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0xFFD36A));
    lv_obj_set_pos(detailLabel_, 32, 286);
    lv_obj_set_width(detailLabel_, 960);
    reportLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0x9AD7A6));
    lv_obj_set_pos(reportLabel_, 32, 336);
    lv_obj_set_width(reportLabel_, 960);
    storageLabel_ = label(screen_, storageStatus_, p4Font14(), lv_color_hex(0x8FE4FF));
    lv_obj_set_pos(storageLabel_, 32, 410);
    lv_obj_set_width(storageLabel_, 960);

    makeButton(screen_, 12, 492, 116, p4SelectText("ÖZET", "SUMMARY", "OVERZICHT", "ÜBERSICHT", "RÉSUMÉ", "RESUMEN", "PODSUMOWANIE"),
               lv_color_hex(0x1677A8), 0U, ButtonCommand::Summary);
    makeButton(screen_, 136, 492, 116, p4SelectText("KAYIT", "RESULTS", "RESULTATEN", "ERGEBNISSE", "RÉSULTATS", "RESULTADOS", "WYNIKI"),
               lv_color_hex(0x1677A8), 1U, ButtonCommand::Results);
    makeButton(screen_, 260, 492, 116, p4SelectText("PROFİL", "PROFILES", "PROFIELEN", "PROFILE", "PROFILS", "PERFILES", "PROFILE"),
               lv_color_hex(0x1677A8), 2U, ButtonCommand::Profiles);
    previousButton_ = makeButton(screen_, 384, 492, 104, p4SelectText("ÖNCEKİ", "PREV", "VORIGE", "ZURÜCK", "PRÉC.", "ANTERIOR", "POPRZEDNI"),
               lv_color_hex(0x4C5962), 3U, ButtonCommand::Previous);
    nextButton_ = makeButton(screen_, 496, 492, 104, p4SelectText("SONRAKİ", "NEXT", "VOLGENDE", "WEITER", "SUIVANT", "SIGUIENTE", "NASTĘPNY"),
               lv_color_hex(0x4C5962), 4U, ButtonCommand::Next);
    primaryButton_ = makeButton(screen_, 608, 492, 200, p4SelectText("TÜM CSV", "ALL CSV", "ALLE CSV", "ALLE CSV", "TOUT CSV", "TODO CSV", "WSZYSTKIE CSV"),
               lv_color_hex(0x1B8C63), 5U, ButtonCommand::Primary, &primaryButtonLabel_);
    makeButton(screen_, 816, 492, 196, p4SelectText("GERİ", "BACK", "TERUG", "ZURÜCK", "RETOUR", "VOLVER", "WSTECZ"),
               lv_color_hex(0x4C5962), 6U, ButtonCommand::Back);
}

lv_obj_t* WorkflowArchiveScreen::makeButton(lv_obj_t* parent,
                                            lv_coord_t x,
                                            lv_coord_t y,
                                            lv_coord_t width,
                                            const char* text,
                                            lv_color_t color,
                                            uint8_t bindingIndex,
                                            ButtonCommand command,
                                            lv_obj_t** captionOut) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, 70);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    bindings_[bindingIndex] = {this, command};
    lv_obj_add_event_cb(button, buttonCallback, LV_EVENT_CLICKED, &bindings_[bindingIndex]);
    lv_obj_t* caption = label(button, text, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_set_width(caption, width - 8);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(caption);
    if (captionOut != nullptr) *captionOut = caption;
    return button;
}

void WorkflowArchiveScreen::buttonCallback(lv_event_t* event) {
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->owner == nullptr) return;
    binding->owner->handle(binding->command);
}

void WorkflowArchiveScreen::handle(ButtonCommand command) {
    if (archive_ == nullptr) return;
    switch (command) {
        case ButtonCommand::Summary:
            mode_ = ViewMode::Summary;
            selectedOffset_ = 0U;
            break;
        case ButtonCommand::Results:
            mode_ = ViewMode::Results;
            selectedOffset_ = 0U;
            break;
        case ButtonCommand::Profiles:
            mode_ = ViewMode::Profiles;
            selectedOffset_ = 0U;
            break;
        case ButtonCommand::Previous: {
            const size_t count = mode_ == ViewMode::Results ? archive_->resultCount()
                                : mode_ == ViewMode::Profiles ? archive_->profileCacheCount() : 0U;
            if (count > 0U && selectedOffset_ + 1U < count) ++selectedOffset_;
            break;
        }
        case ButtonCommand::Next:
            if (selectedOffset_ > 0U) --selectedOffset_;
            break;
        case ButtonCommand::Primary:
            if (mode_ == ViewMode::Summary && archive_->resultCount() > 0U) {
                pendingAction_ = WorkflowArchiveAction::ExportAllResultsCsv;
            } else if (mode_ == ViewMode::Results && archive_->resultNewest(selectedOffset_) != nullptr) {
                pendingAction_ = WorkflowArchiveAction::ExportSelectedResultCsv;
            } else if (mode_ == ViewMode::Profiles && archive_->cachedProfileNewest(selectedOffset_) != nullptr) {
                pendingAction_ = WorkflowArchiveAction::LoadSelectedProfile;
            }
            break;
        case ButtonCommand::Back:
            pendingAction_ = WorkflowArchiveAction::Back;
            break;
    }
    refresh();
}

void WorkflowArchiveScreen::refresh() {
    if (archive_ == nullptr) return;
    char text[640]{};

    if (mode_ == ViewMode::Summary) {
        lv_obj_add_state(previousButton_, LV_STATE_DISABLED);
        lv_obj_add_state(nextButton_, LV_STATE_DISABLED);
        lv_label_set_text(primaryButtonLabel_, p4SelectText("TÜM CSV", "ALL CSV", "ALLE CSV", "ALLE CSV", "TOUT CSV", "TODO CSV", "WSZYSTKIE CSV"));
        if (archive_->resultCount() > 0U) lv_obj_clear_state(primaryButton_, LV_STATE_DISABLED);
        else lv_obj_add_state(primaryButton_, LV_STATE_DISABLED);
        lv_label_set_text(modeLabel_, p4SelectText("ÜRETİM ÖZETİ", "PRODUCTION SUMMARY", "PRODUCTIEOVERZICHT", "PRODUKTIONSÜBERSICHT", "RÉSUMÉ PRODUCTION", "RESUMEN DE PRODUCCIÓN", "PODSUMOWANIE PRODUKCJI"));
        snprintf(text, sizeof(text), "%u %s | %u %s",
                 static_cast<unsigned>(archive_->resultCount()), p4SelectText("test kaydı", "test records", "testrecords", "Testdatensätze", "enregistrements de test", "registros de prueba", "rekordów testu"),
                 static_cast<unsigned>(archive_->profileCacheCount()), p4SelectText("profil cache", "cached profiles", "profielcache", "Profil-Cache", "profils en cache", "perfiles en caché", "profili w cache"));
        lv_label_set_text(counterLabel_, text);
        lv_label_set_text(primaryLabel_, p4SelectText("SON 32 TESTLİK RAPOR PENCERESİ", "LAST-32-TEST REPORT WINDOW", "RAPPORTVENSTER LAATSTE 32 TESTEN", "BERICHT DER LETZTEN 32 TESTS", "FENÊTRE DES 32 DERNIERS TESTS", "VENTANA DE LOS ÚLTIMOS 32 TESTS", "OKNO OSTATNICH 32 TESTÓW"));
        snprintf(text, sizeof(text), "PASS=%u   WARNING=%u   FAIL=%u   RETEST=%u",
                 static_cast<unsigned>(archive_->passCount()),
                 static_cast<unsigned>(archive_->warningCount()),
                 static_cast<unsigned>(archive_->failCount()),
                 static_cast<unsigned>(archive_->retestCount()));
        lv_label_set_text(secondaryLabel_, text);
        const uint16_t firstCount = archive_->firstAttemptCount();
        const uint16_t firstPass = archive_->firstPassCount();
        const unsigned fpy = firstCount > 0U ? static_cast<unsigned>((100U * firstPass) / firstCount) : 0U;
        snprintf(text, sizeof(text), "%s: %u/%u = %u%%",
                 p4SelectText("Pencere FPY (ilk deneme PASS)", "Window FPY (first-attempt PASS)", "Venster-FPY (PASS bij eerste poging)", "Fenster-FPY (PASS im ersten Versuch)", "FPY fenêtre (PASS au premier essai)", "FPY de ventana (PASS al primer intento)", "FPY okna (PASS za pierwszym razem)"),
                 static_cast<unsigned>(firstPass), static_cast<unsigned>(firstCount), fpy);
        lv_label_set_text(identityLabel_, text);
        lv_label_set_text(detailLabel_,
            p4SelectText("Sonuçlar ve profil cache CRC korumalı, sürümlü LittleFS arşivine kaydedilir.",
                         "Results and profile cache are stored in a versioned, CRC-protected LittleFS archive.",
                         "Resultaten en profielcache worden opgeslagen in een geversioneerd, CRC-beveiligd LittleFS-archief.",
                         "Ergebnisse und Profil-Cache werden in einem versionierten, CRC-geschützten LittleFS-Archiv gespeichert.",
                         "Les résultats et le cache profil sont stockés dans une archive LittleFS versionnée et protégée par CRC.",
                         "Los resultados y la caché de perfiles se guardan en un archivo LittleFS versionado y protegido por CRC.",
                         "Wyniki i cache profilu są zapisywane w wersjonowanym archiwum LittleFS chronionym CRC."));
        lv_label_set_text(reportLabel_,
            p4SelectText("TÜM CSV: görünür arşiv test kayıtlarını /mgreports/results_all.csv dosyasına aktarır.",
                         "ALL CSV exports the visible archive window to /mgreports/results_all.csv.",
                         "ALLE CSV exporteert het zichtbare archiefvenster naar /mgreports/results_all.csv.",
                         "ALLE CSV exportiert das sichtbare Archivfenster nach /mgreports/results_all.csv.",
                         "TOUT CSV exporte la fenêtre d'archive visible vers /mgreports/results_all.csv.",
                         "TODO CSV exporta la ventana visible del archivo a /mgreports/results_all.csv.",
                         "WSZYSTKIE CSV eksportuje widoczne okno archiwum do /mgreports/results_all.csv."));
        lv_label_set_text(storageLabel_, storageStatus_);
        return;
    }

    const size_t count = mode_ == ViewMode::Results ? archive_->resultCount() : archive_->profileCacheCount();
    if (count == 0U) selectedOffset_ = 0U;
    else if (selectedOffset_ >= count) selectedOffset_ = count - 1U;
    if (count > 1U && selectedOffset_ + 1U < count) lv_obj_clear_state(previousButton_, LV_STATE_DISABLED);
    else lv_obj_add_state(previousButton_, LV_STATE_DISABLED);
    if (selectedOffset_ > 0U) lv_obj_clear_state(nextButton_, LV_STATE_DISABLED);
    else lv_obj_add_state(nextButton_, LV_STATE_DISABLED);

    lv_label_set_text(modeLabel_, mode_ == ViewMode::Results
        ? p4SelectText("TEST SONUÇLARI", "TEST RESULTS", "TESTRESULTATEN", "TESTERGEBNISSE", "RÉSULTATS DE TEST", "RESULTADOS DE PRUEBA", "WYNIKI TESTÓW")
        : p4SelectText("SON KULLANILAN PROFİLLER", "RECENT PROFILES", "RECENTE PROFIELEN", "LETZTE PROFILE", "PROFILS RÉCENTS", "PERFILES RECIENTES", "OSTATNIE PROFILE"));
    snprintf(text, sizeof(text), "%u / %u",
             count == 0U ? 0U : static_cast<unsigned>(count - selectedOffset_),
             static_cast<unsigned>(count));
    lv_label_set_text(counterLabel_, text);

    if (mode_ == ViewMode::Results) {
        lv_label_set_text(primaryButtonLabel_, p4SelectText("KAYDI CSV", "RECORD CSV", "RECORD CSV", "DATENSATZ CSV", "ENREG. CSV", "REGISTRO CSV", "REKORD CSV"));
        const WorkflowTestRecord* record = archive_->resultNewest(selectedOffset_);
        if (record == nullptr) {
            lv_obj_add_state(primaryButton_, LV_STATE_DISABLED);
            lv_label_set_text(primaryLabel_, p4SelectText("Henüz tamamlanmış test kaydı yok.", "No completed test record yet.", "Nog geen voltooid testrecord.", "Noch kein abgeschlossener Testdatensatz.", "Aucun test terminé enregistré.", "Aún no hay ningún registro de prueba completado.", "Brak zakończonego rekordu testu."));
            lv_label_set_text(secondaryLabel_, "");
            lv_label_set_text(identityLabel_, "");
            lv_label_set_text(detailLabel_, "");
            lv_label_set_text(reportLabel_, "");
            lv_label_set_text(storageLabel_, storageStatus_);
            return;
        }
        lv_obj_clear_state(primaryButton_, LV_STATE_DISABLED);
        snprintf(text, sizeof(text), "#%lu  %s  |  %s",
                 static_cast<unsigned long>(record->recordId),
                 record->profileName,
                 TestWorkflowController::verdictText(record->verdict));
        lv_label_set_text(primaryLabel_, text);
        snprintf(text, sizeof(text), "%s | A=%s | B=%s | pins=%u | map=%s",
                 localizedArchiveSource(record->source),
                 record->connectorA, record->connectorB,
                 static_cast<unsigned>(record->signalPinCount),
                 record->customMap ? "ELECTRICAL NET" : "ONE-TO-ONE");
        lv_label_set_text(secondaryLabel_, text);
        snprintf(text, sizeof(text), "PR=%s | REF=%s | REV=%s | PROFILE=%s",
                 record->identity.productionPr[0] ? record->identity.productionPr : "-",
                 record->identity.customerReference[0] ? record->identity.customerReference : "-",
                 record->identity.revision[0] ? record->identity.revision : "-",
                 record->identity.profileId[0] ? record->identity.profileId : "-");
        lv_label_set_text(identityLabel_, text);
        snprintf(text, sizeof(text), "%s=%u | generation=%lu | attempt=%u | quality=%s | PASS/WARN/FAIL=%u/%u/%u",
                 p4SelectText("Elektriksel hata", "Electrical errors", "Elektrische fouten", "Elektrische Fehler", "Erreurs électriques", "Errores eléctricos", "Błędy elektryczne"),
                 static_cast<unsigned>(record->electricalErrors),
                 static_cast<unsigned long>(record->generation),
                 static_cast<unsigned>(record->attempt),
                 record->qualityWarning ? "WARNING" : "OK",
                 static_cast<unsigned>(archive_->passCount()),
                 static_cast<unsigned>(archive_->warningCount()),
                 static_cast<unsigned>(archive_->failCount()));
        lv_label_set_text(detailLabel_, text);
        archive_->formatRecordCsv(*record, text, sizeof(text));
        lv_label_set_text(reportLabel_, text);
    } else {
        lv_label_set_text(primaryButtonLabel_, p4SelectText("PROFİLİ YÜKLE", "LOAD PROFILE", "PROFIEL LADEN", "PROFIL LADEN", "CHARGER PROFIL", "CARGAR PERFIL", "WCZYTAJ PROFIL"));
        const WorkflowCachedProfile* item = archive_->cachedProfileNewest(selectedOffset_);
        if (item == nullptr) {
            lv_obj_add_state(primaryButton_, LV_STATE_DISABLED);
            lv_label_set_text(primaryLabel_, p4SelectText("Profil cache boş.", "Profile cache is empty.", "Profielcache is leeg.", "Profil-Cache ist leer.", "Le cache profil est vide.", "La caché de perfiles está vacía.", "Cache profilu jest pusty."));
            lv_label_set_text(secondaryLabel_, "");
            lv_label_set_text(identityLabel_, "");
            lv_label_set_text(detailLabel_, "");
            lv_label_set_text(reportLabel_, "");
            lv_label_set_text(storageLabel_, storageStatus_);
            return;
        }
        lv_obj_clear_state(primaryButton_, LV_STATE_DISABLED);
        snprintf(text, sizeof(text), "CACHE #%lu  |  %s",
                 static_cast<unsigned long>(item->cacheId), item->profileName);
        lv_label_set_text(primaryLabel_, text);
        snprintf(text, sizeof(text), "%s | A=%s | B=%s | pins=%u | map=%s",
                 localizedArchiveSource(item->source),
                 item->connectorA, item->connectorB,
                 static_cast<unsigned>(item->profile.signalPinCount),
                 item->customMap ? "ELECTRICAL NET" : "ONE-TO-ONE");
        lv_label_set_text(secondaryLabel_, text);
        snprintf(text, sizeof(text), "REF=%s | REV=%s | PROFILE=%s",
                 item->identity.customerReference[0] ? item->identity.customerReference : "-",
                 item->identity.revision[0] ? item->identity.revision : "-",
                 item->identity.profileId[0] ? item->identity.profileId : "-");
        lv_label_set_text(identityLabel_, text);
        snprintf(text, sizeof(text), "%s: LittleFS kalici cache | fingerprint=%08lX%08lX",
                 p4SelectText("Depolama", "Storage", "Opslag", "Speicher", "Stockage", "Almacenamiento", "Pamięć"),
                 static_cast<unsigned long>((item->mapFingerprint >> 32U) & 0xFFFFFFFFULL),
                 static_cast<unsigned long>(item->mapFingerprint & 0xFFFFFFFFULL));
        lv_label_set_text(detailLabel_, text);
        lv_label_set_text(reportLabel_,
            p4SelectText("PROFİLİ YÜKLE: Cache profili READY yapar; otomatik START kesinlikle yoktur.",
                         "LOAD PROFILE restores cache to READY; automatic START remains forbidden.",
                         "PROFIEL LADEN zet het cacheprofiel op READY; automatisch START blijft verboden.",
                         "PROFIL LADEN setzt das Cache-Profil auf READY; automatischer START bleibt gesperrt.",
                         "CHARGER PROFIL remet le profil en cache à READY; le START automatique reste interdit.",
                         "CARGAR PERFIL pone el perfil en caché en READY; START automático sigue prohibido.",
                         "WCZYTAJ PROFIL ustawia profil z cache na READY; automatyczny START pozostaje zabroniony."));
    }
    lv_label_set_text(storageLabel_, storageStatus_);
}

void WorkflowArchiveScreen::activate() {
    pendingAction_ = WorkflowArchiveAction::None;
    if (screen_ != nullptr && lv_scr_act() != screen_) lv_scr_load(screen_);
    refresh();
}

void WorkflowArchiveScreen::update() {
    refresh();
}

WorkflowArchiveAction WorkflowArchiveScreen::consumeAction() {
    const WorkflowArchiveAction action = pendingAction_;
    pendingAction_ = WorkflowArchiveAction::None;
    return action;
}

}  // namespace mg::p4
