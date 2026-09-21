#include "WorkflowOverviewScreen.h"

#include <stdio.h>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

const char* localizedWorkflowState(TestWorkflowState state) {
    switch (state) {
        case TestWorkflowState::Loading:
            return p4SelectText("YÜKLENİYOR", "LOADING", "LADEN", "LÄDT", "CHARGEMENT", "CARGANDO", "ŁADOWANIE");
        case TestWorkflowState::NeedsLookup:
            return p4SelectText("SUNUCU ÇÖZÜMLEME BEKLİYOR", "WAITING SERVER LOOKUP", "WACHT OP SERVERZOEKACTIE", "WARTET AUF SERVERABFRAGE", "ATTENTE RECHERCHE SERVEUR", "ESPERANDO CONSULTA AL SERVIDOR", "OCZEKIWANIE NA SERWER");
        case TestWorkflowState::Ready: return "READY";
        case TestWorkflowState::Running:
            return p4SelectText("TEST EDİLİYOR", "TESTING", "TESTEN", "TEST LÄUFT", "TEST EN COURS", "PROBANDO", "TESTOWANIE");
        case TestWorkflowState::Paused:
            return p4SelectText("DURAKLATILDI", "PAUSED", "GEPAUZEERD", "PAUSIERT", "EN PAUSE", "PAUSADO", "WSTRZYMANO");
        case TestWorkflowState::Complete:
            return p4SelectText("TAMAMLANDI", "COMPLETE", "VOLTOOID", "ABGESCHLOSSEN", "TERMINÉ", "COMPLETADO", "ZAKOŃCZONO");
        case TestWorkflowState::Blocked:
            return p4SelectText("BLOKE", "BLOCKED", "GEBLOKKEERD", "BLOCKIERT", "BLOQUÉ", "BLOQUEADO", "ZABLOKOWANO");
        case TestWorkflowState::Empty:
        default:
            return p4SelectText("PROFİL YOK", "NO PROFILE", "GEEN PROFIEL", "KEIN PROFIL", "AUCUN PROFIL", "SIN PERFIL", "BRAK PROFILU");
    }
}

