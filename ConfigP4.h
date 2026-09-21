#pragma once

#include <stdint.h>
#include <stddef.h>

namespace mg::p4 {

// Guition JC1060P470C_I_W: JD9165BA, native 1024x600 landscape MIPI-DSI.
// No software rotation is used: panel, LVGL and GT911 share one coordinate
// system.  Keeping this explicit prevents the old JC4880 480x800/ST7701 port
// from being selected accidentally.
constexpr uint16_t kNativeDisplayWidth = 1024;
constexpr uint16_t kNativeDisplayHeight = 600;
constexpr uint16_t kDisplayWidth = 1024;
constexpr uint16_t kDisplayHeight = 600;

constexpr int8_t kLcdResetPin = 27;
constexpr int8_t kLcdBacklightPin = 23;
constexpr int8_t kTouchSdaPin = 7;
constexpr int8_t kTouchSclPin = 8;
// The JC1060 manufacturer's Arduino and ESP-IDF examples leave the GT911
// reset and interrupt lines unconnected.  Driving GPIO22/GPIO21 here held the
// controller in the wrong state and made the first I2C transaction fail.
constexpr int8_t kTouchResetPin = -1;
constexpr int8_t kTouchInterruptPin = -1;

// The display port uses the two native MIPI-DPI RGB565 framebuffers directly as LVGL's
// full-screen draw buffers. The panel swaps complete frames at refresh-done;
// no pixel strip is copied into a framebuffer while that framebuffer is being
// scanned to the display.
constexpr uint8_t kFrameBufferCount = 2;
constexpr uint32_t kPanelFramePixels =
    static_cast<uint32_t>(kNativeDisplayWidth) * kNativeDisplayHeight;
constexpr uint32_t kPanelFrameBytes = kPanelFramePixels * 2U;
constexpr uint32_t kFrameSyncTimeoutMs = 100UL;

// Electrical scanning and visual rendering intentionally have separate rates.
// The scan engine can continue at 10 ms/step, while the screen presents at
// most one coherent state every 33 ms (about 30 FPS). This prevents a backlog
// of obsolete LED positions at the fast end of the speed slider.
constexpr uint32_t kUiFrameIntervalMs = 33UL;
static_assert(kFrameBufferCount == 2,
              "Tear-free MIPI-DPI output requires two framebuffers");
static_assert(kUiFrameIntervalMs >= 16UL,
              "UI frame limiter must not exceed the physical panel rate");

// Per side there are 64 electrical test points in the agreed hardware model:
// index 0 = PE, indices 1..62 = signal conductors, index 63 = drain/shield.
constexpr uint8_t kTestPointsPerSide = 64;
constexpr uint8_t kPeTestIndex = 0;
constexpr uint8_t kFirstSignalTestIndex = 1;
constexpr uint8_t kLastSignalTestIndex = 62;
constexpr uint8_t kDrainShieldTestIndex = 63;
constexpr uint8_t kMaximumSignalPins = 62;

// Operator adjustable scan speed. The UI exposes 10..100%. The mapping is
// intentionally piecewise so the useful 80%/180 ms production setting stays
// unchanged, while the final part of the slider becomes a genuinely fast
// production sweep:
//   10% = 1500 ms for close visual inspection
//   80% =  180 ms normal production speed
//   90% =   25 ms fast production sweep
//  100% =   20 ms maximum validated target
//
// The previous speed curve used 95 ms at 90% and 10 ms only at the extreme 100% position. The
// 60 FPS hardware recording showed that the operator was therefore still
// seeing roughly ten visible steps per second near the fast end. The current curve makes
// 90..100% the useful 20..25 ms range without changing the established 80%
// setting. The UI continues to present coherent frames at its own rate.
constexpr uint8_t kMinimumTestSpeedPercent = 10;
constexpr uint8_t kMaximumTestSpeedPercent = 100;
constexpr uint8_t kDefaultTestSpeedPercent = 80;
constexpr uint8_t kRapidTestSpeedPercent = 90;
constexpr uint32_t kSlowestScanStepIntervalMs = 1500UL;
constexpr uint32_t kDefaultScanStepIntervalMs = 180UL;
constexpr uint32_t kRapidScanStepIntervalMs = 25UL;
constexpr uint32_t kFastestScanStepIntervalMs = 20UL;
constexpr uint32_t kDemoStepIntervalMs = kDefaultScanStepIntervalMs;

// After the final B->A measurement, keep that last physical
// connection visible before opening the completion report. At fast scan
// speeds one electrical step can be only 20..25 ms, which is too short for
// the operator (and can be shorter than one UI frame). The final point is
// therefore shown for at least 300 ms, or for the selected scan interval if
// the operator deliberately chose a slower visual scan.
constexpr uint32_t kFinalScanPointDisplayMinMs = 300UL;
static_assert(kFinalScanPointDisplayMinMs >= kUiFrameIntervalMs,
              "Final scan point must remain visible for at least one UI frame");

// TEK ADIM has a second, operator-friendly jog function. A normal tap still
// advances exactly one point. Holding the button for 1.5 s starts a temporary
// medium-speed scan; releasing the button pauses immediately and restores the
// operator's previous TEST HIZI setting. Keep these values behind a regression
// guard because this interaction is part of the production UI contract.
constexpr uint32_t kSingleStepHoldThresholdMs = 1500UL;
constexpr uint8_t kSingleStepHoldSpeedPercent = 50U;
static_assert(kMinimumTestSpeedPercent < kDefaultTestSpeedPercent &&
                  kDefaultTestSpeedPercent < kRapidTestSpeedPercent &&
                  kRapidTestSpeedPercent < kMaximumTestSpeedPercent,
              "Default scan speed must remain inside the slider range");
static_assert(kSlowestScanStepIntervalMs > kDefaultScanStepIntervalMs &&
                  kDefaultScanStepIntervalMs > kRapidScanStepIntervalMs &&
                  kRapidScanStepIntervalMs > kFastestScanStepIntervalMs,
              "Scan intervals must be ordered slow-to-fast");
constexpr bool kAutoReverseAfterAtoB = true;

// Workplace PR lookup fallback defaults. Runtime configuration normally loads these
// values from AYARLAR -> WORKPLACE SERVER (Preferences/NVS), so the real site
// URL/auth/CA/response mapping can be entered later without rebuilding. The
// template must include literal {PR}; the scanned PR is percent-encoded.
constexpr const char* kWorkplacePrEndpointTemplate = "";
// Leave empty for HTTP. For HTTPS, the workplace root/intermediate CA PEM can
// be entered at runtime. This compile-time value is only a fallback; insecure
// TLS is intentionally forbidden.
constexpr const char* kWorkplaceTlsCaPem = "";
constexpr uint32_t kWorkplaceHttpTimeoutMs = 5000UL;
constexpr size_t kWorkplaceMaxResponseBytes = 32768U;

// The measurement PCB connection is intentionally disabled until the final
// RS485/UART pins and protocol are frozen. The complete UI/session code runs
// against the deterministic demo source meanwhile.
constexpr bool kEnableExternalMeasurementController = false;
constexpr int8_t kPlannedMeasurementUartRxPin = -1;
constexpr int8_t kPlannedMeasurementUartTxPin = -1;
constexpr uint32_t kMeasurementUartBaud = 1000000UL;

// UI geometry shared by firmware and the reference renderer.
constexpr uint16_t kTopAreaHeight = 286;
constexpr uint16_t kBottomAreaY = kTopAreaHeight;
constexpr uint16_t kBottomAreaHeight = kDisplayHeight - kTopAreaHeight;
constexpr uint16_t kBarAreaLeft = 48;
constexpr uint16_t kBarAreaRight = 1010;
constexpr uint16_t kBarAreaWidth = kBarAreaRight - kBarAreaLeft;

static_assert(kTestPointsPerSide == 64,
              "The P4 tester is designed for 64 test points per side");
static_assert(kPeTestIndex == 0, "PE must remain test-point zero");
static_assert(kDrainShieldTestIndex == 63,
              "dS must remain test-point 63");
static_assert(kLastSignalTestIndex + 1 == kDrainShieldTestIndex,
              "Signal and dS indices must be contiguous");

}  // namespace mg::p4
