#!/usr/bin/env python3
"""Guard the JC1060 hardware and Fix28 UI/state-machine contracts."""

from pathlib import Path
import sys


def require(text: str, token: str, source: Path) -> None:
    if token not in text:
        raise AssertionError(f"{source.name}: missing {token!r}")


project = Path(sys.argv[1]).resolve()


def read(relative: str) -> tuple[Path, str]:
    path = project / relative
    return path, path.read_text(encoding="utf-8")


config_path, config = read("ConfigP4.h")
panel_path, panel = read("src/lcd/esp_lcd_jd9165.h")
panel_port_path, panel_port = read("src/lcd/jd9165_lcd.cpp")
touch_path, touch = read("src/touch/gt911_touch.cpp")
port_header_path, port_header = read("P4LvglPort.h")
port_source_path, port_source = read("P4LvglPort.cpp")
touch_test_path, touch_test = read("TouchTestScreen.cpp")
touch_controls_path, touch_controls = read("TouchControlPanel.cpp")
touch_controls_h_path, touch_controls_h = read("TouchControlPanel.h")
scan_path, scan = read("ScanScreen.cpp")
scan_h_path, scan_h = read("ScanScreen.h")
session_h_path, session_h = read("ScanSession.h")
session_cpp_path, session_cpp = read("ScanSession.cpp")
arrow_path, arrow = read("DirectionArrow.cpp")
arrow_h_path, arrow_h = read("DirectionArrow.h")
connector_path, connector = read("ConnectorFaceRenderer.cpp")
connector_h_path, connector_h = read("ConnectorFaceRenderer.h")
menu_path, menu = read("MainMenuScreen.cpp")
menu_h_path, menu_h = read("MainMenuScreen.h")
language_path, language = read("LanguageScreen.cpp")
language_visuals_path, language_visuals = read("P4LanguageVisuals.cpp")
localization_path, localization = read("P4Localization.cpp")
localization_h_path, localization_h = read("P4Localization.h")
profile_path, profile = read("CableProfile.cpp")
profile_h_path, profile_h = read("CableProfile.h")
controller_path, controller = read("FrontPanelController.cpp")
controller_h_path, controller_h = read("FrontPanelController.h")
sound_path, sound = read("SoundToggleButton.cpp")
button_3d_path, button_3d = read("P4Button3D.cpp")
button_3d_h_path, button_3d_h = read("P4Button3D.h")
main_path, main = read("MG_Test_ESP32P4_JC1060_Beta_1_0.ino")
lv_conf_path, lv_conf = read("lv_conf.h")
font_header_path, font_header = read("P4Fonts.h")
font_script_path, font_script = read("tools/generate_ui_fonts.sh")
platformio_path, platformio = read("platformio.ini")

# Proven working JC1060P470C_I_W panel/touch contract.
for token in (
    "kNativeDisplayWidth = 1024", "kNativeDisplayHeight = 600",
    "kLcdResetPin = 27", "kLcdBacklightPin = 23",
    "kTouchSdaPin = 7", "kTouchSclPin = 8",
    "kTouchResetPin = -1", "kTouchInterruptPin = -1",
):
    require(config, token, config_path)
require(panel, ".lane_bit_rate_mbps = 550", panel_path)
for token in (
    "dpi_config.virtual_channel = 0",
    "dpi_config.dpi_clock_freq_mhz = 51.2",
    "dpi_config.pixel_format = MIPI_DPI_PX_FORMAT",
    "dpi_config.num_fbs = 2",
    "dpi_config.video_timing.hsync_back_porch = 136",
    "dpi_config.video_timing.hsync_pulse_width = 24",
    "dpi_config.video_timing.hsync_front_porch = 160",
    "dpi_config.video_timing.vsync_back_porch = 21",
    "dpi_config.video_timing.vsync_pulse_width = 2",
    "dpi_config.video_timing.vsync_front_porch = 12",
    "dpi_config.flags.use_dma2d = true",
):
    require(panel_port, token, panel_port_path)
