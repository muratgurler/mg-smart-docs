#include "NetworkStatusScreen.h"
#include <Arduino.h>
#include <cstring>
#include <WiFi.h>
#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"

namespace mg::p4 {
namespace {
void flat(lv_obj_t* o) {
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
}

lv_obj_t* makeText(lv_obj_t* p, const char* s, const lv_font_t* f, lv_color_t c) {
    lv_obj_t* l = lv_label_create(p);
    lv_label_set_text(l, s);
    lv_obj_set_style_text_font(l, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, c, LV_PART_MAIN);
    return l;
}


static const char* kWifiKeyboardLowerMap[] = {
    "123", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "DEL", "\n",
    "SHIFT", "a", "s", "d", "f", "g", "h", "j", "k", "l", "ENTER", "\n",
    "CTRL", "z", "x", "c", "v", "b", "n", "m", ",", ".", ":", "\n",
    "CANCEL", "<", "SPACE", ">", "OK", ""
};

static const char* kWifiKeyboardUpperMap[] = {
    "123", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "DEL", "\n",
    "ABC", "A", "S", "D", "F", "G", "H", "J", "K", "L", "ENTER", "\n",
    "CTRL", "Z", "X", "C", "V", "B", "N", "M", ",", ".", ":", "\n",
    "CANCEL", "<", "SPACE", ">", "OK", ""
};

static const char* kWifiKeyboardSpecialMap[] = {
    "ABC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "DEL", "\n",
    "@", "#", "$", "%", "&", "*", "-", "+", "(", ")", "ENTER", "\n",
    "_", "!", "?", "/", "\\", "=", "[", "]", "{", "}", ":", "\n",
    "CANCEL", "<", "SPACE", ">", "OK", ""
};
lv_obj_t* makeActionButton(lv_obj_t* parent, int x, int y, int w, int h,
                           const char* text, lv_color_t bg,
                           lv_event_cb_t cb, void* userData) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);
    lv_obj_t* label = makeText(btn, text, p4Font14(), lv_color_hex(0xFFFFFF));
    lv_obj_center(label);
    return btn;
}
}

void NetworkStatusScreen::begin(NetworkScreenMode mode, MgNetworkManager& network) {
    network_ = &network;
    mode_ = mode;
    backRequested_ = false;
    scanRequested_ = false;
    lastScanState_ = -999;
    selectedSsid_ = String();
    selectedEncrypted_ = false;
    passwordVisible_ = false;
    passwordOverlay_ = nullptr;
    passwordTitle_ = nullptr;
    passwordTextArea_ = nullptr;
    keyboard_ = nullptr;
    showPasswordButton_ = nullptr;
    disconnectButton_ = nullptr;
    forgetButton_ = nullptr;

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

    const auto& t = p4Texts();
    lv_obj_t* title = makeText(header,
                               mode_ == NetworkScreenMode::Wifi ? t.wifi : t.ethernet,
                               p4Font20(), lv_color_hex(0xEAF6FF));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);
    soundButton_.create(header, 970, 18, 42);

    statusLabel_ = makeText(screen_, "", p4Font14(), lv_color_hex(0xCDE8F5));
    lv_obj_set_pos(statusLabel_, 40, 94);
    lv_obj_set_width(statusLabel_, 944);
    lv_label_set_long_mode(statusLabel_, LV_LABEL_LONG_WRAP);

    if (mode_ == NetworkScreenMode::Wifi) {
        scanButton_ = makeActionButton(screen_, 760, 150, 220, 58,
                                       p4SelectText("Wi-Fi TARA", "Wi-Fi SCAN", "Wi-Fi SCANNEN", "Wi-Fi SCANNEN", "SCAN Wi-Fi", "ESCANEAR Wi-Fi", "SKAN Wi-Fi"), lv_color_hex(0x167A9A),
                                       scanCallback, this);

        list_ = lv_list_create(screen_);
        lv_obj_set_pos(list_, 40, 150);
        lv_obj_set_size(list_, 690, 370);
        lv_obj_set_style_bg_color(list_, lv_color_hex(0x0F1D26), LV_PART_MAIN);
        lv_obj_set_style_border_color(list_, lv_color_hex(0x33576A), LV_PART_MAIN);
        lv_obj_set_style_border_width(list_, 1, LV_PART_MAIN);

        createWifiActionButtons();
        requestScan();
    } else {
        network_->ensureEthernetStarted();
        lv_obj_t* info = makeText(screen_,
            "IP101 | RMII | PHY addr 1 | MDC 31 | MDIO 52 | PHY RST 51 | DHCP",
            p4Font14(), lv_color_hex(0x7FB7D1));
        lv_obj_set_pos(info, 40, 180);
        lv_obj_set_width(info, 920);
    }

    lv_obj_t* back = makeActionButton(screen_, 40, 535, 220, 50,
                                      t.back, lv_color_hex(0x6A4B8A),
                                      backCallback, this);
    (void)back;

    refreshStatus();
}

