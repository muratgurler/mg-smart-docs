#include "CustomMapMenuScreen.h"

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {
struct Texts {
    const char* title;
    const char* subtitle;
    const char* test;
    const char* testInfo;
    const char* edit;
    const char* editInfo;
    const char* web;
    const char* webReady;
    const char* webOffline;
    const char* back;
};

const Texts& texts() {
    static const Texts tr{"ÖZEL HARİTA", "Özel bağlantı haritasını test edin veya düzenleyin",
                          "ÖZEL HARİTA İLE TEST", "Kayıtlı haritayı kullanarak A->B / B->A tarama",
                          "DOKUNMATİK HARİTA EDİTÖRÜ", "7 inç ekranda 64 + 64 pini doğrudan düzenle",
                          "WEB HARİTA EDİTÖRÜ", "Tarayıcıdan düzenleme hazır", "Wi-Fi/Ethernet bağlantısı bekleniyor", "GERİ"};
    static const Texts en{"CUSTOM MAP", "Test or edit the custom wiring map",
                          "TEST WITH CUSTOM MAP", "Scan A->B / B->A using the saved map",
                          "TOUCH MAP EDITOR", "Edit all 64 + 64 points directly on the 7-inch display",
                          "WEB MAP EDITOR", "Browser editor is ready", "Waiting for Wi-Fi/Ethernet", "BACK"};
    static const Texts nl{"AANGEPASTE KAART", "Test of bewerk de aangepaste kabelkaart",
                          "TEST MET KAART", "Scan A->B / B->A met de opgeslagen kaart",
                          "TOUCH KAARTEDITOR", "Bewerk 64 + 64 punten op het 7-inch scherm",
                          "WEB KAARTEDITOR", "Browsereditor is gereed", "Wacht op Wi-Fi/Ethernet", "TERUG"};
    static const Texts de{"SONDERBELEGUNG", "Sonderbelegung testen oder bearbeiten",
                          "MIT SONDERBELEGUNG TESTEN", "A->B / B->A mit gespeicherter Belegung scannen",
                          "TOUCH-BELEGUNGSEDITOR", "64 + 64 Punkte direkt auf dem 7-Zoll-Display bearbeiten",
                          "WEB-BELEGUNGSEDITOR", "Browser-Editor ist bereit", "Warte auf Wi-Fi/Ethernet", "ZURÜCK"};
    static const Texts fr{"CARTE PERSONNALISÉE", "Tester ou modifier la carte de câblage",
                          "TESTER AVEC LA CARTE", "Scan A->B / B->A avec la carte enregistrée",
                          "ÉDITEUR TACTILE", "Modifier 64 + 64 points sur l'écran 7 pouces",
                          "ÉDITEUR WEB", "Éditeur navigateur prêt", "En attente du Wi-Fi/Ethernet", "RETOUR"};
    static const Texts es{"MAPA PERSONALIZADO", "Probar o editar el mapa de cableado",
                          "PROBAR CON MAPA", "Escaneo A->B / B->A con el mapa guardado",
                          "EDITOR TÁCTIL", "Editar 64 + 64 puntos en la pantalla de 7 pulgadas",
                          "EDITOR WEB", "Editor del navegador listo", "Esperando Wi-Fi/Ethernet", "VOLVER"};
    static const Texts pl{"MAPA NIESTANDARDOWA", "Testuj lub edytuj niestandardową mapę połączeń",
                          "TEST Z MAPĄ", "Skan A->B / B->A z zapisaną mapą",
                          "EDYTOR DOTYKOWY", "Edytuj 64 + 64 punkty na ekranie 7 cali",
                          "EDYTOR WEB", "Edytor w przeglądarce gotowy", "Oczekiwanie na Wi-Fi/Ethernet", "WSTECZ"};
    switch (currentP4Language()) {
        case P4Language::English: return en;
        case P4Language::Dutch: return nl;
        case P4Language::German: return de;
        case P4Language::French: return fr;
        case P4Language::Spanish: return es;
        case P4Language::Polish: return pl;
        case P4Language::Turkish:
        default: return tr;
    }
}

void flat(lv_obj_t* o) {
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
}

lv_obj_t* label(lv_obj_t* p, const char* v, const lv_font_t* f, lv_color_t c) {
    lv_obj_t* l = lv_label_create(p);
    lv_label_set_text(l, v);
    lv_obj_set_style_text_font(l, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, c, LV_PART_MAIN);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}
}  // namespace

