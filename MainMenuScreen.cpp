#include "MainMenuScreen.h"

#include <Arduino.h>
#include <stdio.h>

#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4LanguageVisuals.h"
#include "P4Localization.h"

namespace mg::p4 {

namespace {

lv_obj_t* addMenuText(lv_obj_t* parent,
                      const char* text,
                      const lv_font_t* font,
                      lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    // Touch regression guard: labels are presentation-only. They must never
    // steal a press from the menu button that owns them.
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

void flattenMenuObject(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

}  // namespace

void MainMenuScreen::begin() {
    pendingAction_ = MainMenuAction::None;
    if (screen_ == nullptr) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    lv_scr_load(screen_);
    flattenMenuObject(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 82);
    flattenMenuObject(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    const P4UiTexts& text = p4Texts();
    lv_obj_t* title = addMenuText(header,
                                  text.mainTitle,
                                  p4Font20(),
                                  lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_t* subtitle = addMenuText(
        header,
        text.mainSubtitle,
        p4Font14(),
        lv_color_hex(0x9CC6DC));
    lv_obj_align(subtitle, LV_ALIGN_BOTTOM_MID, 0, -10);

    struct MenuSpec {
        const char* title;
        const char* subtitle;
        lv_color_t color;
        MainMenuAction action;
        bool enabled;
    };

    const MenuSpec specs[9] = {
        {text.oneToOne, text.oneToOneInfo,
         lv_color_hex(0x147A4B), MainMenuAction::NormalScan, true},
        {text.subDTest, text.subDTestInfo,
         lv_color_hex(0x315B8A), MainMenuAction::SubDScan, true},
        {p4SelectText("KABLO ANALİZİ", "CABLE ANALYSIS", "KABELANALYSE", "KABELANALYSE", "ANALYSE DU CÂBLE", "ANÁLISIS DE CABLE", "ANALIZA KABLA"),
         p4SelectText("Direnç, krimp, kopuk yeri ve çift kontrolü", "Resistance, crimp, fault location and pair checks", "Weerstand, krimp, foutlocatie en paartests", "Widerstand, Crimp, Fehlerortung und Paarprüfung", "Résistance, sertissage, localisation de défaut et paires", "Resistencia, crimpado, localización de fallos y pares", "Rezystancja, zacisk, lokalizacja uszkodzeń i pary"),
         lv_color_hex(0x8A4F17), MainMenuAction::AdvancedAnalysis, true},
        {text.settings, text.settingsInfo,
         lv_color_hex(0x167A9A), MainMenuAction::Settings, true},
        {text.cableLearn,
         p4SelectText("Bağlantıları otomatik öğren ve net haritası oluştur", "Automatically learn connections and build an electrical-net map", "Leer verbindingen automatisch en maak een elektrisch netdiagram", "Verbindungen automatisch lernen und Netzplan erzeugen", "Apprendre automatiquement les connexions et créer la carte des nets", "Aprender conexiones automáticamente y crear el mapa de redes", "Automatycznie ucz połączeń i utwórz mapę sieci"),
         lv_color_hex(0x176B55), MainMenuAction::CableLearn, true},
        {text.qrBarcode,
         p4SelectText("PR / ASML / MG profil kodunu oku", "Read PR / ASML / MG profile codes", "Lees PR / ASML / MG-profielcodes", "PR-/ASML-/MG-Profilcode lesen", "Lire le code de profil PR / ASML / MG", "Leer código de perfil PR / ASML / MG", "Odczytaj kod profilu PR / ASML / MG"),
         lv_color_hex(0x6D4C7D), MainMenuAction::QrBarcode, true},
        {text.reports,
         p4SelectText("Test kayıtları, fikstür sağlığı ve SPC trendleri", "Test records, fixture health and SPC trends", "Testrecords, fixturestatus en SPC-trends", "Testaufzeichnungen, Fixture-Zustand und SPC-Trends", "Enregistrements, état du fixture et tendances SPC", "Registros, estado del fixture y tendencias SPC", "Rejestry testów, stan fixture i trendy SPC"),
         lv_color_hex(0x7A5B17), MainMenuAction::Reports, true},
        {text.multiConnector,
         p4SelectText("A-B / B-C / C-D segment bazlı test", "A-B / B-C / C-D segment-based testing", "Segmenttest A-B / B-C / C-D", "Segmenttest A-B / B-C / C-D", "Test par segments A-B / B-C / C-D", "Prueba por segmentos A-B / B-C / C-D", "Test segmentów A-B / B-C / C-D"),
         lv_color_hex(0x166E7A), MainMenuAction::MultiConnector, true},
        {p4SelectText("EKSTRA", "EXTRA", "EXTRA", "EXTRA", "EXTRA", "EXTRA", "DODATKOWE"),
         p4SelectText("Self-test, belge aktarımı, prob, SPC, ses ve USB-C", "Self-test, document import, probe, SPC, voice and USB-C", "Zelftest, documentimport, probe, SPC, spraak en USB-C", "Selbsttest, Dokumentimport, Probe, SPC, Sprache und USB-C", "Auto-test, import de documents, sonde, SPC, voix et USB-C", "Autotest, importación de documentos, sonda, SPC, voz y USB-C", "Autotest, import dokumentów, sonda, SPC, głos i USB-C"),
         lv_color_hex(0x5A496F), MainMenuAction::ExtraFeatures, true},
    };

    constexpr lv_coord_t firstX = 22;
    constexpr lv_coord_t firstY = 92;
    constexpr lv_coord_t columnPitch = 334;
    constexpr lv_coord_t rowPitch = 136;
    for (uint8_t index = 0; index < 9; ++index) {
        const lv_coord_t x = static_cast<lv_coord_t>(
            firstX + static_cast<lv_coord_t>(index % 3U) * columnPitch);
        const lv_coord_t y = static_cast<lv_coord_t>(
            firstY + static_cast<lv_coord_t>(index / 3U) * rowPitch);
        createMenuButton(screen_,
                         x,
                         y,
                         specs[index].title,
                         specs[index].subtitle,
                         specs[index].color,
                         specs[index].action,
                         specs[index].enabled,
                         index);
    }

    char languageTitle[64] = {};
    snprintf(languageTitle,
             sizeof(languageTitle),
             "%s: %s",
             text.language,
             p4LanguageName(currentP4Language()));
    lv_obj_t* languageButton = createMenuButton(screen_,
                                                22,
                                                504,
                                                languageTitle,
                                                text.selectLanguage,
                                                lv_color_hex(0x6A4B8A),
                                                MainMenuAction::Language,
                                                true,
                                                9);
    (void)languageButton;

    lv_obj_t* footer = addMenuText(
        screen_,
        text.mainStatus,
        p4Font10(),
        lv_color_hex(0xF1C96A));
    lv_obj_set_width(footer, 610);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(footer, 382, 526);

    soundButton_.create(header, 970, 18, 42);

    Serial.println("[MENU] Main menu ready; automatic scan disabled");
    Serial.flush();
}

lv_obj_t* MainMenuScreen::createMenuButton(lv_obj_t* parent,
                                            lv_coord_t x,
                                            lv_coord_t y,
                                            const char* title,
                                            const char* subtitle,
                                            lv_color_t color,
                                            MainMenuAction action,
                                            bool enabled,
                                            uint8_t bindingIndex) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    const bool compact = bindingIndex == 9U;
    lv_obj_set_size(button, compact ? 340 : 312, compact ? 72 : 118);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    lv_obj_set_style_border_color(button,
                                  enabled ? lv_color_hex(0x75A7BD)
                                          : lv_color_hex(0x52606A),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);

    if (compact) {
        // PERMANENT LANGUAGE TOUCH GUARD:
        // The language selector sits at the screen edge and must remain easy
        // to hit on the physical 7-inch panel. Do not remove these lines.
        lv_obj_set_ext_click_area(button, 8);
        lv_obj_add_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
    }

    if (enabled) {
        bindings_[bindingIndex].owner = this;
        bindings_[bindingIndex].action = action;
        lv_obj_add_event_cb(button,
                            menuCallback,
                            LV_EVENT_CLICKED,
                            &bindings_[bindingIndex]);
    } else {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    }

    if (compact) {
        // One centred row prevents the button theme padding from pushing the
        // icons down and keeps both icon gaps exactly eight pixels.
        lv_obj_t* row = lv_obj_create(button);
        flattenMenuObject(row);
        lv_obj_set_size(row, 326, 44);
        lv_obj_center(row);
        // lv_obj_create() is clickable by default. If this transparent row
        // stays clickable, presses on globe/text/flag are swallowed and only
        // the thin outer edge of the real button responds.
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);

        createGlobeIcon(row, 0, 0);
        addMenuText(row,
                    title,
                    p4Font14(),
                    enabled ? lv_color_hex(0xFFFFFF)
                            : lv_color_hex(0x9AA7AE));
        createLanguageFlag(row, currentP4Language(), 0, 0);
    } else {
        lv_obj_t* titleLabel = addMenuText(
            button,
            title,
            p4Font20(),
            enabled ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x9AA7AE));
        lv_obj_set_width(titleLabel, 292);
        lv_obj_set_style_text_align(titleLabel,
                                    LV_TEXT_ALIGN_CENTER,
                                    LV_PART_MAIN);
        lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 18);
    }

    if (!compact) {
        lv_obj_t* subtitleLabel = addMenuText(
            button,
            subtitle,
            p4Font14(),
            enabled ? lv_color_hex(0xD3E6EF) : lv_color_hex(0x7F8C94));
        lv_obj_set_width(subtitleLabel, 292);
        lv_obj_set_style_text_align(subtitleLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(subtitleLabel, LV_ALIGN_BOTTOM_MID, 0, -13);
    }
    return button;
}

void MainMenuScreen::menuCallback(lv_event_t* event) {
    auto* binding = static_cast<MenuBinding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->owner != nullptr) {
        binding->owner->request(binding->action);
    }
}

void MainMenuScreen::request(MainMenuAction action) {
    pendingAction_ = action;
    Serial.printf("[MENU] action=%u\n", static_cast<unsigned>(action));
    Serial.flush();
}

void MainMenuScreen::activate() {
    pendingAction_ = MainMenuAction::None;
    if (screen_ != nullptr && lv_scr_act() != screen_) {
        lv_scr_load(screen_);
    }
}

MainMenuAction MainMenuScreen::consumeAction() {
    const MainMenuAction action = pendingAction_;
    pendingAction_ = MainMenuAction::None;
    return action;
}

}  // namespace mg::p4