void NetworkStatusScreen::createWifiActionButtons() {
    disconnectButton_ = makeActionButton(screen_, 760, 225, 220, 52,
                                         p4SelectText("BAĞLANTIYI KES", "DISCONNECT", "VERBINDING VERBREKEN", "TRENNEN", "DÉCONNECTER", "DESCONECTAR", "ROZŁĄCZ"), lv_color_hex(0x8A5C25),
                                         disconnectCallback, this);
    forgetButton_ = makeActionButton(screen_, 760, 292, 220, 52,
                                     p4SelectText("AĞI UNUT", "FORGET NETWORK", "NETWERK VERGETEN", "NETZWERK VERGESSEN", "OUBLIER LE RÉSEAU", "OLVIDAR RED", "ZAPOMNIJ SIEĆ"), lv_color_hex(0x8A3434),
                                     forgetCallback, this);
}

void NetworkStatusScreen::update() {
    if (!network_) return;

    if (scanRequested_ && mode_ == NetworkScreenMode::Wifi && !passwordOverlay_ &&
        !network_->wifiConnecting()) {
        scanRequested_ = false;
        network_->startWifiScan();
        lastScanState_ = -999;
    }

    if (mode_ == NetworkScreenMode::Wifi) {
        const int state = WiFi.scanComplete();
        if (state != lastScanState_) {
            lastScanState_ = state;
            if (state >= 0) {
                refreshWifiList();
            }
        }
    }

    const uint32_t now = millis();
    if (now - lastStatusRefreshMs_ >= 500U) {
        lastStatusRefreshMs_ = now;
        refreshStatus();
    }
}

void NetworkStatusScreen::refreshWifiList() {
    if (!list_ || !network_) return;
    lv_obj_clean(list_);
    const int count = network_->wifiScanCount();
    const int visible = count > 25 ? 25 : count;
    if (visible == 0) {
        lv_obj_t* l = lv_list_add_text(list_, p4SelectText("Ağ bulunamadı", "No networks found", "Geen netwerken gevonden", "Keine Netzwerke gefunden", "Aucun réseau trouvé", "No se encontraron redes", "Nie znaleziono sieci"));
        lv_obj_set_style_text_font(l, p4Font14(), LV_PART_MAIN);
        return;
    }

    for (int i = 0; i < visible; ++i) {
        const String ssid = network_->wifiSsid(i);
        const int32_t rssi = network_->wifiRssi(i);
        const bool locked = network_->wifiEncrypted(i);
        const bool saved = network_->hasSavedWifi() && ssid == network_->savedWifiSsid();

        char line[112] = {};
        snprintf(line, sizeof(line), "%s  %ld dBm  %s%s",
                 ssid.length() ? ssid.c_str() : p4SelectText("<gizli>", "<hidden>", "<verborgen>", "<versteckt>", "<masqué>", "<oculto>", "<ukryta>"),
                 static_cast<long>(rssi), locked ? p4SelectText("KİLİTLİ", "LOCK", "BEVEILIGD", "GESICHERT", "SÉCURISÉ", "SEGURA", "ZABEZPIECZONA") : p4SelectText("AÇIK", "OPEN", "OPEN", "OFFEN", "OUVERT", "ABIERTA", "OTWARTA"),
                 saved ? p4SelectText("  KAYITLI", "  SAVED", "  OPGESLAGEN", "  GESPEICHERT", "  ENREGISTRÉ", "  GUARDADA", "  ZAPISANA") : "");

        lv_obj_t* b = lv_list_add_btn(list_, nullptr, line);
        lv_obj_set_style_text_font(b, p4Font14(), LV_PART_MAIN);
        wifiButtonContexts_[i].self = this;
        wifiButtonContexts_[i].index = i;
        lv_obj_add_event_cb(b, wifiItemCallback, LV_EVENT_CLICKED, &wifiButtonContexts_[i]);
    }
}

