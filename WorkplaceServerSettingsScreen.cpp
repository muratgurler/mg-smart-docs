#include "WorkplaceServerSettingsScreen.h"

#include <Arduino.h>
#include <stdlib.h>
#include <cstring>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

void flat(lv_obj_t* o) {
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
}

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, color, LV_PART_MAIN);
    return l;
}

lv_obj_t* button(lv_obj_t* parent, int x, int y, int w, int h, const char* text,
                 lv_color_t color, lv_event_cb_t cb, void* userData) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, color, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 9, LV_PART_MAIN);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, userData);
    lv_obj_t* l = label(b, text, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_center(l);
    return b;
}

static const char* kKeyboardLower[] = {
    "123", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "DEL", "\n",
    "SHIFT", "a", "s", "d", "f", "g", "h", "j", "k", "l", "NL", "\n",
    "ABC", "z", "x", "c", "v", "b", "n", "m", ".", ":", "/", "\n",
    "CANCEL", "<", "SPACE", ">", "OK", ""
};
static const char* kKeyboardUpper[] = {
    "123", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "DEL", "\n",
    "abc", "A", "S", "D", "F", "G", "H", "J", "K", "L", "NL", "\n",
    "ABC", "Z", "X", "C", "V", "B", "N", "M", ".", ":", "/", "\n",
    "CANCEL", "<", "SPACE", ">", "OK", ""
};
static const char* kKeyboardSpecial[] = {
    "ABC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "DEL", "\n",
    "@", "#", "$", "%", "&", "*", "-", "+", "=", "_", "NL", "\n",
    "{", "}", "[", "]", "(", ")", "?", "!", ",", ";", "\\", "\n",
    "CANCEL", "<", "SPACE", ">", "OK", ""
};

bool parseUnsigned(const String& text, uint32_t minValue, uint32_t maxValue, uint32_t& out) {
    if (text.isEmpty()) return false;
    char* end = nullptr;
    const unsigned long v = strtoul(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || v < minValue || v > maxValue) return false;
    out = static_cast<uint32_t>(v);
    return true;
}

}  // namespace

