#include "DocumentImportScreen.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "ConfigP4.h"
#include "DocumentUiLocalization.h"
#include "MapWebEditor.h"
#include "MgNetworkManager.h"
#include "MgBleProfileServer.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {

void flat(lv_obj_t* object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
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

}  // namespace

void DocumentImportScreen::begin(MapWebEditor& portal, MgNetworkManager& network, MgBleProfileServer& ble) {
    portal_ = &portal;
    network_ = &network;
    ble_ = &ble;
    backRequested_ = false;
    reviewRequested_ = false;
    autoOpenedForCurrentProfile_ = false;
    wifiFallbackRequested_ = false;
    lastUploadCount_ = 0xFFU;
    lastRefreshMs_ = 0U;

    if (!screen_) {
        screen_ = lv_obj_create(nullptr);
    } else {
        lv_obj_clean(screen_);
    }
    lv_scr_load(screen_);
    buildUi();

    // BLE is the normal transfer path. Do not start the Wi-Fi
    // AP/captive portal unless the operator explicitly opens the fallback.
    // A new import session still clears the previous prepared profile so BLE
    // starts from a clean, fail-closed store.
    portal_->stopDocumentPortal();
    const bool cleared = portal_->clearDocumentItems();
    const bool bleStarted = ble_->openSession();
    Serial.printf("[MOBILE-PROFILE] Wi-Fi fallback=OFF profile-clear=%s BLE session=%s\n",
                  cleared ? "OK" : "FAILED", bleStarted ? "OK" : "FAILED");
    Serial.flush();
    refreshWifiFallbackUi();
    refreshQr();
    refreshLabels();
}