if "JD9165_1024_600_PANEL_60HZ_DPI_CONFIG" in panel_port:
    raise AssertionError("jd9165_lcd.cpp: order-sensitive DPI macro returned")

require(touch, "bool gt911_touch::begin()", touch_path)
require(touch, "GT911 unavailable", touch_path)
if "ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911" in touch:
    raise AssertionError("gt911_touch.cpp: missing touch must not abort boot")
require(port_header, '#include "esp_lcd_mipi_dsi.h"', port_header_path)
require(port_header, "esp_lcd_dpi_panel_event_data_t", port_header_path)
require(port_source, "esp_lcd_dpi_panel_register_event_callbacks", port_source_path)
for token in (
    "esp_lcd_dpi_panel_get_frame_buffer",
    "displayDriver_.full_refresh = true",
    "callbacks.on_refresh_done = refreshDoneCallback",
    "xSemaphoreTake(instance_->frameDoneSemaphore_",
    "lv_disp_flush_is_last(display)",
):
    require(port_source, token, port_source_path)
for token in (
    "kFrameBufferCount = 2", "kPanelFramePixels",
    "kUiFrameIntervalMs = 33UL",
):
    require(config, token, config_path)

# Five-point touch diagnostic stays reachable and uses the Unicode UI font.
for token in (
    "SOL ÜST", "SAĞ ÜST", "MERKEZ", "SOL ALT", "SAĞ ALT",
    "LV_EVENT_PRESSED", "LV_EVENT_RELEASED", "[TOUCHTEST][RESULT]",
    "TARAMA EKRANINA GEÇ", "p4Font20()", "soundButton_.create",
):
    require(touch_test, token, touch_test_path)

# The application must boot to the menu and expose both scan modes.
for token in (
    "showMainMenu();",
    "MainMenuAction::NormalScan",
    "MainMenuAction::SubDScan",
    "startScanUi(ScanProfileKind::Normal)",
    "startScanUi(ScanProfileKind::SubD)",
    "scanScreen.setProfileHandler(handleProfileControl, nullptr)",
    "scanScreen.setDirectionHandler(handleLargeDirectionArrow, nullptr)",
    "touchControls.begin(scanScreen.root())",
):
    require(main, token, main_path)
setup_body = main.split("void setup()", 1)[1].split("void loop()", 1)[0]
require(setup_body, "showMainMenu();", main_path)
if "startScanUi(" in setup_body:
    raise AssertionError("setup must not bypass the main menu")

for token in (
    "MainMenuAction::NormalScan", "MainMenuAction::SubDScan",
    "MainMenuAction::ExtraFeatures",
    "text.oneToOne", "text.subDTest", "soundButton_.create",
):
    require(menu + menu_h, token, menu_path)

# Sub-D profile order is operator-facing and must be exact. Sub-D never has PE.
expected_profiles = [
    "Sub-D25", "Sub-D9", "Sub-D15", "Sub-D37",
    "Sub-D44", "Sub-D50", "Sub-D62",
]
positions = []
for name in expected_profiles:
    token = f'{{"{name}", ScanProfileKind::SubD'
    require(profile, token, profile_path)
    positions.append(profile.index(token))
if positions != sorted(positions):
    raise AssertionError("CableProfile.cpp: Sub-D profile cycle order changed")
if profile.count("ScanProfileKind::SubD") != len(expected_profiles):
    raise AssertionError("CableProfile.cpp: unexpected Sub-D profile count")
for block in profile.split("ScanProfileKind::SubD")[1:]:
    head = block.split("dsub(", 1)[0]
    if "false, true" not in head:
        raise AssertionError("CableProfile.cpp: Sub-D must be 1..N+dS without PE")
for token in (
    "makeNormalProfile", "setNormalSignalPinCount",
    "nextNormalSignalPinPreset", "presets[] = {5U, 10U, 20U, 30U, 40U, 50U, 60U}",
    "kMaximumSignalPins", "includePe", "includeDrainShield",
    "includePeA", "includePeB", "includeDrainShieldA", "includeDrainShieldB",
    "pointEnabledOnSide", "sidePointMask",
):
    require(profile + profile_h, token, profile_path)