void WorkplaceServerSettingsScreen::begin(const WorkplaceServerConfig& config, const char* storageStatus) {
    config_ = config;
    storageStatus_ = storageStatus != nullptr ? storageStatus : "";
    pendingAction_ = WorkplaceServerSettingsAction::None;
    editorOverlay_ = nullptr;
    editorTitle_ = nullptr;
    editorTextArea_ = nullptr;
    keyboard_ = nullptr;

    if (!screen_) screen_ = lv_obj_create(nullptr); else lv_obj_clean(screen_);
    lv_scr_load(screen_);
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 82);
    flat(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_t* title = label(header,
        p4SelectText("WORKPLACE SERVER AYARLARI", "WORKPLACE SERVER SETTINGS", "WORKPLACE-SERVERINSTELLINGEN", "WORKPLACE-SERVER-EINSTELLUNGEN", "RÉGLAGES SERVEUR WORKPLACE", "AJUSTES DEL SERVIDOR WORKPLACE", "USTAWIENIA SERWERA WORKPLACE"),
        p4Font20(), lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t* sub = label(header,
        p4SelectText("Sonradan elle girilir; kaydedilene kadar CONFIG_REQUIRED", "Enter later; remains CONFIG_REQUIRED until saved", "Later handmatig invoeren; blijft CONFIG_REQUIRED tot opslaan", "Später manuell eingeben; bleibt bis zum Speichern CONFIG_REQUIRED", "À saisir manuellement plus tard; reste CONFIG_REQUIRED jusqu'à l'enregistrement", "Se introduce manualmente después; permanece CONFIG_REQUIRED hasta guardar", "Wprowadź ręcznie później; pozostaje CONFIG_REQUIRED do zapisu"),
        p4Font14(), lv_color_hex(0x9CC6DC));
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -8);
    soundButton_.create(header, 970, 18, 42);

    statusLabel_ = label(screen_, "", p4Font14(), lv_color_hex(0xD5ECF6));
    lv_obj_set_pos(statusLabel_, 32, 88);
    lv_obj_set_width(statusLabel_, 960);

    body_ = lv_obj_create(screen_);
    lv_obj_set_pos(body_, 24, 116);
    lv_obj_set_size(body_, 976, 390);
    lv_obj_set_style_bg_color(body_, lv_color_hex(0x0C1921), LV_PART_MAIN);
    lv_obj_set_style_border_color(body_, lv_color_hex(0x33576A), LV_PART_MAIN);
    lv_obj_set_style_border_width(body_, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body_, 8, LV_PART_MAIN);
    lv_obj_set_scroll_dir(body_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body_, LV_SCROLLBAR_MODE_AUTO);

    buildRows();

    button(screen_, 24, 520, 220, 60,
           p4SelectText("KAYDET", "SAVE", "OPSLAAN", "SPEICHERN", "ENREGISTRER", "GUARDAR", "ZAPISZ"),
           lv_color_hex(0x218A55), saveCallback, this);
    button(screen_, 260, 520, 220, 60,
           p4SelectText("SIFIRLA / SİL", "RESET / CLEAR", "RESET / WISSEN", "RESET / LÖSCHEN", "RESET / EFFACER", "RESET / BORRAR", "RESET / WYCZYŚĆ"),
           lv_color_hex(0x8A4B3D), resetCallback, this);
    button(screen_, 780, 520, 220, 60,
           p4SelectText("GERİ", "BACK", "TERUG", "ZURÜCK", "RETOUR", "VOLVER", "WSTECZ"),
           lv_color_hex(0x6A4B8A), backCallback, this);

    refreshRows();
    refreshStatus();
}

void WorkplaceServerSettingsScreen::buildRows() {
    const uint8_t count = static_cast<uint8_t>(Field::Count);
    for (uint8_t i = 0; i < count; ++i) {
        const Field field = static_cast<Field>(i);
        lv_obj_t* row = lv_btn_create(body_);
        lv_obj_set_pos(row, 6, static_cast<int>(i) * 62);
        lv_obj_set_size(row, 942, 54);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x17303D), LV_PART_MAIN);
        lv_obj_set_style_radius(row, 7, LV_PART_MAIN);
        bindings_[i] = {this, field};
        lv_obj_add_event_cb(row, fieldCallback, LV_EVENT_CLICKED, &bindings_[i]);
        lv_obj_t* k = label(row, fieldTitle(field), p4Font14(), lv_color_hex(0x9CC6DC));
        lv_obj_set_pos(k, 14, 17);
        lv_obj_set_width(k, 245);
        rowValue_[i] = label(row, "", p4Font14(), lv_color_hex(0xFFFFFF));
        lv_obj_set_pos(rowValue_[i], 270, 17);
        lv_obj_set_width(rowValue_[i], 650);
        lv_label_set_long_mode(rowValue_[i], LV_LABEL_LONG_DOT);
    }
}