void CustomMapMenuScreen::begin(const String& webUrl, bool webAvailable) {
    pendingAction_ = CustomMapMenuAction::None;
    if (screen_ == nullptr) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    lv_scr_load(screen_);
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    const Texts& t = texts();
    lv_obj_t* h = lv_obj_create(screen_);
    lv_obj_set_pos(h, 0, 0);
    lv_obj_set_size(h, kDisplayWidth, 82);
    flat(h);
    lv_obj_set_style_bg_color(h, lv_color_hex(0x2E2240), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(h, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(h, 0, LV_PART_MAIN);
    lv_obj_t* title = label(h, t.title, p4Font20(), lv_color_hex(0xF4EFFF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_t* sub = label(h, t.subtitle, p4Font14(), lv_color_hex(0xC9B7E3));
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -10);
    soundButton_.create(h, 970, 18, 42);

    addButton(screen_, 50, 120, 430, t.test, t.testInfo,
              lv_color_hex(0x176B55), CustomMapMenuAction::Test, 0);
    addButton(screen_, 544, 120, 430, t.edit, t.editInfo,
              lv_color_hex(0x166B8F), CustomMapMenuAction::Edit, 1);

    lv_obj_t* webCard = lv_obj_create(screen_);
    lv_obj_set_pos(webCard, 50, 315);
    lv_obj_set_size(webCard, 924, 145);
    flat(webCard);
    lv_obj_set_style_bg_color(webCard, lv_color_hex(0x172631), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(webCard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(webCard, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(webCard, lv_color_hex(0x4D6878), LV_PART_MAIN);
    lv_obj_set_style_radius(webCard, 12, LV_PART_MAIN);
    lv_obj_t* webTitle = label(webCard, t.web, p4Font20(), lv_color_hex(0xEAF6FF));
    lv_obj_align(webTitle, LV_ALIGN_TOP_MID, 0, 16);
    webStatusLabel_ = label(webCard, "", p4Font14(), lv_color_hex(0xA8C8D7));
    lv_obj_set_width(webStatusLabel_, 880);
    lv_obj_set_style_text_align(webStatusLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(webStatusLabel_, LV_ALIGN_BOTTOM_MID, 0, -22);

    addButton(screen_, 50, 492, 220, t.back, "",
              lv_color_hex(0x6A4B8A), CustomMapMenuAction::Back, 2);
    refreshWebInfo(webUrl, webAvailable);
}

lv_obj_t* CustomMapMenuScreen::addButton(lv_obj_t* p,
                                         lv_coord_t x,
                                         lv_coord_t y,
                                         lv_coord_t width,
                                         const char* title,
                                         const char* subtitle,
                                         lv_color_t c,
                                         CustomMapMenuAction a,
                                         uint8_t i) {
    lv_obj_t* b = lv_btn_create(p);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, width, i == 2 ? 70 : 155);
    lv_obj_set_style_bg_color(b, c, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(b, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(b, lv_color_hex(0x75A7BD), LV_PART_MAIN);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
    bindings_[i] = {this, a};
    lv_obj_add_event_cb(b, callback, LV_EVENT_CLICKED, &bindings_[i]);

    lv_obj_t* tl = label(b, title, p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_set_width(tl, width - 30);
    lv_obj_set_style_text_align(tl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(tl, i == 2 ? LV_ALIGN_CENTER : LV_ALIGN_TOP_MID, 0, i == 2 ? 0 : 28);
    if (i != 2) {
        lv_obj_t* sl = label(b, subtitle, p4Font14(), lv_color_hex(0xD3E6EF));
        lv_obj_set_width(sl, width - 40);
        lv_obj_set_style_text_align(sl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_long_mode(sl, LV_LABEL_LONG_WRAP);
        lv_obj_align(sl, LV_ALIGN_BOTTOM_MID, 0, -25);
    }
    return b;
}

void CustomMapMenuScreen::refreshWebInfo(const String& webUrl, bool webAvailable) {
    if (webStatusLabel_ == nullptr) {
        return;
    }
    const Texts& t = texts();
    String value = webAvailable ? String(t.webReady) + "\n" + webUrl : String(t.webOffline);
    lv_label_set_text(webStatusLabel_, value.c_str());
    lv_obj_set_style_text_color(webStatusLabel_,
                                webAvailable ? lv_color_hex(0x86E8A0) : lv_color_hex(0xD9B36A),
                                LV_PART_MAIN);
}

void CustomMapMenuScreen::updateWebInfo(const String& webUrl, bool webAvailable) {
    refreshWebInfo(webUrl, webAvailable);
}

void CustomMapMenuScreen::activate() {
    pendingAction_ = CustomMapMenuAction::None;
    if (screen_ != nullptr && lv_scr_act() != screen_) {
        lv_scr_load(screen_);
    }
}

void CustomMapMenuScreen::callback(lv_event_t* e) {
    auto* b = static_cast<Binding*>(lv_event_get_user_data(e));
    if (b != nullptr && b->owner != nullptr) {
        b->owner->request(b->action);
    }
}

void CustomMapMenuScreen::request(CustomMapMenuAction action) {
    pendingAction_ = action;
}

CustomMapMenuAction CustomMapMenuScreen::consumeAction() {
    const CustomMapMenuAction action = pendingAction_;
    pendingAction_ = CustomMapMenuAction::None;
    return action;
}

}  // namespace mg::p4
