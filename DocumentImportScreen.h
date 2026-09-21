#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace mg::p4 {

class MapWebEditor;
class MgNetworkManager;
class MgBleProfileServer;

class DocumentImportScreen {
public:
    void begin(MapWebEditor& portal, MgNetworkManager& network, MgBleProfileServer& ble);
    void activate();
    void update(bool force = false);
    bool consumeBackRequest();
    bool consumeReviewRequest();

private:
    static void backCallback(lv_event_t* event);
    static void restartCallback(lv_event_t* event);
    static void reviewCallback(lv_event_t* event);
    static void wifiFallbackCallback(lv_event_t* event);

    void buildUi();
    void refreshQr();
    void refreshLabels();
    void refreshFileList();
    void refreshWifiFallbackUi();

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* qrPanel_ = nullptr;
    lv_obj_t* qr_ = nullptr;
    lv_obj_t* qrInfoLabel_ = nullptr;
    lv_obj_t* wifiFallbackButton_ = nullptr;
    lv_obj_t* wifiFallbackButtonText_ = nullptr;
    lv_obj_t* bleNameLabel_ = nullptr;
    lv_obj_t* bleSessionLabel_ = nullptr;
    lv_obj_t* ssidLabel_ = nullptr;
    lv_obj_t* passwordLabel_ = nullptr;
    lv_obj_t* urlLabel_ = nullptr;
    lv_obj_t* sessionLabel_ = nullptr;
    lv_obj_t* receivedLabel_ = nullptr;
    lv_obj_t* stateLabel_ = nullptr;
    lv_obj_t* fileListLabel_ = nullptr;
    lv_obj_t* restartButton_ = nullptr;
    lv_obj_t* reviewButton_ = nullptr;

    MapWebEditor* portal_ = nullptr;
    MgNetworkManager* network_ = nullptr;
    MgBleProfileServer* ble_ = nullptr;
    bool backRequested_ = false;
    bool reviewRequested_ = false;
    bool autoOpenedForCurrentProfile_ = false;
    bool wifiFallbackRequested_ = false;
    uint8_t lastUploadCount_ = 0xFFU;
    uint32_t lastRefreshMs_ = 0U;
};

}  // namespace mg::p4
