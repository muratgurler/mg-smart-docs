#pragma once
#include "Arduino.h"
typedef int wl_status_t;
typedef int arduino_event_id_t;
constexpr wl_status_t WL_IDLE_STATUS = 0;
class FakeWiFiClass { public: wl_status_t status() const { return WL_IDLE_STATUS; } };
static FakeWiFiClass WiFi;
