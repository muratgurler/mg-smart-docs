#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>

namespace mg::p4 {

class MgNetworkManager {
public:
    void begin();
    void tick();
    bool ensureEthernetStarted();

    bool startWifiScan();
    bool wifiScanRunning() const;
    int wifiScanCount() const;
    String wifiSsid(int index) const;
    int32_t wifiRssi(int index) const;
    bool wifiEncrypted(int index) const;
    void clearWifiScan();

    bool connectWifi(const String& ssid, const String& password, bool remember = true);
    bool connectSavedWifi();
    void disconnectWifi();
    void forgetWifi();
    bool hasSavedWifi() const;
    String savedWifiSsid() const;
    bool wifiConnecting() const { return wifiConnecting_; }
    wl_status_t wifiStatus() const { return WiFi.status(); }

    bool wifiConnected() const;
    String wifiIp() const;
    String wifiMac() const;
    String wifiConnectedSsid() const;

    bool ethernetStarted() const { return ethernetStarted_; }
    bool ethernetLinkUp() const { return ethernetLinkUp_; }
    bool ethernetHasIp() const { return ethernetHasIp_; }
    String ethernetIp() const;
    String ethernetMac() const;
    uint16_t ethernetSpeedMbps() const;
    bool ethernetFullDuplex() const;

    bool startDocumentAp(uint32_t timeoutSeconds = 600U);
    void stopDocumentAp();
    void touchDocumentApSession();
    bool documentApActive() const { return documentApActive_; }
    String documentApSsid() const { return documentApSsid_; }
    String documentApPassword() const { return documentApPassword_; }
    String documentApIp() const;
    String documentSessionToken() const { return documentSessionToken_; }
    uint32_t documentSessionRemainingSeconds() const;

private:
    static void networkEvent(arduino_event_id_t event);
    static MgNetworkManager* instance_;

    void loadSavedWifi();
    void saveWifiCredentials(const String& ssid, const String& password);

    bool ethernetStarted_ = false;
    volatile bool ethernetLinkUp_ = false;
    volatile bool ethernetHasIp_ = false;

    String savedSsid_;
    String savedPassword_;
    String pendingSsid_;
    String pendingPassword_;
    bool pendingRemember_ = false;
    bool wifiConnecting_ = false;
    wl_status_t lastWifiStatus_ = WL_IDLE_STATUS;
    uint32_t wifiConnectStartMs_ = 0;

    DNSServer documentDns_;
    bool documentApActive_ = false;
    String documentApSsid_;
    String documentApPassword_;
    String documentSessionToken_;
    uint32_t documentApTimeoutMs_ = 600000U;
    uint32_t documentApExpiresAtMs_ = 0U;
    // Document transfer is intentionally isolated AP-only. Remember whether
    // an existing STA connection should be restored when the portal closes.
    bool documentRestoreStaAfterAp_ = false;
};

} // namespace mg::p4
