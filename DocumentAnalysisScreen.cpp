#include "DocumentAnalysisScreen.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <new>

#include "ConfigP4.h"
#include "MapWebEditor.h"
#include "P4Fonts.h"
#include "P4Localization.h"
#include "PreparedProfileService.h"

namespace mg::p4 {
namespace {

void flat(lv_obj_t* object) {
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* makeLabel(lv_obj_t* parent,
                    const char* text,
                    const lv_font_t* font,
                    lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

lv_obj_t* makeButton(lv_obj_t* parent,
                     int x,
                     int width,
                     lv_color_t color,
                     const char* text,
                     void* callbackContext) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, 510);
    lv_obj_set_size(button, width, 68);
    lv_obj_set_style_bg_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 2, LV_PART_MAIN);
    lv_obj_add_event_cb(button,
                        DocumentAnalysisScreen::actionCallback,
                        LV_EVENT_CLICKED,
                        callbackContext);
    lv_obj_t* textLabel = makeLabel(button, text, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_set_width(textLabel, width - 8);
    lv_obj_set_style_text_align(textLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(textLabel, LV_LABEL_LONG_WRAP);
    lv_obj_center(textLabel);
    return button;
}

void formatPoint(uint8_t testIndex, char* output, size_t outputSize) {
    if (output == nullptr || outputSize == 0U) return;
    if (testIndex == kPeTestIndex) {
        snprintf(output, outputSize, "PE");
    } else if (testIndex == kDrainShieldTestIndex) {
        snprintf(output, outputSize, "dS");
    } else {
        snprintf(output, outputSize, "%u", static_cast<unsigned>(testIndex));
    }
}

bool isGenericPlaceholder(const char* value, const char* placeholder) {
    return value == nullptr || value[0] == '\0' ||
           (placeholder != nullptr && strcmp(value, placeholder) == 0);
}

void appendPointRef(String& out, const char* connector, uint8_t testIndex);

bool mapsEqualElectrical(const CableMap& a, const CableMap& b) {
    for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
        if (a.receiverMaskAtoB(i) != b.receiverMaskAtoB(i) ||
            a.sameSideMaskA(i) != b.sameSideMaskA(i) ||
            a.sameSideMaskB(i) != b.sameSideMaskB(i)) {
            return false;
        }
    }
    return true;
}

void appendMaskRefs(String& out,
                    uint64_t mask,
                    const char* connector,
                    uint64_t enabledMask) {
    bool wrote = false;
    for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
        if ((mask & enabledMask & (1ULL << i)) == 0ULL) continue;
        if (wrote) out += '+';
        appendPointRef(out, connector, i);
        wrote = true;
    }
    if (!wrote) out += '-';
}

void appendMapDiffSummary(String& out,
                          const CableMap& original,
                          const CableMap& current,
                          const CableProfile& profile,
                          const ProductionProfileRecord& record) {
    const char* aName = record.connectorAName[0] != '\0' ? record.connectorAName : "X1";
    const char* bName = record.connectorBName[0] != '\0' ? record.connectorBName : "X2";
    const uint64_t aMask = profile.sidePointMask(true);
    const uint64_t bMask = profile.sidePointMask(false);
    uint8_t shown = 0U;

    for (uint8_t a = 0U; a < kTestPointsPerSide && shown < 8U; ++a) {
        if ((aMask & (1ULL << a)) == 0ULL) continue;
        const uint64_t before = original.receiverMaskAtoB(a) & bMask;
        const uint64_t after = current.receiverMaskAtoB(a) & bMask;
        if (before == after) continue;
        appendPointRef(out, aName, a);
        out += ": ";
        appendMaskRefs(out, before, bName, bMask);
        out += " -> ";
        appendMaskRefs(out, after, bName, bMask);
        out += '\n';
        ++shown;
    }

    for (uint8_t i = 0U; i < kTestPointsPerSide && shown < 8U; ++i) {
        if ((aMask & (1ULL << i)) != 0ULL) {
            const uint64_t before = original.sameSideMaskA(i) & aMask;
            const uint64_t after = current.sameSideMaskA(i) & aMask;
            if (before != after) {
                appendPointRef(out, aName, i);
                out += " [A-A]: ";
                appendMaskRefs(out, before, aName, aMask);
                out += " -> ";
                appendMaskRefs(out, after, aName, aMask);
                out += '\n';
                ++shown;
            }
        }
        if ((bMask & (1ULL << i)) != 0ULL && shown < 8U) {
            const uint64_t before = original.sameSideMaskB(i) & bMask;
            const uint64_t after = current.sameSideMaskB(i) & bMask;
            if (before != after) {
                appendPointRef(out, bName, i);
                out += " [B-B]: ";
                appendMaskRefs(out, before, bName, bMask);
                out += " -> ";
                appendMaskRefs(out, after, bName, bMask);
                out += '\n';
                ++shown;
            }
        }
    }

    if (shown == 0U) {
        out += p4SelectText("Elektriksel fark yok.\n", "No electrical difference.\n",
                            "Geen elektrisch verschil.\n", "Kein elektrischer Unterschied.\n",
                            "Aucune différence électrique.\n", "Sin diferencia eléctrica.\n",
                            "Brak różnicy elektrycznej.\n");
    } else if (shown >= 8U) {
        out += p4SelectText("... diğer değişiklikler harita ekranında görülebilir.\n",
                            "... more changes are available on the map screen.\n",
                            "... meer wijzigingen staan op het kaartscherm.\n",
                            "... weitere Änderungen sind in der Kartenansicht sichtbar.\n",
                            "... d'autres modifications sont visibles sur l'écran de carte.\n",
                            "... hay más cambios en la pantalla del mapa.\n",
                            "... więcej zmian jest widocznych na ekranie mapy.\n");
    }
}

uint16_t expectedRelationCount(const CableMap& map, const CableProfile& profile) {
    uint16_t count = 0U;
    const uint64_t aMask = profile.sidePointMask(true);
    const uint64_t bMask = profile.sidePointMask(false);

    for (uint8_t a = 0U; a < kTestPointsPerSide; ++a) {
        if ((aMask & (1ULL << a)) == 0ULL) continue;
        uint64_t targets = map.receiverMaskAtoB(a) & bMask;
        while (targets != 0ULL) {
            count += static_cast<uint16_t>(targets & 1ULL);
            targets >>= 1U;
        }
    }

    for (uint8_t first = 0U; first < kTestPointsPerSide; ++first) {
        if ((aMask & (1ULL << first)) != 0ULL) {
            const uint64_t peers = map.sameSideMaskA(first) & aMask;
            for (uint8_t second = static_cast<uint8_t>(first + 1U);
                 second < kTestPointsPerSide;
                 ++second) {
                if ((peers & (1ULL << second)) != 0ULL) ++count;
            }
        }
        if ((bMask & (1ULL << first)) != 0ULL) {
            const uint64_t peers = map.sameSideMaskB(first) & bMask;
            for (uint8_t second = static_cast<uint8_t>(first + 1U);
                 second < kTestPointsPerSide;
                 ++second) {
                if ((peers & (1ULL << second)) != 0ULL) ++count;
            }
        }
    }
    return count;
}

void appendPointRef(String& out, const char* connector, uint8_t testIndex) {
    char point[8] = {};
    formatPoint(testIndex, point, sizeof(point));
    out += connector;
    out += ':';
    out += point;
}

void appendConnectionList(String& out,
                          const ProductionProfileRecord& record,
                          const CableProfile& profile,
                          const CableMap& map) {
    const char* aName = record.connectorAName[0] != '\0' ? record.connectorAName : "X1";
    const char* bName = record.connectorBName[0] != '\0' ? record.connectorBName : "X2";
    const uint64_t aMask = profile.sidePointMask(true);
    const uint64_t bMask = profile.sidePointMask(false);

    out += "\n";
    out += p4SelectText("Bağlantılar", "Connections", "Verbindingen", "Verbindungen",
                        "Connexions", "Conexiones", "Połączenia");
    out += ":\n";

    for (uint8_t a = 0U; a < kTestPointsPerSide; ++a) {
        if ((aMask & (1ULL << a)) == 0ULL) continue;
        const uint64_t targets = map.receiverMaskAtoB(a) & bMask;
        const uint64_t sameSide = map.sameSideMaskA(a) & aMask;
        if (targets == 0ULL && sameSide == 0ULL) {
            appendPointRef(out, aName, a);
            out += " -> NC\n";
            continue;
        }
        if (targets != 0ULL) {
            appendPointRef(out, aName, a);
            out += " -> ";
            bool firstTarget = true;
            for (uint8_t b = 0U; b < kTestPointsPerSide; ++b) {
                if ((targets & (1ULL << b)) == 0ULL) continue;
                if (!firstTarget) out += ", ";
                appendPointRef(out, bName, b);
                firstTarget = false;
            }
            out += '\n';
        }
    }

    // Show B-side NC points that are not referenced by any A point.
    for (uint8_t b = 0U; b < kTestPointsPerSide; ++b) {
        if ((bMask & (1ULL << b)) == 0ULL) continue;
        const uint64_t sources = map.receiverMaskBtoA(b) & aMask;
        const uint64_t sameSide = map.sameSideMaskB(b) & bMask;
        if (sources == 0ULL && sameSide == 0ULL) {
            appendPointRef(out, bName, b);
            out += " -> NC\n";
        }
    }

    // Same-side splice/common-net relations are listed once per pair.
    for (uint8_t first = 0U; first < kTestPointsPerSide; ++first) {
        if ((aMask & (1ULL << first)) != 0ULL) {
            const uint64_t peers = map.sameSideMaskA(first) & aMask;
            for (uint8_t second = static_cast<uint8_t>(first + 1U);
                 second < kTestPointsPerSide;
                 ++second) {
                if ((peers & (1ULL << second)) == 0ULL) continue;
                appendPointRef(out, aName, first);
                out += " <-> ";
                appendPointRef(out, aName, second);
                out += '\n';
            }
        }
        if ((bMask & (1ULL << first)) != 0ULL) {
            const uint64_t peers = map.sameSideMaskB(first) & bMask;
            for (uint8_t second = static_cast<uint8_t>(first + 1U);
                 second < kTestPointsPerSide;
                 ++second) {
                if ((peers & (1ULL << second)) == 0ULL) continue;
                appendPointRef(out, bName, first);
                out += " <-> ";
                appendPointRef(out, bName, second);
                out += '\n';
            }
        }
    }
}

String operatorFailureText(const String& technical) {
    if (technical.indexOf("production PR") >= 0 || technical.indexOf("PR") >= 0) {
        return String(p4SelectText(
            "Üretim numarası doğrulanamadı. Telefonda profil bilgisini kontrol edip yeniden gönderin.",
            "The production number could not be verified. Check the profile on the phone and send it again.",
            "Het productienummer kon niet worden gecontroleerd. Controleer het profiel op de telefoon en stuur opnieuw.",
            "Die Produktionsnummer konnte nicht geprüft werden. Profil am Telefon prüfen und erneut senden.",
            "Le numéro de production n'a pas pu être vérifié. Vérifiez le profil sur le téléphone et renvoyez-le.",
            "No se pudo verificar el número de producción. Revise el perfil en el teléfono y vuelva a enviarlo.",
            "Nie udało się zweryfikować numeru produkcyjnego. Sprawdź profil w telefonie i wyślij ponownie."));
    }
    if (technical.indexOf("CRC") >= 0 || technical.indexOf("JSON") >= 0 ||
        technical.indexOf("file") >= 0 || technical.indexOf("File") >= 0 ||
        technical.indexOf("size") >= 0) {
        return String(p4SelectText(
            "Alınan profil doğrulanamadı. Geri dönüp profili yeniden gönderin.",
            "The received profile could not be verified. Go back and send the profile again.",
            "Het ontvangen profiel kon niet worden gecontroleerd. Ga terug en stuur het profiel opnieuw.",
            "Das empfangene Profil konnte nicht geprüft werden. Zurückgehen und erneut senden.",
            "Le profil reçu n'a pas pu être vérifié. Revenez en arrière et renvoyez-le.",
            "No se pudo verificar el perfil recibido. Vuelva atrás y envíelo de nuevo.",
            "Nie udało się zweryfikować odebranego profilu. Wróć i wyślij go ponownie."));
    }
    return String(p4SelectText(
        "Profil bilgileri uygun değil. Telefonda profili kontrol edip yeniden gönderin.",
        "The profile information is not valid. Check it on the phone and send it again.",
        "De profielgegevens zijn niet geldig. Controleer ze op de telefoon en stuur opnieuw.",
        "Die Profilinformationen sind ungültig. Am Telefon prüfen und erneut senden.",
        "Les informations du profil ne sont pas valides. Vérifiez-les sur le téléphone et renvoyez-les.",
        "La información del perfil no es válida. Revísela en el teléfono y vuelva a enviarla.",
        "Dane profilu są nieprawidłowe. Sprawdź je w telefonie i wyślij ponownie."));
}

}  // namespace