# Header profile controls: full Sub-D cycle or normal +/-/preset/edit.
# PE/dS are independent A/B switches located below their connector drawings.
for token in (
    '"Sub-D25"', "ProfileCommand::NextSubD",
    "ProfileCommand::DecreasePins", "ProfileCommand::IncreasePins",
    "ProfileCommand::NextPinPreset",
    "ProfileCommand::TogglePeA", "ProfileCommand::ToggleDrainShieldA",
    "ProfileCommand::TogglePeB", "ProfileCommand::ToggleDrainShieldB",
    'profile.includePeA ? "PE ON" : "PE OFF"',
    'profile.includeDrainShieldB ? "dS ON" : "dS OFF"',
    "soundButton_.create(header",
):
    require(scan, token, scan_path)
for token in (
    "ControlAction::NormalPinsDecrease", "ControlAction::NormalPinsIncrease",
    "ControlAction::NormalPinsNextPreset", "nextNormalSignalPinPreset",
    "ControlAction::TogglePeA", "ControlAction::ToggleDrainShieldA",
    "ControlAction::TogglePeB", "ControlAction::ToggleDrainShieldB",
    "selectScanMode", "profileIndex_ = 0",
):
    require(controller + controller_h, token, controller_path)

# PE/dS connector visibility is side-specific while bar/map slot allocation
# retains the union so a point can exist on only one side.
for token in (
    "session_.profile().includePeA",
    "session_.profile().includeDrainShieldA",
    "session_.profile().includePeB",
    "session_.profile().includeDrainShieldB",
    "formatSlotLabel",
):
    require(scan, token, scan_path)
for token in ("includePe_", "includeDrainShield_", "setConnector"):
    require(connector + connector_h, token, connector_path)

# Adjustable scan-speed control, rounded Sub-D shell, Knight-Rider active pair,
# persistent result state, non-stop and 7-corner arrow.
for token in (
    "lv_slider_create", "LV_EVENT_VALUE_CHANGED", "speedSliderCallback",
    "setScanSpeedPercent", "scanSpeedPercent", "p4Texts().testSpeed",
    "lv_obj_invalidate_area", "changedAreaCount > 8U",
    "nowMs - lastUiRefreshMs_ < kUiFrameIntervalMs",
):
    require(scan + session_h + session_cpp, token, scan_path)
if "demoLabel_" in scan:
    raise AssertionError("ScanScreen.cpp: DEMO DATA label must not remain")
for token in (
    "DSubGeometry", "makeDSubGeometry", "dSubRowCounts",
    "flangeWidth = 132", "flangeWidth = 142", "flangeWidth = 154",
    "flangeWidth = 166", "contacts == 50U",
    "fillRoundedTrapezoid", "geometry.shellTopWidth - 16",
):
    require(connector + connector_h, token, connector_path)
for token in (
    "kSlowestScanStepIntervalMs = 1500UL",
    "kDefaultScanStepIntervalMs = 180UL",
    "kRapidTestSpeedPercent = 90",
    "kRapidScanStepIntervalMs = 25UL",
    "kFastestScanStepIntervalMs = 20UL",
):
    require(config, token, config_path)

# Knight-Rider active pair, persistent result state, non-stop and 7-corner arrow.
for token in (
    "setContinuousMode(bool enabled)", "beginNextContinuousCycle()",
    "reportAtoB_[slot] = Measurement{}", "runState_ = RunState::Running",
    "stopOnError_ = enabled;", "continuousMode_ = enabled;",
    "restartCycleOnStart_ = true;",
):
    require(session_h + session_cpp, token, session_cpp_path)
for forbidden in (
    "stopOnError_ = continuousMode_ ? false : enabled;",
    "if (continuousMode_) {\n        stopOnError_ = false;",
):
    if forbidden in session_cpp:
        raise AssertionError(
            "ScanSession.cpp: NON-STOP must not disable HATADA DUR")
for token in (
    "kWidth = 236", "kHeight = 92", "arrowPoints_[7]",
    "rasterizeSevenCornerArrow", "arrowPoints_[0] = {142, 8}",
    "arrowPoints_[6] = {142, 31}", "LV_OBJ_FLAG_CLICKABLE",
):
    require(arrow + arrow_h, token, arrow_path)