const char* localizedProfileSource(TestProfileSource source) {
    switch (source) {
        case TestProfileSource::ManualNormal:
            return p4SelectText("SERBEST / NORMAL", "FREE / NORMAL", "VRIJ / NORMAAL", "FREI / NORMAL", "LIBRE / NORMAL", "LIBRE / NORMAL", "DOWOLNY / NORMALNY");
        case TestProfileSource::ManualSubD: return "SUB-D";
        case TestProfileSource::CustomMap:
            return p4SelectText("ÖZEL HARİTA", "CUSTOM MAP", "AANGEPASTE KAART", "SONDERBELEGUNG", "CARTE PERSONNALISÉE", "MAPA PERSONALIZADO", "MAPA NIESTANDARDOWA");
        case TestProfileSource::CableLearn:
            return p4SelectText("KABLO ÖĞRENME", "CABLE LEARN", "KABEL LEREN", "KABEL LERNEN", "APPRENTISSAGE CÂBLE", "APRENDER CABLE", "UCZENIE KABLA");
        case TestProfileSource::BarcodeMgProfile: return "MG BARCODE / QR";
        case TestProfileSource::BarcodeCustomerReference:
            return p4SelectText("MÜŞTERİ KODU", "CUSTOMER CODE", "KLANTCODE", "KUNDENCODE", "CODE CLIENT", "CÓDIGO CLIENTE", "KOD KLIENTA");
        case TestProfileSource::ProductionPr: return "PR / SERVER";
        case TestProfileSource::DocumentImport:
            return p4SelectText("BELGE AKTARIM", "DOCUMENT IMPORT", "DOCUMENTIMPORT", "DOKUMENTIMPORT", "IMPORT DE DOCUMENTS", "IMPORTAR DOCUMENTOS", "IMPORT DOKUMENTÓW");
        case TestProfileSource::ProfileCache:
            return p4SelectText("PROFİL CACHE", "PROFILE CACHE", "PROFIELCACHE", "PROFIL-CACHE", "CACHE PROFIL", "CACHÉ DE PERFIL", "CACHE PROFILU");
        case TestProfileSource::None:
        default:
            return p4SelectText("YOK", "NONE", "GEEN", "KEINE", "AUCUNE", "NINGUNO", "BRAK");
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

lv_color_t verdictColor(TestWorkflowVerdict verdict) {
    switch (verdict) {
        case TestWorkflowVerdict::Pass: return lv_color_hex(0x77E08A);
        case TestWorkflowVerdict::Warning: return lv_color_hex(0xFFD36A);
        case TestWorkflowVerdict::Fail: return lv_color_hex(0xFF7E86);
        case TestWorkflowVerdict::None: default: return lv_color_hex(0xDDEBF2);
    }
}

}  // namespace

void WorkflowOverviewScreen::begin(TestWorkflowController& controller) {
    controller_ = &controller;
    pendingAction_ = WorkflowOverviewAction::None;
    buildUi();
    refresh();
}

void WorkflowOverviewScreen::buildUi() {
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
                            p4SelectText("TEST / PROFİL / SONUÇ AKIŞI", "TEST / PROFILE / RESULT WORKFLOW", "TEST / PROFIEL / RESULTAATFLOW", "TEST / PROFIL / ERGEBNISABLAUF", "TEST / PROFIL / FLUX DE RÉSULTAT", "PRUEBA / PERFIL / FLUJO DE RESULTADO", "TEST / PROFIL / PRZEPŁYW WYNIKU"),
                            p4Font20(), lv_color_hex(0xF1FAFF));
    lv_obj_center(title);

    stateLabel_ = label(screen_, "", p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_set_pos(stateLabel_, 36, 104);
    sourceLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0x8FE4FF));
    lv_obj_set_pos(sourceLabel_, 36, 146);
    profileLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0xDDEBF2));
    lv_obj_set_pos(profileLabel_, 36, 188);
    lv_obj_set_width(profileLabel_, 950);
    identityLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0xB8CBD5));
    lv_obj_set_pos(identityLabel_, 36, 226);
    lv_obj_set_width(identityLabel_, 950);

    progressBar_ = lv_bar_create(screen_);
    lv_obj_set_pos(progressBar_, 36, 282);
    lv_obj_set_size(progressBar_, 770, 26);
    lv_bar_set_range(progressBar_, 0, 100);
    progressLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0xDDEBF2));
    lv_obj_set_pos(progressLabel_, 830, 283);

    resultLabel_ = label(screen_, "", p4Font20(), lv_color_hex(0xDDEBF2));
    lv_obj_set_pos(resultLabel_, 36, 335);
    safetyLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0xFFD36A));
    lv_obj_set_pos(safetyLabel_, 36, 383);
    lv_obj_set_width(safetyLabel_, 950);

    makeButton(screen_, 20, 478, 176,
               p4SelectText("KAYITLAR", "RECORDS", "RECORDS", "AUFZEICHNUNGEN", "ENREGISTREMENTS", "REGISTROS", "REJESTRY"),
               lv_color_hex(0x1677A8), 0U, WorkflowOverviewAction::OpenHistory);
    retestButton_ = makeButton(screen_, 206, 478, 176,
               p4SelectText("TEKRAR TEST", "RETEST", "OPNIEUW TESTEN", "ERNEUT TESTEN", "RETESTER", "REPETIR PRUEBA", "TEST PONOWNIE"),
               lv_color_hex(0x1B8C63), 1U, WorkflowOverviewAction::Retest);
    nextCableButton_ = makeButton(screen_, 392, 478, 176,
               p4SelectText("SONRAKİ KABLO", "NEXT CABLE", "VOLGENDE KABEL", "NÄCHSTES KABEL", "CÂBLE SUIVANT", "SIGUIENTE CABLE", "NASTĘPNY KABEL"),
               lv_color_hex(0xA66A16), 2U, WorkflowOverviewAction::NextCable);
    makeButton(screen_, 578, 478, 176,
               p4SelectText("SPC / TREND", "SPC / TRENDS", "SPC / TRENDS", "SPC / TRENDS", "SPC / TENDANCES", "SPC / TENDENCIAS", "SPC / TRENDY"),
               lv_color_hex(0x1677A8), 3U, WorkflowOverviewAction::OpenSpc);
    makeButton(screen_, 764, 478, 176,
               p4SelectText("GERİ", "BACK", "TERUG", "ZURÜCK", "RETOUR", "VOLVER", "WSTECZ"),
               lv_color_hex(0x4C5962), 4U, WorkflowOverviewAction::Back);
}

lv_obj_t* WorkflowOverviewScreen::makeButton(lv_obj_t* parent,
                                             lv_coord_t x,
                                             lv_coord_t y,
                                             lv_coord_t w,
                                             const char* text,
                                             lv_color_t color,
                                             uint8_t bindingIndex,
                                             WorkflowOverviewAction action) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, 72);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    bindings_[bindingIndex] = {this, action};
    lv_obj_add_event_cb(button, buttonCallback, LV_EVENT_CLICKED, &bindings_[bindingIndex]);
    lv_obj_t* caption = label(button, text, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_center(caption);
    return button;
}

void WorkflowOverviewScreen::buttonCallback(lv_event_t* event) {
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->owner == nullptr) return;
    binding->owner->pendingAction_ = binding->action;
}