void DocumentAnalysisScreen::begin(MapWebEditor& portal,
                                   DocumentImportValidator& validator) {
    portal_ = &portal;
    validator_ = &validator;
    action_ = DocumentAnalysisAction::None;
    importAttempted_ = false;
    profileReady_ = false;
    operatorEdited_ = false;
    operatorEditConfirmed_ = false;
    editValidationError_ = String();
    record_ = {};
    originalRecord_ = {};
    draftMap_.clear();
    originalMap_.clear();
    draftProfile_ = makeNormalProfile();
    message_ = String();
    validationReport_ = validator_->validate(portal);

    if (!screen_) screen_ = lv_obj_create(nullptr);
    else lv_obj_clean(screen_);
    lv_scr_load(screen_);
    buildUi();

    // The operator no longer walks through a separate file/CRC
    // screen and then presses READ PROFILE. The same fail-closed validation is
    // executed automatically when this single summary screen opens.
    if (validationReport_.ok) {
        importProfile();
    } else {
        refresh();
    }
}

void DocumentAnalysisScreen::activate() {
    if (screen_) lv_scr_load(screen_);
    refresh();
}

void DocumentAnalysisScreen::buildUi() {
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 78);
    flat(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    lv_obj_t* title = makeLabel(
        header,
        p4SelectText("ALINAN PROFİL", "RECEIVED PROFILE", "ONTVANGEN PROFIEL", "EMPFANGENES PROFIL",
                     "PROFIL REÇU", "PERFIL RECIBIDO", "ODEBRANY PROFIL"),
        p4Font20(), lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_t* subtitle = makeLabel(
        header,
        p4SelectText("Telefon profili otomatik kontrol edilir; yalnız sonucu onaylayın.",
                     "The phone profile is checked automatically; only confirm the result.",
                     "Het telefoonprofiel wordt automatisch gecontroleerd; bevestig alleen het resultaat.",
                     "Das Telefonprofil wird automatisch geprüft; nur das Ergebnis bestätigen.",
                     "Le profil du téléphone est contrôlé automatiquement ; confirmez seulement le résultat.",
                     "El perfil del teléfono se comprueba automáticamente; confirme solo el resultado.",
                     "Profil z telefonu jest sprawdzany automatycznie; potwierdź tylko wynik."),
        p4Font14(), lv_color_hex(0x9CC6DC));
    lv_obj_align(subtitle, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_obj_t* panel = lv_obj_create(screen_);
    lv_obj_set_pos(panel, 24, 92);
    lv_obj_set_size(panel, 972, 400);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x10202A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x355565), LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 18, LV_PART_MAIN);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);

    statusLabel_ = makeLabel(panel, "", p4Font20(), lv_color_hex(0xF1C96A));
    lv_obj_set_pos(statusLabel_, 8, 6);
    lv_obj_set_width(statusLabel_, 900);

    detailLabel_ = makeLabel(panel, "", p4Font14(), lv_color_hex(0xD7EAF4));
    lv_obj_set_pos(detailLabel_, 8, 54);
    lv_obj_set_width(detailLabel_, 900);
    lv_obj_set_height(detailLabel_, LV_SIZE_CONTENT);
    lv_label_set_long_mode(detailLabel_, LV_LABEL_LONG_WRAP);

    actionContexts_[0] = {this, UiAction::ReturnToReview};
    actionContexts_[1] = {this, UiAction::ImportProfile};  // retained for compatibility; no normal-flow button
    actionContexts_[2] = {this, UiAction::EditDraftMap};
    actionContexts_[3] = {this, UiAction::ApplyProfile};
    actionContexts_[4] = {this, UiAction::RestoreOriginal};
    actionContexts_[5] = {this, UiAction::BackToExtra};

    makeButton(screen_, 24, 190, lv_color_hex(0x176B8F),
               p4SelectText("GERİ", "BACK", "TERUG", "ZURÜCK", "RETOUR", "VOLVER", "WSTECZ"),
               &actionContexts_[0]);
    editButton_ = makeButton(
        screen_, 228, 250, lv_color_hex(0x7A5B20),
        p4SelectText("HARİTAYI GÖR / DÜZENLE", "VIEW / EDIT MAP", "KAART BEKIJKEN / BEWERKEN",
                     "KARTE ANZEIGEN / BEARBEITEN", "VOIR / MODIFIER LA CARTE",
                     "VER / EDITAR MAPA", "POKAŻ / EDYTUJ MAPĘ"),
        &actionContexts_[2]);
    restoreButton_ = makeButton(
        screen_, 438, 200, lv_color_hex(0x6A4B55),
        p4SelectText("ORİJİNALE DÖN", "RESTORE ORIGINAL", "ORIGINEEL HERSTELLEN",
                     "ORIGINAL WIEDERHERSTELLEN", "RESTAURER L'ORIGINAL",
                     "RESTAURAR ORIGINAL", "PRZYWRÓĆ ORYGINAŁ"),
        &actionContexts_[4]);
    lv_obj_add_flag(restoreButton_, LV_OBJ_FLAG_HIDDEN);
    applyButton_ = makeButton(
        screen_, 492, 504, lv_color_hex(0x2F7443),
        p4SelectText("TESTE HAZIRLA", "PREPARE FOR TEST", "KLAAR VOOR TEST", "FÜR TEST VORBEREITEN",
                     "PRÉPARER LE TEST", "PREPARAR PARA PRUEBA", "PRZYGOTUJ DO TESTU"),
        &actionContexts_[3]);
}