if "lv_canvas_draw_polygon" in arrow or "lv_canvas_draw_rect" in arrow:
    raise AssertionError("DirectionArrow.cpp: direct 7-corner rasterizer required")

# Language order, reusable globe/flags and compact Unicode font set.
expected_language_order = (
    "P4Language::English", "P4Language::Dutch", "P4Language::Turkish",
    "P4Language::German", "P4Language::French", "P4Language::Spanish",
    "P4Language::Polish",
)
language_order_block = language.split("kLanguageDisplayOrder[]", 1)[1].split("};", 1)[0]
language_positions = [language_order_block.index(token)
                      for token in expected_language_order]
if language_positions != sorted(language_positions):
    raise AssertionError("LanguageScreen.cpp: operator language order changed")
for token in (
    "kLanguageDisplayOrder", "createLanguageFlag(rowContent, language, 0, 0)",
    "language == currentP4Language()", "soundButton_.create",
    "lv_obj_set_style_pad_column(rowContent, 10",
    "LV_FLEX_ALIGN_CENTER",
):
    require(language, token, language_path)
for token in (
    "createLanguageFlag", "createGlobeIcon", "kReadableStar[10]",
    "initializeTurkishFlagPixels", "insideReadableStar", "windingNumber",
    "178.084F", "200.345F", "samplesPerAxis = 4",
    "lv_canvas_set_buffer(canvas", "kFlagInnerWidth",
):
    require(language_visuals, token, language_visuals_path)
for token in (
    "createGlobeIcon(row, 0, 0)",
    "createLanguageFlag(row, currentP4Language(), 0, 0)",
    "lv_obj_set_style_pad_column(row, 8",
    "currentP4Language()",
):
    require(menu, token, menu_path)
for token in ("\"Dil\"", "Türkçe", "Français", "Español", "ÇOKLU KONNEKTÖR TESTİ"):
    require(localization, token, localization_path)
for token in ("mg_font_10", "mg_font_14", "mg_font_20",
              "mg_font_pin_84", "p4PinFont84"):
    require(font_header, token, font_header_path)
for token in (
    "#define LV_FONT_MONTSERRAT_10 0",
    "#define LV_FONT_MONTSERRAT_14 0",
    "#define LV_FONT_MONTSERRAT_20 0",
    "#define LV_FONT_MONTSERRAT_48 1",
    "#define LV_USE_FONT_COMPRESSED 1",
    "LV_FONT_DECLARE(mg_font_10)",
    "LV_FONT_DECLARE(mg_font_pin_84)",
):
    require(lv_conf, token, lv_conf_path)
for name in ("mg_font_10.c", "mg_font_14.c", "mg_font_20.c",
             "mg_font_pin_84.c"):
    if not (project / "src/fonts" / name).is_file():
        raise AssertionError(f"missing generated UI font: {name}")

required_glyphs = "ÇçĞğİıÖöŞşÜüÄäËëÏïßÀàÂâÆæÉéÈèÊêÎîÔôŒœÙùÛûŸÿÁáÍíÓóÚúÑñĄąĆćĘęŁłŃńŚśŹźŻż"
for character in required_glyphs:
    if character not in font_script:
        raise AssertionError(f"font generator: missing Unicode glyph {character!r}")
for token in ("DejaVuSans-Bold.ttf", "--size 84",
              "--symbols=-0123456789EPSd", "mg_font_pin_84"):
    require(font_script, token, font_script_path)

# A real speaker icon is present on all principal screens; mute adds red slash.
for token in (
    "Speaker body and cone", "Two sound-wave chevrons",
    "p4SoundMuted()", "0xF04444", "toggleP4SoundMuted()",
):
    require(sound, token, sound_path)

for token in (
    "ControlAction::Step", "ControlAction::ContinuousMode",
    "ControlAction::NextCable",
    "text.singleStep", "text.nonStop", "quickActionLabels_[4]",
    "consumeMainMenuRequest", "quickActionButtons_[4]",
    "nextCableButton_", "nextCableLabel_", "text.nextCable",
    "refreshButtonStates",
):
    require(touch_controls + touch_controls_h, token, touch_controls_path)