void WorkflowOverviewScreen::refresh() {
    if (controller_ == nullptr) return;
    const auto& snap = controller_->snapshot();
    char text[256]{};

    snprintf(text, sizeof(text), "%s  |  %s",
             localizedWorkflowState(snap.state),
             TestWorkflowController::verdictText(snap.verdict));
    lv_label_set_text(stateLabel_, text);
    lv_obj_set_style_text_color(stateLabel_, verdictColor(snap.verdict), LV_PART_MAIN);

    snprintf(text, sizeof(text), "%s: %s",
             p4SelectText("Profil kaynağı", "Profile source", "Profielbron", "Profilquelle", "Source du profil", "Origen del perfil", "Źródło profilu"),
             localizedProfileSource(snap.source));
    lv_label_set_text(sourceLabel_, text);

    const CableProfile& profile = controller_->profile();
    snprintf(text, sizeof(text),
             "%s: %s | %u pin | A=%s | B=%s | %s",
             p4SelectText("Profil", "Profile", "Profiel", "Profil", "Profil", "Perfil", "Profil"),
             snap.profileValid ? (profile.name != nullptr ? profile.name : "-") : "-",
             static_cast<unsigned>(snap.signalPinCount),
             snap.profileValid && profile.sideA.displayName != nullptr ? profile.sideA.displayName : "-",
             snap.profileValid && profile.sideB.displayName != nullptr ? profile.sideB.displayName : "-",
             snap.customMap ? "ELECTRICAL NET MAP" : "ONE-TO-ONE");
    lv_label_set_text(profileLabel_, text);

    char identity[180]{};
    controller_->formatIdentity(identity, sizeof(identity));
    lv_label_set_text(identityLabel_, identity);

    lv_bar_set_value(progressBar_, snap.progressPercent, LV_ANIM_OFF);
    snprintf(text, sizeof(text), "%u%%", static_cast<unsigned>(snap.progressPercent));
    lv_label_set_text(progressLabel_, text);

    snprintf(text, sizeof(text), "%s: %s   |   %s: %u",
             p4SelectText("Sonuç", "Result", "Resultaat", "Ergebnis", "Résultat", "Resultado", "Wynik"),
             TestWorkflowController::verdictText(snap.verdict),
             p4SelectText("Elektriksel hata", "Electrical errors", "Elektrische fouten", "Elektrische Fehler", "Erreurs électriques", "Errores eléctricos", "Błędy elektryczne"),
             static_cast<unsigned>(snap.electricalErrors));
    lv_label_set_text(resultLabel_, text);

    const bool completed = snap.profileValid && snap.state == TestWorkflowState::Complete;
    if (completed) {
        lv_obj_clear_state(retestButton_, LV_STATE_DISABLED);
        lv_obj_clear_state(nextCableButton_, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(retestButton_, LV_STATE_DISABLED);
        lv_obj_add_state(nextCableButton_, LV_STATE_DISABLED);
    }

    if (snap.state == TestWorkflowState::Blocked && controller_->blockReason()[0] != '\0') {
        snprintf(text, sizeof(text), "%s: %s",
                 p4SelectText("BLOKE", "BLOCKED", "GEBLOKKEERD", "BLOCKIERT", "BLOQUÉ", "BLOQUEADO", "ZABLOKOWANO"), controller_->blockReason());
        lv_label_set_text(safetyLabel_, text);
    } else {
        lv_label_set_text(safetyLabel_,
            p4SelectText("GÜVENLİK: Profil READY olsa bile test otomatik BAŞLAMAZ. Operatör START / Tara-Duraklat komutu gerekir.",
                         "SAFETY: READY never auto-starts a test. Operator START / Scan-Pause action is always required.",
                         "VEILIGHEID: READY start nooit automatisch. START / Scan-Pauze door de operator is altijd vereist.",
                         "SICHERHEIT: READY startet niemals automatisch. Bediener-START / Scan-Pause ist immer erforderlich.",
                         "SÉCURITÉ: READY ne lance jamais le test automatiquement. START / Scan-Pause opérateur est toujours requis.",
                         "SEGURIDAD: READY nunca inicia automáticamente. Siempre se requiere START / Escaneo-Pausa del operador.",
                         "BEZPIECZEŃSTWO: READY nigdy nie uruchamia testu automatycznie. Zawsze wymagane jest START / Skan-Pauza operatora."));
    }
}

void WorkflowOverviewScreen::activate() {
    pendingAction_ = WorkflowOverviewAction::None;
    if (screen_ != nullptr && lv_scr_act() != screen_) lv_scr_load(screen_);
    refresh();
}

void WorkflowOverviewScreen::update() {
    refresh();
}

WorkflowOverviewAction WorkflowOverviewScreen::consumeAction() {
    const WorkflowOverviewAction action = pendingAction_;
    pendingAction_ = WorkflowOverviewAction::None;
    return action;
}

}  // namespace mg::p4