const char* WorkplaceServerSettingsScreen::fieldTitle(Field field) const {
    switch (field) {
        case Field::Endpoint: return "PR URL / PATH";
        case Field::AuthMode: return p4SelectText("AUTH TÜRÜ", "AUTH MODE", "AUTH-MODUS", "AUTH-MODUS", "MODE AUTH", "MODO AUTH", "TRYB AUTH");
        case Field::AuthUser: return p4SelectText("KULLANICI", "USERNAME", "GEBRUIKER", "BENUTZER", "UTILISATEUR", "USUARIO", "UŻYTKOWNIK");
        case Field::AuthSecret: return p4SelectText("ŞİFRE / TOKEN / DEĞER", "PASSWORD / TOKEN / VALUE", "WACHTWOORD / TOKEN / WAARDE", "PASSWORT / TOKEN / WERT", "MOT DE PASSE / TOKEN / VALEUR", "CONTRASEÑA / TOKEN / VALOR", "HASŁO / TOKEN / WARTOŚĆ");
        case Field::HeaderName: return p4SelectText("CUSTOM HEADER ADI", "CUSTOM HEADER NAME", "NAAM CUSTOM HEADER", "NAME DES CUSTOM HEADERS", "NOM HEADER PERSONNALISÉ", "NOMBRE DE HEADER PERSONALIZADO", "NAZWA CUSTOM HEADER");
        case Field::TlsCa: return "HTTPS CA PEM";
        case Field::ResponseFormat: return p4SelectText("CEVAP FORMATI", "RESPONSE FORMAT", "ANTWOORDFORMAAT", "ANTWORTFORMAT", "FORMAT DE RÉPONSE", "FORMATO DE RESPUESTA", "FORMAT ODPOWIEDZI");
        case Field::FieldMap: return p4SelectText("JSON ALAN EŞLEMESİ", "JSON FIELD MAP", "JSON-VELDMAPPING", "JSON-FELDZUORDNUNG", "MAPPAGE DES CHAMPS JSON", "MAPEO DE CAMPOS JSON", "MAPOWANIE PÓL JSON");
        case Field::Timeout: return "TIMEOUT (ms)";
        case Field::MaxResponse: return p4SelectText("MAKS. CEVAP (byte)", "MAX RESPONSE (bytes)", "MAX ANTWOORD (bytes)", "MAX. ANTWORT (Bytes)", "RÉPONSE MAX (octets)", "RESPUESTA MÁX. (bytes)", "MAKS. ODPOWIEDŹ (bajty)");
        case Field::Count: break;
    }
    return "";
}

String WorkplaceServerSettingsScreen::fieldSummary(Field field) const {
    switch (field) {
        case Field::Endpoint: return config_.prEndpointTemplate.isEmpty() ? p4SelectText("(boş)", "(empty)", "(leeg)", "(leer)", "(vide)", "(vacío)", "(puste)") : config_.prEndpointTemplate;
        case Field::AuthMode: return workplaceAuthModeName(config_.authMode);
        case Field::AuthUser: return config_.authUser.isEmpty() ? p4SelectText("(boş)", "(empty)", "(leeg)", "(leer)", "(vide)", "(vacío)", "(puste)") : config_.authUser;
        case Field::AuthSecret: return config_.authSecret.isEmpty() ? p4SelectText("(boş)", "(empty)", "(leeg)", "(leer)", "(vide)", "(vacío)", "(puste)") : String("********  (") + config_.authSecret.length() + " chars)";
        case Field::HeaderName: return config_.customHeaderName.isEmpty() ? p4SelectText("(boş)", "(empty)", "(leeg)", "(leer)", "(vide)", "(vacío)", "(puste)") : config_.customHeaderName;
        case Field::TlsCa: return config_.tlsCaPem.isEmpty() ? p4SelectText("(boş)", "(empty)", "(leeg)", "(leer)", "(vide)", "(vacío)", "(puste)") : String("SET  (") + config_.tlsCaPem.length() + " bytes)";
        case Field::ResponseFormat: return workplaceResponseFormatName(config_.responseFormat);
        case Field::FieldMap: return config_.responseFormat == WorkplaceResponseFormat::CanonicalJsonV1 ? p4SelectText("kanonik alanlar", "canonical keys", "canonieke velden", "kanonische Felder", "champs canoniques", "campos canónicos", "pola kanoniczne") : workplaceFieldMapSpec(config_.fieldMap);
        case Field::Timeout: return String(config_.timeoutMs);
        case Field::MaxResponse: return String(static_cast<uint32_t>(config_.maxResponseBytes));
        case Field::Count: break;
    }
    return String();
}

