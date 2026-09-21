#pragma once

#include <lvgl.h>
#include <stdint.h>
#include "MgNetworkManager.h"
#include "SoundToggleButton.h"

namespace mg::p4 {

enum class NetworkScreenMode : uint8_t { Wifi, Ethernet };

class NetworkStatusScreen {
public:
    void begin(NetworkScreenMode mode, MgNetworkManager& network);
    void update();
    bool consumeBackRequest();

private:
    struct WifiButtonContext {
        NetworkStatusScreen* self = nullptr;
        int index = -1;
    };

    static void backCallback(lv_event_t* event);
    static void scanCallback(lv_event_t* event);
    static void wifiItemCallback(lv_event_t* event);
    static void connectCallback(lv_event_t* event);
    static void cancelPasswordCallback(lv_event_t* event);
    static void showPasswordCallback(lv_event_t* event);
    static void disconnectCallback(lv_event_t* event);
    static void forgetCallback(lv_event_t* event);
    static void keyboardCallback(lv_event_t* event);

    void requestBack();
    void requestScan();
    void selectWifiNetwork(int index);
    void openPasswordDialog(const String& ssid);
    void closePasswordDialog();
    void submitPassword();
    void refreshWifiList();
    void refreshStatus();
    void createWifiActionButtons();

    MgNetworkManager* network_ = nullptr;
    NetworkScreenMode mode_ = NetworkScreenMode::Wifi;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* list_ = nullptr;
    lv_obj_t* scanButton_ = nullptr;
    lv_obj_t* disconnectButton_ = nullptr;
    lv_obj_t* forgetButton_ = nullptr;

    lv_obj_t* passwordOverlay_ = nullptr;
    lv_obj_t* passwordTitle_ = nullptr;
    lv_obj_t* passwordTextArea_ = nullptr;
    lv_obj_t* keyboard_ = nullptr;
    lv_obj_t* showPasswordButton_ = nullptr;

    SoundToggleButton soundButton_;
    WifiButtonContext wifiButtonContexts_[25]{};
    String selectedSsid_;
    bool selectedEncrypted_ = false;
    bool passwordVisible_ = false;
    bool backRequested_ = false;
    bool scanRequested_ = false;
    int lastScanState_ = -999;
    uint32_t lastStatusRefreshMs_ = 0;
};

} // namespace mg::p4