void DocumentImportScreen::buildUi() {
    flat(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x081119), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* header = lv_obj_create(screen_);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, kDisplayWidth, 78);
    flat(header);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x142B3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    lv_obj_t* title = makeLabel(header,
                                p4SelectText("TELEFONDAN PROFİL AKTAR", "PHONE PROFILE IMPORT", "PROFIEL VAN TELEFOON", "PROFIL VOM TELEFON", "IMPORT DE PROFIL TÉLÉPHONE", "IMPORTAR PERFIL DEL TELÉFONO", "IMPORT PROFILU Z TELEFONU"),
                                p4Font20(),
                                lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_t* subtitle = makeLabel(
        header,
        p4SelectText("Bluetooth ana aktarım | Wi-Fi yedek", "Bluetooth primary transfer | Wi-Fi fallback", "Bluetooth primair | Wi-Fi reserve", "Bluetooth primär | Wi-Fi Ersatz", "Bluetooth principal | Wi-Fi secours", "Bluetooth principal | Wi-Fi de respaldo", "Bluetooth główny | Wi-Fi zapasowe"),
        p4Font14(),
        lv_color_hex(0x9CC6DC));
    lv_obj_align(subtitle, LV_ALIGN_BOTTOM_MID, 0, -8);

    qrPanel_ = lv_obj_create(screen_);
    lv_obj_set_pos(qrPanel_, 24, 94);
    lv_obj_set_size(qrPanel_, 272, 352);
    flat(qrPanel_);
    lv_obj_set_style_bg_color(qrPanel_, lv_color_hex(0x102431), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(qrPanel_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(qrPanel_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(qrPanel_, lv_color_hex(0x4C7A91), LV_PART_MAIN);
    lv_obj_set_style_radius(qrPanel_, 14, LV_PART_MAIN);

#if LV_USE_QRCODE
    qr_ = lv_qrcode_create(qrPanel_, 190, lv_color_hex(0x071018), lv_color_hex(0xFFFFFF));
    lv_obj_align(qr_, LV_ALIGN_TOP_MID, 0, 12);
#endif
    qrInfoLabel_ = makeLabel(
        qrPanel_,
        "",
        p4Font14(),
        lv_color_hex(0xD7EAF4));
    lv_obj_set_width(qrInfoLabel_, 238);
    lv_label_set_long_mode(qrInfoLabel_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(qrInfoLabel_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    wifiFallbackButton_ = lv_btn_create(qrPanel_);
    lv_obj_set_pos(wifiFallbackButton_, 26, 272);
    lv_obj_set_size(wifiFallbackButton_, 220, 58);
    lv_obj_set_style_bg_color(wifiFallbackButton_, lv_color_hex(0x176B8F), LV_PART_MAIN);
    lv_obj_set_style_radius(wifiFallbackButton_, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(wifiFallbackButton_, wifiFallbackCallback, LV_EVENT_CLICKED, this);
    wifiFallbackButtonText_ = makeLabel(wifiFallbackButton_, "", p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_set_width(wifiFallbackButtonText_, 204);
    lv_obj_set_style_text_align(wifiFallbackButtonText_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(wifiFallbackButtonText_);

    lv_obj_t* infoPanel = lv_obj_create(screen_);
    lv_obj_set_pos(infoPanel, 316, 94);
    lv_obj_set_size(infoPanel, 680, 352);
    flat(infoPanel);
    lv_obj_set_style_bg_color(infoPanel, lv_color_hex(0x10202A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(infoPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(infoPanel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(infoPanel, lv_color_hex(0x355565), LV_PART_MAIN);
    lv_obj_set_style_radius(infoPanel, 14, LV_PART_MAIN);

    lv_obj_t* instruction = makeLabel(
        infoPanel,
        p4SelectText(
            "Telefondan MG Smart Tester Cihazına gönderin. Profil ulaştığında kontrol ekranı otomatik açılır.\nWi-Fi yalnız gerektiğinde yedek aktarım için kullanılır.",
            "Send from the phone to MG Smart Tester. The profile check screen opens automatically when it arrives.\nUse Wi-Fi only as a fallback if needed.",
            "Stuur vanaf de telefoon naar MG Smart Tester. Het profielcontrolescherm opent automatisch.\nGebruik Wi-Fi alleen als reserve indien nodig.",
            "Vom Telefon an MG Smart Tester senden. Der Profilprüfbildschirm öffnet sich automatisch.\nWi-Fi nur bei Bedarf als Ersatz verwenden.",
            "Envoyez depuis le téléphone vers MG Smart Tester. L'écran de contrôle du profil s'ouvre automatiquement.\nUtilisez le Wi-Fi seulement en secours si nécessaire.",
            "Envíe desde el teléfono a MG Smart Tester. La pantalla de comprobación se abre automáticamente.\nUse Wi-Fi solo como respaldo si es necesario.",
            "Wyślij z telefonu do MG Smart Tester. Ekran kontroli profilu otworzy się automatycznie.\nWi-Fi używaj tylko awaryjnie."),
        p4Font14(),
        lv_color_hex(0xD7EAF4));
    lv_obj_set_width(instruction, 640);
    lv_label_set_long_mode(instruction, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(instruction, 20, 14);

    bleNameLabel_ = makeLabel(infoPanel, "", p4Font20(), lv_color_hex(0x6EDBFF));
    lv_obj_set_pos(bleNameLabel_, 20, 68);
    bleSessionLabel_ = makeLabel(infoPanel, "", p4Font14(), lv_color_hex(0xF1C96A));
    lv_obj_set_pos(bleSessionLabel_, 20, 102);
    receivedLabel_ = makeLabel(infoPanel, "", p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_set_pos(receivedLabel_, 20, 130);

    fileListLabel_ = makeLabel(infoPanel, "", p4Font14(), lv_color_hex(0xB7D9E8));
    lv_obj_set_pos(fileListLabel_, 20, 164);
    lv_obj_set_width(fileListLabel_, 640);
    lv_label_set_long_mode(fileListLabel_, LV_LABEL_LONG_WRAP);

    ssidLabel_ = makeLabel(infoPanel, "", p4Font14(), lv_color_hex(0x6EDBFF));
    lv_obj_set_pos(ssidLabel_, 20, 208);
    passwordLabel_ = makeLabel(infoPanel, "", p4Font14(), lv_color_hex(0x8EF0A7));
    lv_obj_set_pos(passwordLabel_, 20, 232);
    urlLabel_ = makeLabel(infoPanel, "", p4Font14(), lv_color_hex(0xDCECF4));
    lv_obj_set_pos(urlLabel_, 20, 256);
    sessionLabel_ = makeLabel(infoPanel, "", p4Font14(), lv_color_hex(0xF1C96A));
    lv_obj_set_pos(sessionLabel_, 20, 280);

    stateLabel_ = makeLabel(infoPanel, "", p4Font14(), lv_color_hex(0x9CC6DC));
    lv_obj_set_width(stateLabel_, 640);
    lv_label_set_long_mode(stateLabel_, LV_LABEL_LONG_WRAP);
    lv_obj_align(stateLabel_, LV_ALIGN_BOTTOM_LEFT, 20, -12);

    restartButton_ = lv_btn_create(screen_);
    lv_obj_set_pos(restartButton_, 24, 468);
    lv_obj_set_size(restartButton_, 216, 86);
    lv_obj_set_style_bg_color(restartButton_, lv_color_hex(0x176B8F), LV_PART_MAIN);
    lv_obj_set_style_radius(restartButton_, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(restartButton_, restartCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* restartText = makeLabel(
        restartButton_,
        p4SelectText("YENİ OTURUM", "NEW SESSION", "NIEUWE SESSIE", "NEUE SITZUNG", "NOUVELLE SESSION", "NUEVA SESIÓN", "NOWA SESJA"),
        p4Font20(),
        lv_color_hex(0xFFFFFF));
    lv_obj_center(restartText);

    reviewButton_ = lv_btn_create(screen_);
    lv_obj_set_pos(reviewButton_, 260, 468);
    lv_obj_set_size(reviewButton_, 472, 86);
    lv_obj_set_style_bg_color(reviewButton_, lv_color_hex(0x267047), LV_PART_MAIN);
    lv_obj_set_style_radius(reviewButton_, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(reviewButton_, reviewCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* reviewText = makeLabel(
        reviewButton_,
        p4SelectText("PROFİLİ KONTROL ET", "CHECK PROFILE", "PROFIEL CONTROLEREN", "PROFIL PRÜFEN", "CONTRÔLER PROFIL", "COMPROBAR PERFIL", "SPRAWDŹ PROFIL"),
        p4Font20(),
        lv_color_hex(0xFFFFFF));
    lv_obj_center(reviewText);

    lv_obj_t* back = lv_btn_create(screen_);
    lv_obj_set_pos(back, 752, 468);
    lv_obj_set_size(back, 244, 86);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x5E426F), LV_PART_MAIN);
    lv_obj_set_style_radius(back, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(back, backCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* backText = makeLabel(back, p4Texts().back, p4Font20(), lv_color_hex(0xFFFFFF));
    lv_obj_center(backText);
}

void DocumentImportScreen::refreshQr() {
#if LV_USE_QRCODE
    if (!qr_ || !network_ || !network_->documentApActive()) {
        return;
    }
    String payload = "WIFI:T:WPA;S:";
    payload += network_->documentApSsid();
    payload += ";P:";
    payload += network_->documentApPassword();
    payload += ";;";
    lv_qrcode_update(qr_, payload.c_str(), payload.length());
#endif
}

void DocumentImportScreen::refreshWifiFallbackUi() {
    if (!network_) return;
    const bool active = network_->documentApActive();

#if LV_USE_QRCODE
    if (qr_) {
        if (active) lv_obj_clear_flag(qr_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(qr_, LV_OBJ_FLAG_HIDDEN);
    }
#endif

    if (qrInfoLabel_) {
        if (active) {
            lv_label_set_text(
                qrInfoLabel_,
                p4SelectText("Wi-Fi yedek bağlantı QR", "Wi-Fi fallback QR",
                             "QR voor Wi-Fi-reserve", "QR für Wi-Fi-Ersatz",
                             "QR du secours Wi-Fi", "QR de respaldo Wi-Fi",
                             "QR zapasowego Wi-Fi"));
            lv_obj_align(qrInfoLabel_, LV_ALIGN_BOTTOM_MID, 0, -78);
        } else {
            lv_label_set_text(
                qrInfoLabel_,
                p4SelectText("Wi-Fi yedek bağlantısı kapalı.\nBluetooth kullanılamazsa açın.",
                             "Wi-Fi fallback is off.\nOpen it only if Bluetooth cannot be used.",
                             "Wi-Fi-reserve is uit.\nOpen alleen als Bluetooth niet bruikbaar is.",
                             "Wi-Fi-Ersatz ist aus.\nNur öffnen, wenn Bluetooth nicht verfügbar ist.",
                             "Le secours Wi-Fi est désactivé.\nÀ ouvrir seulement si Bluetooth est indisponible.",
                             "El respaldo Wi-Fi está desactivado.\nÁbralo solo si Bluetooth no está disponible.",
                             "Zapasowe Wi-Fi jest wyłączone.\nWłącz tylko, gdy Bluetooth jest niedostępny."));
            lv_obj_align(qrInfoLabel_, LV_ALIGN_CENTER, 0, -34);
        }
    }

    if (wifiFallbackButtonText_) {
        lv_label_set_text(
            wifiFallbackButtonText_,
            active
                ? p4SelectText("WI-FI YEDEĞİNİ KAPAT", "CLOSE WI-FI FALLBACK",
                               "WI-FI-RESERVE SLUITEN", "WI-FI-ERSATZ SCHLIESSEN",
                               "FERMER LE SECOURS WI-FI", "CERRAR RESPALDO WI-FI",
                               "WYŁĄCZ ZAPASOWE WI-FI")
                : p4SelectText("WI-FI YEDEĞİNİ AÇ", "OPEN WI-FI FALLBACK",
                               "WI-FI-RESERVE OPENEN", "WI-FI-ERSATZ ÖFFNEN",
                               "OUVRIR LE SECOURS WI-FI", "ABRIR RESPALDO WI-FI",
                               "WŁĄCZ ZAPASOWE WI-FI"));
    }
    if (wifiFallbackButton_) {
        const bool blockNewFallback = !active && portal_ && portal_->documentItemCount() > 0U;
        if (blockNewFallback) lv_obj_add_state(wifiFallbackButton_, LV_STATE_DISABLED);
        else lv_obj_clear_state(wifiFallbackButton_, LV_STATE_DISABLED);
    }

    lv_obj_t* labels[] = {ssidLabel_, passwordLabel_, urlLabel_, sessionLabel_};
    for (lv_obj_t* label : labels) {
        if (!label) continue;
        if (active) lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
}

void DocumentImportScreen::refreshFileList() {
    if (!portal_ || !fileListLabel_) return;
    const uint8_t count = portal_->documentItemCount();
    if (count == 0U) {
        lv_label_set_text(fileListLabel_,
                          p4SelectText("Profil bekleniyor...", "Waiting for profile...",
                                       "Wachten op profiel...", "Warte auf Profil...",
                                       "En attente du profil...", "Esperando perfil...",
                                       "Oczekiwanie na profil..."));
        return;
    }

    lv_label_set_text(fileListLabel_,
                      p4SelectText("Profil cihazda hazır. Kontrol ekranı otomatik açılacak.",
                                   "The profile is ready on the device. The check screen will open automatically.",
                                   "Het profiel staat klaar op het apparaat. Het controlescherm opent automatisch.",
                                   "Das Profil ist auf dem Gerät bereit. Der Prüfbildschirm öffnet sich automatisch.",
                                   "Le profil est prêt sur l'appareil. L'écran de contrôle va s'ouvrir automatiquement.",
                                   "El perfil está listo en el dispositivo. La pantalla de comprobación se abrirá automáticamente.",
                                   "Profil jest gotowy na urządzeniu. Ekran kontroli otworzy się automatycznie."));
}

void DocumentImportScreen::refreshLabels() {
    if (!network_ || !portal_) {
        return;
    }

    char buffer[256] = {};
    const bool active = network_->documentApActive();
    const bool bleAvailable = ble_ && ble_->available();
    const uint8_t count = portal_->documentUploadCount();
    const uint8_t previousCount = lastUploadCount_;

    const char* connectedSuffix =
        (bleAvailable && ble_->connected())
            ? p4SelectText("  [BAĞLI]", "  [CONNECTED]", "  [VERBONDEN]", "  [VERBUNDEN]",
                           "  [CONNECTÉ]", "  [CONECTADO]", "  [POŁĄCZONO]")
            : "";
    snprintf(buffer, sizeof(buffer), "Bluetooth: %s%s",
             bleAvailable ? ble_->deviceName().c_str() : "-",
             connectedSuffix);
    lv_label_set_text(bleNameLabel_, buffer);

    lv_label_set_text(
        bleSessionLabel_,
        count > 0U
            ? p4SelectText("Telefon aktarımı tamamlandı", "Phone transfer completed",
                           "Telefoonoverdracht voltooid", "Telefonübertragung abgeschlossen",
                           "Transfert du téléphone terminé", "Transferencia del teléfono completada",
                           "Transfer z telefonu zakończony")
            : (bleAvailable && ble_->connected())
                ? p4SelectText("Telefon bağlı", "Phone connected", "Telefoon verbonden", "Telefon verbunden",
                               "Téléphone connecté", "Teléfono conectado", "Telefon połączony")
                : p4SelectText("Telefon bağlantısı bekleniyor", "Waiting for phone connection",
                               "Wachten op telefoonverbinding", "Warte auf Telefonverbindung",
                               "En attente du téléphone", "Esperando conexión del teléfono",
                               "Oczekiwanie na połączenie telefonu"));

    snprintf(buffer, sizeof(buffer), "%s: %s",
             p4SelectText("Wi-Fi yedek", "Wi-Fi fallback", "Wi-Fi-reserve", "Wi-Fi-Ersatz",
                          "Secours Wi-Fi", "Respaldo Wi-Fi", "Zapasowe Wi-Fi"),
             active ? network_->documentApSsid().c_str() : "-");
    lv_label_set_text(ssidLabel_, buffer);

    snprintf(buffer, sizeof(buffer), "%s: %s",
             p4SelectText("Şifre", "Password", "Wachtwoord", "Passwort", "Mot de passe", "Contraseña", "Hasło"),
             active ? network_->documentApPassword().c_str() : "-");
    lv_label_set_text(passwordLabel_, buffer);

    snprintf(buffer, sizeof(buffer), "%s: %s",
             p4SelectText("Yedek adres", "Fallback address", "Reserveadres", "Ersatzadresse",
                          "Adresse de secours", "Dirección de respaldo", "Adres zapasowy"),
             active ? portal_->documentPortalUrl().c_str() : "-");
    lv_label_set_text(urlLabel_, buffer);

    snprintf(buffer, sizeof(buffer), "%s: %lu s",
             p4SelectText("Oturum kalan", "Session remaining", "Resterende sessie", "Sitzung verbleibend",
                          "Session restante", "Sesión restante", "Pozostały czas sesji"),
             static_cast<unsigned long>(network_->documentSessionRemainingSeconds()));
    lv_label_set_text(sessionLabel_, buffer);

    lv_label_set_text(
        receivedLabel_,
        count > 0U
            ? p4SelectText("Profil alındı", "Profile received", "Profiel ontvangen", "Profil empfangen",
                           "Profil reçu", "Perfil recibido", "Profil odebrany")
            : p4SelectText("Profil bekleniyor", "Waiting for profile", "Wachten op profiel", "Warte auf Profil",
                           "En attente du profil", "Esperando perfil", "Oczekiwanie na profil"));

    refreshFileList();
    refreshWifiFallbackUi();

    if (!portal_->lastDocumentError().isEmpty()) {
        lv_label_set_text(
            stateLabel_,
            p4SelectText("Aktarım tamamlanamadı. Telefonda profili kontrol edip yeniden deneyin.",
                         "Transfer could not be completed. Check the profile on the phone and try again.",
                         "De overdracht kon niet worden voltooid. Controleer het profiel op de telefoon en probeer opnieuw.",
                         "Die Übertragung konnte nicht abgeschlossen werden. Profil am Telefon prüfen und erneut versuchen.",
                         "Le transfert n'a pas pu être terminé. Vérifiez le profil sur le téléphone et réessayez.",
                         "No se pudo completar la transferencia. Revise el perfil en el teléfono y vuelva a intentarlo.",
                         "Nie udało się zakończyć przesyłania. Sprawdź profil w telefonie i spróbuj ponownie."));
    } else if (count > 0U) {
        lv_label_set_text(
            stateLabel_,
            p4SelectText("Profil alındı ve temel kontroller tamamlandı. Profil kontrol ekranı açılıyor...",
                         "Profile received and basic checks completed. Opening profile check...",
                         "Profiel ontvangen en basiscontroles voltooid. Profielcontrole wordt geopend...",
                         "Profil empfangen und Grundprüfungen abgeschlossen. Profilprüfung wird geöffnet...",
                         "Profil reçu et contrôles de base terminés. Ouverture du contrôle du profil...",
                         "Perfil recibido y comprobaciones básicas completadas. Abriendo comprobación del perfil...",
                         "Profil odebrany i podstawowe kontrole zakończone. Otwieranie kontroli profilu..."));
    } else if (bleAvailable) {
        lv_label_set_text(
            stateLabel_,
            p4SelectText("Telefondan profili gönderin.", "Send the profile from the phone.",
                         "Stuur het profiel vanaf de telefoon.", "Profil vom Telefon senden.",
                         "Envoyez le profil depuis le téléphone.", "Envíe el perfil desde el teléfono.",
                         "Wyślij profil z telefonu."));
    } else if (active) {
        lv_label_set_text(
            stateLabel_,
            p4SelectText("Bluetooth kullanılamıyor; Wi-Fi yedek bağlantısı hazır.",
                         "Bluetooth unavailable; Wi-Fi fallback is ready.",
                         "Bluetooth niet beschikbaar; Wi-Fi reserve is gereed.",
                         "Bluetooth nicht verfügbar; Wi-Fi-Ersatz ist bereit.",
                         "Bluetooth indisponible ; le secours Wi-Fi est prêt.",
                         "Bluetooth no disponible; Wi-Fi de respaldo listo.",
                         "Bluetooth niedostępny; zapasowe Wi-Fi gotowe."));
    } else {
        lv_label_set_text(
            stateLabel_,
            p4SelectText("Bluetooth kullanılamıyor. Gerekirse Wi-Fi yedek bağlantısını açın.",
                         "Bluetooth is unavailable. Open the Wi-Fi fallback if needed.",
                         "Bluetooth is niet beschikbaar. Open zo nodig de Wi-Fi-reserve.",
                         "Bluetooth ist nicht verfügbar. Bei Bedarf Wi-Fi-Ersatz öffnen.",
                         "Bluetooth est indisponible. Ouvrez le secours Wi-Fi si nécessaire.",
                         "Bluetooth no está disponible. Abra el respaldo Wi-Fi si es necesario.",
                         "Bluetooth jest niedostępny. W razie potrzeby włącz zapasowe Wi-Fi."));
    }

    if (reviewButton_) {
        lv_obj_clear_state(reviewButton_, LV_STATE_DISABLED);
    }

    // Normal successful flow advances automatically from the
    // transfer screen to one operator-friendly profile summary. Existing
    // stored profiles do not auto-open when this screen itself is first built.
    if (count == 0U) {
        autoOpenedForCurrentProfile_ = false;
    }
    const bool bleProfileReady = bleAvailable && ble_->profileReady();
    const bool wifiProfileReady = active && count > 0U && !bleProfileReady;
    if (count > 0U && !autoOpenedForCurrentProfile_ && (bleProfileReady || wifiProfileReady)) {
        reviewRequested_ = true;
        autoOpenedForCurrentProfile_ = true;
        Serial.printf("[MOBILE-PROFILE] operator summary auto-open requested transport=%s\n",
                      bleProfileReady ? "BLE" : "WIFI");
        Serial.flush();
    }

    (void)previousCount;
    lastUploadCount_ = count;
}

void DocumentImportScreen::update(bool force) {
    if (!network_ || !portal_) {
        return;
    }
    const uint32_t now = millis();
    const uint8_t uploadCount = portal_->documentUploadCount();
    // Avoid forcing a full 1024x600 LVGL refresh every second while flash and
    // Wi-Fi are actively receiving a profile. Upload completion still
    // refreshes immediately because the item count changes.
    const uint32_t refreshPeriodMs = portal_->documentUploadBusy() ? 5000U : 1000U;
    if (force || uploadCount != lastUploadCount_ ||
        now - lastRefreshMs_ >= refreshPeriodMs) {
        lastRefreshMs_ = now;
        refreshLabels();
    }
}

void DocumentImportScreen::activate() {
    backRequested_ = false;
    reviewRequested_ = false;
    if (ble_ && !ble_->sessionOpen() && portal_ && portal_->documentItemCount() == 0U) {
        const bool resumedBle = ble_->openSession();
        Serial.printf("[MOBILE-PROFILE] BLE session resume=%s\n", resumedBle ? "OK" : "FAILED");
        Serial.flush();
    }
    if (screen_ && lv_scr_act() != screen_) {
        lv_scr_load(screen_);
    }
    refreshWifiFallbackUi();
    refreshQr();
    update(true);
}

void DocumentImportScreen::backCallback(lv_event_t* event) {
    auto* self = static_cast<DocumentImportScreen*>(lv_event_get_user_data(event));
    if (self) {
        self->backRequested_ = true;
    }
}

void DocumentImportScreen::restartCallback(lv_event_t* event) {
    auto* self = static_cast<DocumentImportScreen*>(lv_event_get_user_data(event));
    if (!self || !self->portal_) {
        return;
    }
    if (self->ble_) self->ble_->closeSession();
    self->autoOpenedForCurrentProfile_ = false;
    self->wifiFallbackRequested_ = false;
    self->portal_->stopDocumentPortal();
    const bool cleared = self->portal_->clearDocumentItems();
    const bool bleOk = self->ble_ ? self->ble_->openSession() : false;
    Serial.printf("[MOBILE-PROFILE] new session Wi-Fi fallback=OFF profile-clear=%s BLE session=%s\n",
                  cleared ? "OK" : "FAILED", bleOk ? "OK" : "FAILED");
    Serial.flush();
    self->refreshWifiFallbackUi();
    self->refreshQr();
    self->update(true);
}

void DocumentImportScreen::wifiFallbackCallback(lv_event_t* event) {
    auto* self = static_cast<DocumentImportScreen*>(lv_event_get_user_data(event));
    if (!self || !self->portal_ || !self->network_) return;

    if (self->network_->documentApActive()) {
        self->portal_->stopDocumentPortal();
        self->wifiFallbackRequested_ = false;
        Serial.println("[MOBILE-PROFILE] Wi-Fi fallback closed by operator");
        Serial.flush();
    } else {
        if (self->portal_->documentItemCount() > 0U) {
            if (self->stateLabel_) {
                lv_label_set_text(
                    self->stateLabel_,
                    p4SelectText("Profil zaten alındı; Wi-Fi yedek bağlantısı açılmadı.",
                                 "A profile is already received; Wi-Fi fallback was not opened.",
                                 "Er is al een profiel ontvangen; Wi-Fi-reserve is niet geopend.",
                                 "Ein Profil wurde bereits empfangen; Wi-Fi-Ersatz wurde nicht geöffnet.",
                                 "Un profil a déjà été reçu ; le secours Wi-Fi n'a pas été ouvert.",
                                 "Ya se recibió un perfil; no se abrió el respaldo Wi-Fi.",
                                 "Profil został już odebrany; zapasowe Wi-Fi nie zostało włączone."));
            }
            return;
        }
        const bool ok = self->portal_->startDocumentPortal();
        self->wifiFallbackRequested_ = ok;
        Serial.printf("[MOBILE-PROFILE] Wi-Fi fallback opened by operator=%s\n", ok ? "OK" : "FAILED");
        Serial.flush();
        if (!ok && self->stateLabel_) {
            lv_label_set_text(
                self->stateLabel_,
                p4SelectText("Wi-Fi yedek bağlantısı açılamadı.",
                             "Wi-Fi fallback could not be opened.",
                             "Wi-Fi-reserve kon niet worden geopend.",
                             "Wi-Fi-Ersatz konnte nicht geöffnet werden.",
                             "Le secours Wi-Fi n'a pas pu être ouvert.",
                             "No se pudo abrir el respaldo Wi-Fi.",
                             "Nie udało się włączyć zapasowego Wi-Fi."));
        }
    }

    self->refreshWifiFallbackUi();
    self->refreshQr();
    self->update(true);
}

void DocumentImportScreen::reviewCallback(lv_event_t* event) {
    auto* self = static_cast<DocumentImportScreen*>(lv_event_get_user_data(event));
    if (!self || !self->portal_) {
        return;
    }
    if (self->portal_->documentItemCount() == 0U) {
        (void)self->portal_->recoverDocumentItemsFromStorage();
    }
    if (self->portal_->documentItemCount() == 0U) {
        if (self->stateLabel_) {
            lv_label_set_text(self->stateLabel_,
                              p4SelectText("İncelenecek kayıtlı profil bulunamadı.",
                                           "No stored profile is available for review.",
                                           "Geen opgeslagen profiel beschikbaar voor controle.",
                                           "Kein gespeichertes Profil zur Prüfung vorhanden.",
                                           "Aucun profil enregistré à vérifier.",
                                           "No hay perfiles guardados para revisar.",
                                           "Brak zapisanego profilu do sprawdzenia."));
        }
        return;
    }
    self->reviewRequested_ = true;
}

bool DocumentImportScreen::consumeBackRequest() {
    const bool value = backRequested_;
    backRequested_ = false;
    return value;
}

bool DocumentImportScreen::consumeReviewRequest() {
    const bool value = reviewRequested_;
    reviewRequested_ = false;
    return value;
}

}  // namespace mg::p4