for forbidden in (
    "launcherButton_", "overlay_", "controlPage_", "diagnosticPage_",
    "wakeOverlay_", "consumeTouchTestRequest", "cancelOverlayResume",
    "showControlPage", "showDiagnosticPage", "launcherCallback",
):
    if forbidden in touch_controls + touch_controls_h:
        raise AssertionError(
            f"TouchControlPanel: removed control page token remains: {forbidden}")
if "consumeTouchTestRequest" in main:
    raise AssertionError("main: scan screen must not open a second control page")

# Scan controls share one low-memory 3D style set. Grey remains passive and
# green remains active; pressed buttons move down and lose most of the shadow.
for token in (
    "P4ButtonTone::Neutral", "P4ButtonTone::Active", "P4ButtonTone::Sound",
    "stylesInitialized", "lv_style_set_bg_grad_dir",
    "lv_style_set_shadow_width", "lv_style_set_shadow_ofs_y",
    "lv_style_set_translate_y", "LV_STATE_PRESSED",
    "LV_STATE_CHECKED", "lv_obj_add_state", "lv_obj_clear_state",
    "setP4Button3DTone", "applyP4Button3D",
):
    require(button_3d + button_3d_h, token, button_3d_path)
for source, path in ((touch_controls, touch_controls_path),
                     (scan, scan_path), (sound, sound_path)):
    require(source, "P4Button3D.h", path)
for token in (
    "session_.runState() == RunState::Running",
    "session_.stopOnError()",
    "session_.continuousMode()",
    "workflow_.snapshot().state == TestWorkflowState::Complete",
    "lv_obj_add_state(nextCableButton_, LV_STATE_DISABLED)",
    "quickX[4] = {850, 72, 168, 312}",
    "quickWidth[4] = {118, 92, 140, 96}",
    "nextCableButton_ = createButton(parent",
    "                                    412,",
    "                                    110,",
    "lv_label_set_text(quickActionLabels_[2], text.stopOnError)",
    "lv_label_set_text(quickActionLabels_[3], text.nonStop)",
    "lv_label_set_text(quickActionLabels_[0], text.scanPause)",
    "(index == 0U || index == 2U || index == 3U)",
):
    require(touch_controls, token, touch_controls_path)
if "? text.pause" in touch_controls or ": text.start" in touch_controls:
    raise AssertionError(
        "TouchControlPanel: SCAN/PAUSE caption must remain fixed")
for token in (
    '"TARA / DURAKLAT"', '"SCAN / PAUSE"',
    '"SCAN / PAUZE"', '"SCAN / PAUSA"', '"SKAN / PAUZA"',
):
    require(localization, token, localization_path)
for token in (
    "p4PinFont84()",
    "lv_obj_set_pos(pinAValue_, 224, 119)",
    "lv_obj_set_width(pinAValue_, 166)",
    "lv_obj_set_pos(pinBValue_, 634, 119)",
    "lv_obj_set_width(pinBValue_, 166)",
    "lv_obj_set_style_text_letter_space(pinAValue_, -2",
    "lv_obj_set_style_text_letter_space(pinBValue_, -2",
):
    require(scan, token, scan_path)
for token in (
    "legendRow_ = lv_obj_create(screen)",
    "lv_obj_set_pos(legendRow_, 6, 566)",
    "lv_obj_set_size(legendRow_, 1012, 34)",
    "LV_FLEX_ALIGN_SPACE_EVENLY",
    "PointVisualStatus::ShortCircuit",
    "PointVisualStatus::WrongConnection",
    "PointVisualStatus::HighResistance",
    "lv_obj_set_size(colorSquare, 22, 22)",
    "visualColor(legendStatuses[index])",
    "p4Font14()",
    'lv_label_set_text_fmt(legendLabels_[index], ": %s"',
    "text.legendShort", "text.legendWrong", "text.legendResistance",
):
    require(scan, token, scan_path)