void NetworkStatusScreen::selectWifiNetwork(int index) {
    if (!network_ || index < 0 || index >= network_->wifiScanCount()) return;

    selectedSsid_ = network_->wifiSsid(index);
    selectedEncrypted_ = network_->wifiEncrypted(index);
    if (selectedSsid_.isEmpty()) {
        Serial.println("[WIFI] hidden SSID selection ignored");
        return;
    }

    if (network_->hasSavedWifi() && selectedSsid_ == network_->savedWifiSsid()) {
        Serial.printf("[WIFI] selected saved SSID=%s\n", selectedSsid_.c_str());
        Serial.flush();
        network_->connectSavedWifi();
        return;
    }

    if (!selectedEncrypted_) {
        network_->connectWifi(selectedSsid_, String(), true);
        return;
    }

    openPasswordDialog(selectedSsid_);
}

void NetworkStatusScreen::openPasswordDialog(const String& ssid) {
    if (!screen_) return;
    closePasswordDialog();

    passwordOverlay_ = lv_obj_create(screen_);
    lv_obj_set_pos(passwordOverlay_, 20, 84);
    lv_obj_set_size(passwordOverlay_, 984, 500);
    lv_obj_set_style_bg_color(passwordOverlay_, lv_color_hex(0x10232F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(passwordOverlay_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(passwordOverlay_, lv_color_hex(0x4C819A), LV_PART_MAIN);
    lv_obj_set_style_border_width(passwordOverlay_, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(passwordOverlay_, 10, LV_PART_MAIN);

    // keep the password dialog fixed.
    // LVGL containers are scrollable by default; when the textarea receives focus
    // the parent could scroll and push the keyboard down/out of the visible area.
    lv_obj_clear_flag(passwordOverlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(passwordOverlay_, LV_SCROLLBAR_MODE_OFF);

    String title = String("Wi-Fi: ") + ssid;
    passwordTitle_ = makeText(passwordOverlay_, title.c_str(), p4Font14(), lv_color_hex(0xEAF6FF));
    lv_obj_set_pos(passwordTitle_, 28, 8);
    lv_obj_set_width(passwordTitle_, 900);

    passwordTextArea_ = lv_textarea_create(passwordOverlay_);
    lv_obj_set_pos(passwordTextArea_, 28, 42);
    lv_obj_set_size(passwordTextArea_, 635, 48);
    lv_textarea_set_one_line(passwordTextArea_, true);
    lv_textarea_set_password_mode(passwordTextArea_, true);
    lv_textarea_set_max_length(passwordTextArea_, 64);
    lv_textarea_set_placeholder_text(passwordTextArea_, p4SelectText("Wi-Fi şifresi", "Wi-Fi password", "Wi-Fi-wachtwoord", "Wi-Fi-Passwort", "Mot de passe Wi-Fi", "Contraseña Wi-Fi", "Hasło Wi-Fi"));
    lv_obj_set_style_text_font(passwordTextArea_, p4Font14(), LV_PART_MAIN);

    showPasswordButton_ = makeActionButton(passwordOverlay_, 682, 42, 250, 48,
                                           p4SelectText("GÖSTER", "SHOW", "TONEN", "ANZEIGEN", "AFFICHER", "MOSTRAR", "POKAŻ"), lv_color_hex(0x385F72),
                                           showPasswordCallback, this);

    makeActionButton(passwordOverlay_, 28, 102, 210, 44,
                     p4SelectText("BAĞLAN", "CONNECT", "VERBINDEN", "VERBINDEN", "CONNECTER", "CONECTAR", "POŁĄCZ"), lv_color_hex(0x218A55),
                     connectCallback, this);
    makeActionButton(passwordOverlay_, 256, 102, 210, 44,
                     p4SelectText("İPTAL", "CANCEL", "ANNULEREN", "ABBRECHEN", "ANNULER", "CANCELAR", "ANULUJ"), lv_color_hex(0x7A4B4B),
                     cancelPasswordCallback, this);

    // Hotfix7: use a plain button matrix instead of LVGL's symbol-based keyboard.
    // All function keys are ASCII text, so missing LV_SYMBOL glyphs cannot render as squares.
    keyboard_ = lv_btnmatrix_create(passwordOverlay_);
    lv_btnmatrix_set_map(keyboard_, kWifiKeyboardLowerMap);
    lv_obj_set_size(keyboard_, 928, 300);
    lv_obj_align(keyboard_, LV_ALIGN_TOP_LEFT, 10, 92);
    lv_obj_clear_flag(keyboard_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(keyboard_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_text_font(keyboard_, p4Font14(), LV_PART_ITEMS);
    lv_obj_set_style_pad_row(keyboard_, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(keyboard_, 4, LV_PART_MAIN);
    lv_obj_add_event_cb(keyboard_, keyboardCallback, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_add_state(passwordTextArea_, LV_STATE_FOCUSED);
    lv_obj_update_layout(passwordOverlay_);
    lv_obj_align(keyboard_, LV_ALIGN_TOP_LEFT, 10, 92);

    Serial.printf("[WIFI][HOTFIX7] keyboard x=%d y=%d w=%d h=%d ASCII-KEYS\n",
                  (int)lv_obj_get_x(keyboard_), (int)lv_obj_get_y(keyboard_),
                  (int)lv_obj_get_width(keyboard_), (int)lv_obj_get_height(keyboard_));
    Serial.flush();
}

void NetworkStatusScreen::closePasswordDialog() {
    if (passwordOverlay_) {
        lv_obj_del(passwordOverlay_);
    }
    passwordOverlay_ = nullptr;
    passwordTitle_ = nullptr;
    passwordTextArea_ = nullptr;
    keyboard_ = nullptr;
    showPasswordButton_ = nullptr;
    passwordVisible_ = false;
}

void NetworkStatusScreen::submitPassword() {
    if (!network_ || !passwordTextArea_ || selectedSsid_.isEmpty()) return;
    const char* text = lv_textarea_get_text(passwordTextArea_);
    const String password = text ? String(text) : String();
    if (selectedEncrypted_ && password.length() < 8) {
        lv_textarea_set_placeholder_text(passwordTextArea_, p4SelectText("En az 8 karakter", "Minimum 8 characters", "Minimaal 8 tekens", "Mindestens 8 Zeichen", "Minimum 8 caractères", "Mínimo 8 caracteres", "Minimum 8 znaków"));
        return;
    }
    network_->connectWifi(selectedSsid_, password, true);
    closePasswordDialog();
}

void NetworkStatusScreen::refreshStatus() {
    if (!statusLabel_ || !network_) return;
    char line[320] = {};
    if (mode_ == NetworkScreenMode::Wifi) {
        const bool connected = network_->wifiConnected();
        const int scanState = WiFi.scanComplete();
        const char* state = connected
            ? p4SelectText("BAĞLI", "CONNECTED", "VERBONDEN", "VERBUNDEN", "CONNECTÉ", "CONECTADO", "POŁĄCZONO")
            : (network_->wifiConnecting()
                ? p4SelectText("BAĞLANIYOR", "CONNECTING", "VERBINDEN...", "VERBINDET...", "CONNEXION...", "CONECTANDO...", "ŁĄCZENIE...")
                : p4SelectText("BAĞLI DEĞİL", "DISCONNECTED", "NIET VERBONDEN", "GETRENNT", "DÉCONNECTÉ", "DESCONECTADO", "ROZŁĄCZONO"));
        snprintf(line, sizeof(line),
                 "Wi-Fi: %s | SSID: %s | IP: %s | MAC: %s | Saved: %s | Scan: %s",
                 state,
                 network_->wifiConnectedSsid().c_str(),
                 network_->wifiIp().c_str(), network_->wifiMac().c_str(),
                 network_->hasSavedWifi() ? network_->savedWifiSsid().c_str() : "-",
                 scanState == WIFI_SCAN_RUNNING ? p4SelectText("TARANIYOR", "RUNNING", "SCANNEN", "LÄUFT", "EN COURS", "ESCANEANDO", "SKANOWANIE") : "READY");
    } else {
        snprintf(line, sizeof(line),
                 "Ethernet driver: %s | Link: %s | IP: %s | MAC: %s | %u Mbps %s",
                 network_->ethernetStarted() ? "READY" : p4SelectText("HATA", "FAILED", "MISLUKT", "FEHLER", "ÉCHEC", "FALLO", "BŁĄD"),
                 network_->ethernetLinkUp() ? p4SelectText("AKTİF", "UP", "ACTIEF", "AKTIV", "ACTIF", "ACTIVO", "AKTYWNE") : p4SelectText("KAPALI", "DOWN", "INACTIEF", "INAKTIV", "INACTIF", "INACTIVO", "NIEAKTYWNE"),
                 network_->ethernetIp().c_str(), network_->ethernetMac().c_str(),
                 static_cast<unsigned>(network_->ethernetSpeedMbps()),
                 network_->ethernetFullDuplex() ? "FULL" : "HALF");
    }
    lv_label_set_text(statusLabel_, line);
}

void NetworkStatusScreen::backCallback(lv_event_t* event) {
    auto* self = static_cast<NetworkStatusScreen*>(lv_event_get_user_data(event));
    if (!self) return;
    if (self->passwordOverlay_) self->closePasswordDialog(); else self->requestBack();
}

void NetworkStatusScreen::scanCallback(lv_event_t* event) {
    auto* self = static_cast<NetworkStatusScreen*>(lv_event_get_user_data(event));
    if (self) self->requestScan();
}

void NetworkStatusScreen::wifiItemCallback(lv_event_t* event) {
    auto* ctx = static_cast<WifiButtonContext*>(lv_event_get_user_data(event));
    if (ctx && ctx->self) ctx->self->selectWifiNetwork(ctx->index);
}

void NetworkStatusScreen::connectCallback(lv_event_t* event) {
    auto* self = static_cast<NetworkStatusScreen*>(lv_event_get_user_data(event));
    if (self) self->submitPassword();
}

void NetworkStatusScreen::cancelPasswordCallback(lv_event_t* event) {
    auto* self = static_cast<NetworkStatusScreen*>(lv_event_get_user_data(event));
    if (self) self->closePasswordDialog();
}

void NetworkStatusScreen::showPasswordCallback(lv_event_t* event) {
    auto* self = static_cast<NetworkStatusScreen*>(lv_event_get_user_data(event));
    if (!self || !self->passwordTextArea_ || !self->showPasswordButton_) return;
    self->passwordVisible_ = !self->passwordVisible_;
    lv_textarea_set_password_mode(self->passwordTextArea_, !self->passwordVisible_);
    lv_obj_t* label = lv_obj_get_child(self->showPasswordButton_, 0);
    if (label) {
        lv_label_set_text(label, self->passwordVisible_ ? p4SelectText("GİZLE", "HIDE", "VERBERGEN", "AUSBLENDEN", "MASQUER", "OCULTAR", "UKRYJ") : p4SelectText("GÖSTER", "SHOW", "TONEN", "ANZEIGEN", "AFFICHER", "MOSTRAR", "POKAŻ"));
    }
}

void NetworkStatusScreen::disconnectCallback(lv_event_t* event) {
    auto* self = static_cast<NetworkStatusScreen*>(lv_event_get_user_data(event));
    if (self && self->network_) self->network_->disconnectWifi();
}

void NetworkStatusScreen::forgetCallback(lv_event_t* event) {
    auto* self = static_cast<NetworkStatusScreen*>(lv_event_get_user_data(event));
    if (self && self->network_) self->network_->forgetWifi();
}

void NetworkStatusScreen::keyboardCallback(lv_event_t* event) {
    auto* self = static_cast<NetworkStatusScreen*>(lv_event_get_user_data(event));
    if (!self || !self->keyboard_ || !self->passwordTextArea_) return;
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;

    const uint16_t id = lv_btnmatrix_get_selected_btn(self->keyboard_);
    if (id == LV_BTNMATRIX_BTN_NONE) return;
    const char* key = lv_btnmatrix_get_btn_text(self->keyboard_, id);
    if (!key || !*key) return;

    if (strcmp(key, "SHIFT") == 0) {
        lv_btnmatrix_set_map(self->keyboard_, kWifiKeyboardUpperMap);
    } else if (strcmp(key, "ABC") == 0) {
        lv_btnmatrix_set_map(self->keyboard_, kWifiKeyboardLowerMap);
    } else if (strcmp(key, "123") == 0) {
        lv_btnmatrix_set_map(self->keyboard_, kWifiKeyboardSpecialMap);
    } else if (strcmp(key, "DEL") == 0) {
        lv_textarea_del_char(self->passwordTextArea_);
    } else if (strcmp(key, "ENTER") == 0 || strcmp(key, "OK") == 0) {
        self->submitPassword();
    } else if (strcmp(key, "CANCEL") == 0) {
        self->closePasswordDialog();
    } else if (strcmp(key, "<") == 0) {
        lv_textarea_cursor_left(self->passwordTextArea_);
    } else if (strcmp(key, ">") == 0) {
        lv_textarea_cursor_right(self->passwordTextArea_);
    } else if (strcmp(key, "SPACE") == 0) {
        lv_textarea_add_text(self->passwordTextArea_, " ");
    } else if (strcmp(key, "CTRL") == 0) {
        // Reserved for future shortcuts; intentionally no action in password entry.
    } else {
        lv_textarea_add_text(self->passwordTextArea_, key);
    }
}

void NetworkStatusScreen::requestBack() { backRequested_ = true; }
void NetworkStatusScreen::requestScan() { scanRequested_ = true; }
bool NetworkStatusScreen::consumeBackRequest() {
    const bool requested = backRequested_;
    backRequested_ = false;
    return requested;
}

} // namespace mg::p4