void WorkplaceServerSettingsScreen::refreshRows() {
    const uint8_t count = static_cast<uint8_t>(Field::Count);
    for (uint8_t i = 0; i < count; ++i) {
        if (rowValue_[i]) lv_label_set_text(rowValue_[i], fieldSummary(static_cast<Field>(i)).c_str());
    }
}

void WorkplaceServerSettingsScreen::refreshStatus() {
    if (!statusLabel_) return;
    const WorkplaceServerConfigValidation v = validateWorkplaceServerConfig(config_);
    String text = v.ready ? "READY" : "CONFIG_REQUIRED";
    text += " | ";
    if (v.ready) {
        text += p4SelectText("Hazır", "Ready", "Gereed", "Bereit", "Prêt", "Listo", "Gotowe");
    } else if (!v.endpointValid) {
        text += p4SelectText("URL http(s) olmalı ve {PR} içermeli", "URL must be http(s) and contain {PR}", "URL moet http(s) zijn en {PR} bevatten", "URL muss http(s) sein und {PR} enthalten", "L'URL doit être http(s) et contenir {PR}", "La URL debe ser http(s) y contener {PR}", "URL musi być http(s) i zawierać {PR}");
    } else if (!v.authValid) {
        text += p4SelectText("Kimlik doğrulama alanları eksik", "Authentication fields are incomplete", "Authenticatievelden zijn onvolledig", "Authentifizierungsfelder sind unvollständig", "Champs d'authentification incomplets", "Campos de autenticación incompletos", "Pola uwierzytelniania są niekompletne");
    } else if (!v.tlsValid) {
        text += p4SelectText("HTTPS için CA PEM gerekli", "HTTPS requires CA PEM", "HTTPS vereist CA PEM", "HTTPS erfordert CA PEM", "HTTPS nécessite un CA PEM", "HTTPS requiere CA PEM", "HTTPS wymaga CA PEM");
    } else {
        text += p4SelectText("Cevap formatı / alan eşlemesi geçersiz", "Response format / field map is invalid", "Antwoordformaat / veldmapping is ongeldig", "Antwortformat / Feldzuordnung ist ungültig", "Format de réponse / mappage invalide", "Formato de respuesta / mapeo inválido", "Format odpowiedzi / mapowanie pól jest nieprawidłowe");
    }
    if (!storageStatus_.isEmpty()) {
        text += " | ";
        text += storageStatus_;
    }
    lv_label_set_text(statusLabel_, text.c_str());
    lv_obj_set_style_text_color(statusLabel_, v.ready ? lv_color_hex(0x35D273) : lv_color_hex(0xFFD34D), LV_PART_MAIN);
}

void WorkplaceServerSettingsScreen::handleField(Field field) {
    if (field == Field::AuthMode) {
        uint8_t mode = static_cast<uint8_t>(config_.authMode);
        config_.authMode = static_cast<WorkplaceAuthMode>((mode + 1U) % 4U);
        refreshRows(); refreshStatus();
        return;
    }
    if (field == Field::ResponseFormat) {
        config_.responseFormat = config_.responseFormat == WorkplaceResponseFormat::CanonicalJsonV1
            ? WorkplaceResponseFormat::MappedJsonV1 : WorkplaceResponseFormat::CanonicalJsonV1;
        refreshRows(); refreshStatus();
        return;
    }
    openEditor(field);
}

String WorkplaceServerSettingsScreen::fieldText(Field field) const {
    switch (field) {
        case Field::Endpoint: return config_.prEndpointTemplate;
        case Field::AuthUser: return config_.authUser;
        case Field::AuthSecret: return config_.authSecret;
        case Field::HeaderName: return config_.customHeaderName;
        case Field::TlsCa: return config_.tlsCaPem;
        case Field::FieldMap: return workplaceFieldMapSpec(config_.fieldMap);
        case Field::Timeout: return String(config_.timeoutMs);
        case Field::MaxResponse: return String(static_cast<uint32_t>(config_.maxResponseBytes));
        default: return String();
    }
}