if "summaryLabel_" in scan + scan_h:
    raise AssertionError("ScanScreen: obsolete tiny text legend remains")
for token in (
    '"BEKLİYOR", "TARAMA", "DOĞRU", "AÇIK DEVRE", "KISA DEVRE"',
    '"YANLIŞ BAĞLANTI", "YÜKSEK DİRENÇ"',
    '"WAITING", "SCANNING", "OK", "OPEN CIRCUIT", "SHORT CIRCUIT"',
):
    require(localization, token, localization_path)
for token in (
    "profileButton_ = createHeaderButton(526",
    "                                        320",
    "normalMinusButton_ = createHeaderButton(650",
    "normalCountButton_ = createHeaderButton(708",
    "ProfileCommand::NextPinPreset",
    "normalPlusButton_ = createHeaderButton(792",
    'peToggleButtonA_ = createSideSpecialButton(10, "PE ON"',
    'dsToggleButtonA_ = createSideSpecialButton(108, "dS ON"',
    'peToggleButtonB_ = createSideSpecialButton(826, "PE ON"',
    'dsToggleButtonB_ = createSideSpecialButton(924, "dS ON"',
    "lv_obj_set_pos(speedSlider_, 420, 256)",
    "soundButton_.create(header, 974, 4, 42)",
):
    require(scan, token, scan_path)
for forbidden in (
    "stopOnErrorOn", "stopOnErrorOff", "nonStopOn", "nonStopOff",
    "HATADA DUR: AÇIK", "HATADA DUR: KAPALI",
    "NON-STOP: AÇIK", "NON-STOP: KAPALI",
):
    if forbidden in touch_controls + localization + localization_h:
        raise AssertionError(
            f"scan controls: obsolete state suffix remains: {forbidden}")
for token in (
    'strcmp(profile.name, "Sub-D25") != 0',
    "setScanButtonState(peToggleButtonA_, profile.includePeA)",
    "setScanButtonState(dsToggleButtonA_, profile.includeDrainShieldA)",
    "setScanButtonState(peToggleButtonB_, profile.includePeB)",
    "setScanButtonState(dsToggleButtonB_, profile.includeDrainShieldB)",
):
    require(scan, token, scan_path)

if "-DCONFIG_ESP_LCD_TOUCH_MAX_POINTS" in platformio:
    raise AssertionError("platformio.ini: duplicate touch point define")
if list((project / "src/lcd").glob("*st7701*")):
    raise AssertionError("ST7701 files must not be present in JC1060 target")

print("JC1060 Fix28 hardware/UI/state-machine contract passed.")

# Fix30 network-driver contract
network_cpp = (project / "MgNetworkManager.cpp").read_text(encoding="utf-8")
network_ui = (project / "NetworkStatusScreen.cpp").read_text(encoding="utf-8")
settings_cpp = (project / "SettingsScreen.cpp").read_text(encoding="utf-8")
main_ino = (project / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")
assert "ETH_PHY_IP101" in network_cpp
assert "constexpr int kEthPhyAddr = 1;" in network_cpp
assert "constexpr int kEthMdcPin = 31;" in network_cpp
assert "constexpr int kEthMdioPin = 52;" in network_cpp
assert "constexpr int kEthPowerPin = 51;" in network_cpp
assert "ETH.begin(ETH_PHY_IP101, kEthPhyAddr, kEthMdcPin, kEthMdioPin, kEthPowerPin, EMAC_CLK_EXT_IN)" in network_cpp
assert "#define ETH_PHY_TYPE" not in network_cpp
assert "WiFi.scanNetworks()" in network_cpp
assert "SettingsAction::Wifi,true" in settings_cpp.replace(" ", "")
assert "SettingsAction::Ethernet,true" in settings_cpp.replace(" ", "")
assert "NetworkStatusScreen" in main_ino
assert "wifiScanCount" in network_ui
assert "class MgNetworkManager" in (project / "MgNetworkManager.h").read_text(encoding="utf-8")
assert "MgNetworkManager networkManager;" in main_ino
print("Fix31 network driver contract passed.")