void DocumentAnalysisScreen::refresh() {
    if (!statusLabel_ || !detailLabel_) return;

    if (!validationReport_.ok) {
        lv_label_set_text(statusLabel_,
                          p4SelectText("PROFİL KONTROL EDİLEMEDİ", "PROFILE COULD NOT BE CHECKED",
                                       "PROFIEL KON NIET WORDEN GECONTROLEERD", "PROFIL KONNTE NICHT GEPRÜFT WERDEN",
                                       "LE PROFIL N'A PAS PU ÊTRE CONTRÔLÉ", "NO SE PUDO COMPROBAR EL PERFIL",
                                       "NIE UDAŁO SIĘ SPRAWDZIĆ PROFILU"));
        const String friendly = operatorFailureText(validationReport_.message);
        lv_label_set_text(detailLabel_, friendly.c_str());
    } else if (!importAttempted_) {
        lv_label_set_text(statusLabel_, p4SelectText("PROFİL KONTROL EDİLİYOR", "CHECKING PROFILE",
                                                     "PROFIEL CONTROLEREN", "PROFIL WIRD GEPRÜFT",
                                                     "CONTRÔLE DU PROFIL", "COMPROBANDO PERFIL",
                                                     "SPRAWDZANIE PROFILU"));
        lv_label_set_text(detailLabel_, p4SelectText("Lütfen bekleyin...", "Please wait...", "Even wachten...",
                                                     "Bitte warten...", "Veuillez patienter...", "Espere...", "Proszę czekać..."));
    } else if (!profileReady_) {
        lv_label_set_text(statusLabel_, p4SelectText("PROFİL UYGUN DEĞİL", "PROFILE NOT ACCEPTED",
                                                     "PROFIEL NIET GOEDGEKEURD", "PROFIL NICHT GEEIGNET",
                                                     "PROFIL NON ACCEPTÉ", "PERFIL NO ACEPTADO",
                                                     "PROFIL NIEZAAKCEPTOWANY"));
        const String friendly = operatorFailureText(message_);
        lv_label_set_text(detailLabel_, friendly.c_str());
    } else if (!editValidationError_.isEmpty()) {
        lv_label_set_text(statusLabel_,
                          p4SelectText("HARİTA DEĞİŞİKLİĞİ KABUL EDİLMEDİ", "MAP CHANGE NOT ACCEPTED",
                                       "KAARTWIJZIGING NIET GEACCEPTEERD", "KARTENÄNDERUNG NICHT AKZEPTIERT",
                                       "MODIFICATION DE CARTE REFUSÉE", "CAMBIO DE MAPA NO ACEPTADO",
                                       "ZMIANA MAPY NIEZAAKCEPTOWANA"));
        String friendly = p4SelectText(
            "Son geçerli harita korunuyor. Haritayı yeniden açıp düzeltin veya değişikliği iptal edip geri dönün.\n",
            "The last valid map is preserved. Reopen the map and correct it, or cancel the change and go back.\n",
            "De laatste geldige kaart blijft behouden. Open de kaart opnieuw en corrigeer deze, of annuleer de wijziging.\n",
            "Die letzte gültige Karte bleibt erhalten. Karte erneut öffnen und korrigieren oder Änderung verwerfen.\n",
            "La dernière carte valide est conservée. Rouvrez la carte pour la corriger ou annulez la modification.\n",
            "Se conserva el último mapa válido. Abra de nuevo el mapa para corregirlo o cancele el cambio.\n",
            "Ostatnia poprawna mapa została zachowana. Otwórz mapę ponownie i popraw ją albo anuluj zmianę.\n");
        friendly += operatorFailureText(editValidationError_);
        lv_label_set_text(detailLabel_, friendly.c_str());
    } else {
        if (operatorEdited_ && !operatorEditConfirmed_) {
            lv_label_set_text(statusLabel_,
                              p4SelectText("HARİTA DÜZENLENDİ - ONAY GEREKLİ", "MAP EDITED - CONFIRMATION REQUIRED",
                                           "KAART BEWERKT - BEVESTIGING VEREIST", "KARTE BEARBEITET - BESTÄTIGUNG ERFORDERLICH",
                                           "CARTE MODIFIÉE - CONFIRMATION REQUISE", "MAPA EDITADO - SE REQUIERE CONFIRMACIÓN",
                                           "MAPA EDYTOWANA - WYMAGANE POTWIERDZENIE"));
        } else if (operatorEdited_) {
            lv_label_set_text(statusLabel_,
                              p4SelectText("PROFİL HAZIR - HARİTA DEĞİŞİKLİĞİ ONAYLANDI", "PROFILE READY - MAP CHANGE CONFIRMED",
                                           "PROFIEL GEREED - KAARTWIJZIGING BEVESTIGD", "PROFIL BEREIT - KARTENÄNDERUNG BESTÄTIGT",
                                           "PROFIL PRÊT - MODIFICATION DE CARTE CONFIRMÉE", "PERFIL LISTO - CAMBIO DE MAPA CONFIRMADO",
                                           "PROFIL GOTOWY - ZMIANA MAPY POTWIERDZONA"));
        } else {
            lv_label_set_text(statusLabel_,
                              p4SelectText("PROFİL HAZIR - KONTROLLER TAMAM", "PROFILE READY - CHECKS COMPLETE",
                                           "PROFIEL GEREED - CONTROLES VOLTOOID", "PROFIL BEREIT - PRÜFUNGEN ABGESCHLOSSEN",
                                           "PROFIL PRÊT - CONTRÔLES TERMINÉS", "PERFIL LISTO - COMPROBACIONES COMPLETAS",
                                           "PROFIL GOTOWY - KONTROLE ZAKOŃCZONE"));
        }

        String detail;
        detail.reserve(7000U);
        detail += p4SelectText("Kaynak: Telefon / Bluetooth\nHedef: MG Smart Tester Cihazı\n",
                               "Source: Phone / Bluetooth\nTarget: MG Smart Tester\n",
                               "Bron: Telefoon / Bluetooth\nDoel: MG Smart Tester\n",
                               "Quelle: Telefon / Bluetooth\nZiel: MG Smart Tester\n",
                               "Source : Téléphone / Bluetooth\nCible : MG Smart Tester\n",
                               "Origen: Teléfono / Bluetooth\nDestino: MG Smart Tester\n",
                               "Źródło: Telefon / Bluetooth\nCel: MG Smart Tester\n");
        detail += p4SelectText("Üretim no: ", "Production no: ", "Productienr.: ", "Produktionsnr.: ",
                               "N° production : ", "N.º producción: ", "Nr produkcyjny: ");
        detail += record_.productionPr;
        detail += '\n';

        if (!isGenericPlaceholder(record_.customerReference, "MOBILE-IMPORT")) {
            detail += p4SelectText("Müşteri/Ref: ", "Customer/Ref: ", "Klant/Ref: ", "Kunde/Ref: ",
                                   "Client/Réf : ", "Cliente/Ref: ", "Klient/Ref: ");
            detail += record_.customerReference;
            detail += '\n';
        }
        if (!isGenericPlaceholder(record_.revision, "UNSPECIFIED")) {
            detail += p4SelectText("Revizyon: ", "Revision: ", "Revisie: ", "Revision: ",
                                   "Révision : ", "Revisión: ", "Rewizja: ");
            detail += record_.revision;
            detail += '\n';
        }

        detail += p4SelectText("Profil: ", "Profile: ", "Profiel: ", "Profil: ", "Profil : ", "Perfil: ", "Profil: ");
        detail += record_.profileId;
        detail += '\n';

        detail += p4SelectText("Eşleme: ", "Mapping: ", "Toewijzing: ", "Zuordnung: ", "Affectation : ", "Asignación: ", "Mapowanie: ");
        detail += record_.connectorAName[0] != '\0' ? record_.connectorAName : "X1";
        detail += " -> A   |   ";
        detail += record_.connectorBName[0] != '\0' ? record_.connectorBName : "X2";
        detail += " -> B\n";

        detail += p4SelectText("Sinyal pinleri: ", "Signal pins: ", "Signaalpinnen: ", "Signalpins: ",
                               "Broches signal : ", "Pines de señal: ", "Piny sygnałowe: ");
        detail += String(static_cast<unsigned>(record_.signalPinCount));
        detail += '\n';

        detail += "PE A/B: ";
        detail += record_.includePeA ? "ON" : "OFF";
        detail += '/';
        detail += record_.includePeB ? "ON" : "OFF";
        detail += "   dS A/B: ";
        detail += record_.includeDrainShieldA ? "ON" : "OFF";
        detail += '/';
        detail += record_.includeDrainShieldB ? "ON" : "OFF";
        detail += '\n';

        detail += p4SelectText("Bağlantı tanımı: ", "Connection definitions: ", "Verbindingsdefinities: ",
                               "Verbindungsdefinitionen: ", "Définitions de connexion : ",
                               "Definiciones de conexión: ", "Definicje połączeń: ");
        detail += String(static_cast<unsigned>(expectedRelationCount(draftMap_, draftProfile_)));
        detail += '\n';

        if (operatorEdited_) {
            detail += operatorEditConfirmed_
                ? p4SelectText("Harita operatör tarafından değiştirildi ve değişiklik ayrıca onaylandı.\n",
                               "The map was changed by the operator and the change was explicitly confirmed.\n",
                               "De kaart is door de operator gewijzigd en de wijziging is expliciet bevestigd.\n",
                               "Die Karte wurde vom Bediener geändert und die Änderung ausdrücklich bestätigt.\n",
                               "La carte a été modifiée par l'opérateur et la modification a été confirmée explicitement.\n",
                               "El mapa fue modificado por el operador y el cambio fue confirmado explícitamente.\n",
                               "Mapa została zmieniona przez operatora, a zmiana została wyraźnie potwierdzona.\n")
                : p4SelectText("Harita operatör tarafından değiştirildi. TESTE HAZIRLA'dan önce değişikliği onaylayın.\n",
                               "The map was changed by the operator. Confirm the change before preparing the test.\n",
                               "De kaart is door de operator gewijzigd. Bevestig de wijziging vóór het voorbereiden van de test.\n",
                               "Die Karte wurde vom Bediener geändert. Änderung vor der Testvorbereitung bestätigen.\n",
                               "La carte a été modifiée par l'opérateur. Confirmez la modification avant de préparer le test.\n",
                               "El mapa fue modificado por el operador. Confirme el cambio antes de preparar la prueba.\n",
                               "Mapa została zmieniona przez operatora. Potwierdź zmianę przed przygotowaniem testu.\n");
            detail += p4SelectText("Değişiklik özeti:\n", "Change summary:\n", "Samenvatting wijzigingen:\n",
                                   "Änderungsübersicht:\n", "Résumé des modifications :\n",
                                   "Resumen de cambios:\n", "Podsumowanie zmian:\n");
            appendMapDiffSummary(detail, originalMap_, draftMap_, draftProfile_, record_);
        } else {
            detail += p4SelectText("Harita telefondan alındı ve cihaz tarafından doğrulandı.\n",
                                   "The map was received from the phone and validated by the device.\n",
                                   "De kaart is van de telefoon ontvangen en door het apparaat gevalideerd.\n",
                                   "Die Karte wurde vom Telefon empfangen und vom Gerät validiert.\n",
                                   "La carte a été reçue du téléphone et validée par l'appareil.\n",
                                   "El mapa se recibió del teléfono y fue validado por el dispositivo.\n",
                                   "Mapa została odebrana z telefonu i zweryfikowana przez urządzenie.\n");
        }

        appendConnectionList(detail, record_, draftProfile_, draftMap_);
        lv_label_set_text(detailLabel_, detail.c_str());
    }

    if (importButton_) {
        lv_obj_add_state(importButton_, LV_STATE_DISABLED);
    }
    if (editButton_) {
        if (!profileReady_) lv_obj_add_state(editButton_, LV_STATE_DISABLED);
        else lv_obj_clear_state(editButton_, LV_STATE_DISABLED);
    }
    if (editButton_ && restoreButton_ && applyButton_) {
        if (operatorEdited_) {
            lv_obj_set_pos(editButton_, 228, 510);
            lv_obj_set_size(editButton_, 200, 68);
            lv_obj_clear_flag(restoreButton_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(restoreButton_, 438, 510);
            lv_obj_set_size(restoreButton_, 200, 68);
            lv_obj_set_pos(applyButton_, 648, 510);
            lv_obj_set_size(applyButton_, 348, 68);
            if (lv_obj_t* label = lv_obj_get_child(editButton_, 0)) lv_obj_set_width(label, 192);
            if (lv_obj_t* label = lv_obj_get_child(restoreButton_, 0)) lv_obj_set_width(label, 192);
            if (lv_obj_t* label = lv_obj_get_child(applyButton_, 0)) lv_obj_set_width(label, 340);
        } else {
            lv_obj_set_pos(editButton_, 228, 510);
            lv_obj_set_size(editButton_, 250, 68);
            lv_obj_add_flag(restoreButton_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(applyButton_, 492, 510);
            lv_obj_set_size(applyButton_, 504, 68);
            if (lv_obj_t* label = lv_obj_get_child(editButton_, 0)) lv_obj_set_width(label, 242);
            if (lv_obj_t* label = lv_obj_get_child(applyButton_, 0)) lv_obj_set_width(label, 496);
        }
    }
    if (applyButton_) {
        if (!profileReady_ || !editValidationError_.isEmpty()) lv_obj_add_state(applyButton_, LV_STATE_DISABLED);
        else lv_obj_clear_state(applyButton_, LV_STATE_DISABLED);
        lv_obj_t* applyLabel = lv_obj_get_child(applyButton_, 0);
        if (applyLabel != nullptr) {
            lv_label_set_text(applyLabel,
                (operatorEdited_ && !operatorEditConfirmed_)
                    ? p4SelectText("DEĞİŞİKLİĞİ ONAYLA", "CONFIRM CHANGE", "WIJZIGING BEVESTIGEN",
                                   "ÄNDERUNG BESTÄTIGEN", "CONFIRMER LA MODIFICATION",
                                   "CONFIRMAR CAMBIO", "POTWIERDŹ ZMIANĘ")
                    : p4SelectText("TESTE HAZIRLA", "PREPARE FOR TEST", "KLAAR VOOR TEST",
                                   "FÜR TEST VORBEREITEN", "PRÉPARER LE TEST",
                                   "PREPARAR PARA PRUEBA", "PRZYGOTUJ DO TESTU"));
        }
    }
}

void DocumentAnalysisScreen::importProfile() {
    importAttempted_ = true;
    profileReady_ = false;
    operatorEdited_ = false;
    operatorEditConfirmed_ = false;
    record_ = {};
    originalRecord_ = {};

    if (!portal_ || !validator_) {
        message_ = "Profile import service unavailable";
        refresh();
        return;
    }
    if (!PreparedProfileService::loadAndValidate(*portal_, *validator_,
                                                 record_, draftProfile_, draftMap_,
                                                 message_, &validationReport_)) {
        refresh();
        return;
    }

    originalRecord_ = record_;
    originalMap_ = draftMap_;
    profileReady_ = true;
    Serial.printf("[MOBILE-PROFILE] READY id=%s pr=%s pins=%u\n",
                  record_.profileId, record_.productionPr,
                  static_cast<unsigned>(record_.signalPinCount));
    Serial.flush();
    refresh();
}

bool DocumentAnalysisScreen::acceptOperatorEditedMap(const CableMap& map) {
    if (!profileReady_) return false;

    // An operator edit is never trusted just because it came from
    // the local touch editor. Rebuild the electrical arrays in a heap-backed
    // record and run the same production-profile contract before accepting it.
    std::unique_ptr<ProductionProfileRecord> candidate(
        new (std::nothrow) ProductionProfileRecord(record_));
    if (!candidate) {
        editValidationError_ = "Edited map validation memory unavailable";
        refresh();
        return false;
    }

    for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
        candidate->aToB[i] = map.receiverMaskAtoB(i);
        candidate->sameSideA[i] = map.sameSideMaskA(i);
        candidate->sameSideB[i] = map.sameSideMaskB(i);
    }

    const ProductionProfileValidation validation =
        ProductionProfileContract::validate(*candidate);
    if (!validation.valid) {
        editValidationError_ = String("Edited map validation failed: ") + validation.message;
        Serial.printf("[MOBILE-PROFILE] operator map rejected: %s\n", validation.message);
        Serial.flush();
        refresh();
        return false;
    }

    record_ = *candidate;
    draftMap_ = map;
    operatorEdited_ = !mapsEqualElectrical(originalMap_, draftMap_);
    operatorEditConfirmed_ = false;
    editValidationError_ = String();
    message_ = operatorEdited_
        ? "Operator-edited map revalidated; explicit change confirmation required"
        : "Operator map matches the original phone profile";
    Serial.printf("[MOBILE-PROFILE] operator map revalidated=OK changed=%s\n",
                  operatorEdited_ ? "YES" : "NO");
    Serial.flush();
    refresh();
    return true;
}

bool DocumentAnalysisScreen::restoreOriginalMap() {
    if (!profileReady_) return false;
    record_ = originalRecord_;
    draftMap_ = originalMap_;
    operatorEdited_ = false;
    operatorEditConfirmed_ = false;
    editValidationError_ = String();
    message_ = "Original phone map restored";
    Serial.println("[MOBILE-PROFILE] original phone map restored");
    Serial.flush();
    refresh();
    return true;
}

void DocumentAnalysisScreen::handleUiAction(UiAction action) {
    switch (action) {
        case UiAction::ImportProfile:
            // This action remains for compatibility, but normal
            // operator flow validates automatically on screen entry.
            importProfile();
            break;
        case UiAction::EditDraftMap:
            if (profileReady_) {
                // Reopening the editor means the operator is choosing to either
                // correct the rejected candidate or keep the last valid map.
                editValidationError_ = String();
                action_ = DocumentAnalysisAction::EditDraftMap;
            }
            break;
        case UiAction::ReturnToReview:
            action_ = DocumentAnalysisAction::ReturnToReview;
            break;
        case UiAction::BackToExtra:
            action_ = DocumentAnalysisAction::BackToExtra;
            break;
        case UiAction::ApplyProfile:
            if (profileReady_ && editValidationError_.isEmpty()) {
                if (operatorEdited_ && !operatorEditConfirmed_) {
                    operatorEditConfirmed_ = true;
                    message_ = "Operator explicitly confirmed the edited map";
                    Serial.println("[MOBILE-PROFILE] operator map change confirmation=OK");
                    Serial.flush();
                    refresh();
                } else {
                    action_ = DocumentAnalysisAction::ApplyProfile;
                }
            }
            break;
        case UiAction::RestoreOriginal:
            (void)restoreOriginalMap();
            break;
    }
}

void DocumentAnalysisScreen::actionCallback(lv_event_t* event) {
    auto* context = static_cast<ActionContext*>(lv_event_get_user_data(event));
    if (!context || !context->self) return;
    context->self->handleUiAction(context->action);
}

DocumentAnalysisAction DocumentAnalysisScreen::consumeAction() {
    const DocumentAnalysisAction value = action_;
    action_ = DocumentAnalysisAction::None;
    return value;
}

}  // namespace mg::p4