void WorkplaceServerSettingsScreen::openEditor(Field field) {
    if (!screen_ || editorOverlay_) return;
    editingField_ = field;
    editorOverlay_ = lv_obj_create(screen_);
    lv_obj_set_pos(editorOverlay_, 14, 84);
    lv_obj_set_size(editorOverlay_, 996, 502);
    flat(editorOverlay_);
    lv_obj_set_style_bg_color(editorOverlay_, lv_color_hex(0x10232F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(editorOverlay_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(editorOverlay_, lv_color_hex(0x4C819A), LV_PART_MAIN);
    lv_obj_set_style_border_width(editorOverlay_, 2, LV_PART_MAIN);

    editorTitle_ = label(editorOverlay_, fieldTitle(field), p4Font14(), lv_color_hex(0xEAF6FF));
    lv_obj_set_pos(editorTitle_, 20, 8);
    editorTextArea_ = lv_textarea_create(editorOverlay_);
    lv_obj_set_pos(editorTextArea_, 20, 35);
    const bool multiline = field == Field::TlsCa || field == Field::FieldMap;
    lv_obj_set_size(editorTextArea_, 956, multiline ? 125 : 55);
    lv_textarea_set_one_line(editorTextArea_, !multiline);
    lv_textarea_set_max_length(editorTextArea_, field == Field::TlsCa ? 3500 : (field == Field::FieldMap ? 900 : 384));
    lv_obj_set_style_text_font(editorTextArea_, p4Font14(), LV_PART_MAIN);
    lv_textarea_set_text(editorTextArea_, fieldText(field).c_str());
    if (field == Field::AuthSecret) lv_textarea_set_password_mode(editorTextArea_, true);

    keyboard_ = lv_btnmatrix_create(editorOverlay_);
    lv_btnmatrix_set_map(keyboard_, kKeyboardLower);
    lv_obj_set_pos(keyboard_, 20, multiline ? 168 : 100);
    lv_obj_set_size(keyboard_, 956, multiline ? 312 : 380);
    lv_obj_clear_flag(keyboard_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(keyboard_, p4Font14(), LV_PART_ITEMS);
    lv_obj_add_event_cb(keyboard_, keyboardCallback, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_state(editorTextArea_, LV_STATE_FOCUSED);
}

bool WorkplaceServerSettingsScreen::applyFieldText(Field field, const String& raw, String& error) {
    String value = raw;
    if (field != Field::TlsCa && field != Field::FieldMap && field != Field::AuthSecret) value.trim();
    error = String();
    switch (field) {
        case Field::Endpoint: config_.prEndpointTemplate = value; return true;
        case Field::AuthUser: config_.authUser = value; return true;
        case Field::AuthSecret: config_.authSecret = value; return true;
        case Field::HeaderName: config_.customHeaderName = value; return true;
        case Field::TlsCa: config_.tlsCaPem = value; return true;
        case Field::FieldMap: {
            WorkplaceJsonFieldMap mapped{};
            if (!parseWorkplaceFieldMapSpec(value, mapped, error)) return false;
            config_.fieldMap = mapped;
            return true;
        }
        case Field::Timeout: {
            uint32_t v = 0U;
            if (!parseUnsigned(value, 500U, 60000U, v)) { error = "500..60000 ms"; return false; }
            config_.timeoutMs = v; return true;
        }
        case Field::MaxResponse: {
            uint32_t v = 0U;
            if (!parseUnsigned(value, 1024U, 131072U, v)) { error = "1024..131072 bytes"; return false; }
            config_.maxResponseBytes = static_cast<size_t>(v); return true;
        }
        default: return true;
    }
}

void WorkplaceServerSettingsScreen::closeEditor(bool commit) {
    if (!editorOverlay_) return;
    if (commit && editorTextArea_) {
        String error;
        if (!applyFieldText(editingField_, String(lv_textarea_get_text(editorTextArea_)), error)) {
            lv_textarea_set_placeholder_text(editorTextArea_, error.c_str());
            return;
        }
    }
    lv_obj_del(editorOverlay_);
    editorOverlay_ = nullptr;
    editorTitle_ = nullptr;
    editorTextArea_ = nullptr;
    keyboard_ = nullptr;
    refreshRows();
    refreshStatus();
}

void WorkplaceServerSettingsScreen::fieldCallback(lv_event_t* event) {
    auto* binding = static_cast<FieldBinding*>(lv_event_get_user_data(event));
    if (binding && binding->owner) binding->owner->handleField(binding->field);
}
void WorkplaceServerSettingsScreen::saveCallback(lv_event_t* event) {
    auto* self = static_cast<WorkplaceServerSettingsScreen*>(lv_event_get_user_data(event));
    if (self) self->pendingAction_ = WorkplaceServerSettingsAction::Save;
}
void WorkplaceServerSettingsScreen::resetCallback(lv_event_t* event) {
    auto* self = static_cast<WorkplaceServerSettingsScreen*>(lv_event_get_user_data(event));
    if (self) self->pendingAction_ = WorkplaceServerSettingsAction::Reset;
}
void WorkplaceServerSettingsScreen::backCallback(lv_event_t* event) {
    auto* self = static_cast<WorkplaceServerSettingsScreen*>(lv_event_get_user_data(event));
    if (self) self->pendingAction_ = WorkplaceServerSettingsAction::Back;
}

void WorkplaceServerSettingsScreen::keyboardCallback(lv_event_t* event) {
    auto* self = static_cast<WorkplaceServerSettingsScreen*>(lv_event_get_user_data(event));
    if (!self || !self->keyboard_ || !self->editorTextArea_ || lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
    const uint16_t id = lv_btnmatrix_get_selected_btn(self->keyboard_);
    if (id == LV_BTNMATRIX_BTN_NONE) return;
    const char* key = lv_btnmatrix_get_btn_text(self->keyboard_, id);
    if (!key || !*key) return;
    if (strcmp(key, "SHIFT") == 0) lv_btnmatrix_set_map(self->keyboard_, kKeyboardUpper);
    else if (strcmp(key, "abc") == 0 || strcmp(key, "ABC") == 0) lv_btnmatrix_set_map(self->keyboard_, kKeyboardLower);
    else if (strcmp(key, "123") == 0) lv_btnmatrix_set_map(self->keyboard_, kKeyboardSpecial);
    else if (strcmp(key, "DEL") == 0) lv_textarea_del_char(self->editorTextArea_);
    else if (strcmp(key, "CANCEL") == 0) self->closeEditor(false);
    else if (strcmp(key, "OK") == 0) self->closeEditor(true);
    else if (strcmp(key, "<") == 0) lv_textarea_cursor_left(self->editorTextArea_);
    else if (strcmp(key, ">") == 0) lv_textarea_cursor_right(self->editorTextArea_);
    else if (strcmp(key, "SPACE") == 0) lv_textarea_add_text(self->editorTextArea_, " ");
    else if (strcmp(key, "NL") == 0) lv_textarea_add_char(self->editorTextArea_, '\n');
    else lv_textarea_add_text(self->editorTextArea_, key);
}

void WorkplaceServerSettingsScreen::setStorageStatus(const char* text) {
    storageStatus_ = text != nullptr ? text : "";
    refreshStatus();
}

void WorkplaceServerSettingsScreen::activate() {
    pendingAction_ = WorkplaceServerSettingsAction::None;
    if (screen_ && lv_scr_act() != screen_) lv_scr_load(screen_);
}

WorkplaceServerSettingsAction WorkplaceServerSettingsScreen::consumeAction() {
    const auto action = pendingAction_;
    pendingAction_ = WorkplaceServerSettingsAction::None;
    return action;
}

}  // namespace mg::p4
