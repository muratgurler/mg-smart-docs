#!/usr/bin/env python3
from pathlib import Path
import sys

project = Path(sys.argv[1]).resolve()

def text(name):
    return (project / name).read_text(encoding="utf-8")

def require(src, token, name):
    if token not in src:
        raise AssertionError(f"{name}: missing {token!r}")

backbone_h = text("FeatureBackbone.h")
backbone_cpp = text("FeatureBackbone.cpp")
screen = text("BackboneDemoScreen.cpp")
main_h = text("MainMenuScreen.h")
main_cpp = text("MainMenuScreen.cpp")
ino = text("MG_Test_ESP32P4_JC1060_Beta_1_0.ino")
extra_h = text("ExtraFeaturesScreen.h")
extra_cpp = text("ExtraFeaturesScreen.cpp")
adv_h = text("AdvancedAnalysisScreen.h")
adv_cpp = text("AdvancedAnalysisScreen.cpp")
settings_h = text("SettingsScreen.h")
settings_cpp = text("SettingsScreen.cpp")
board = text("V33BoardConfig.h")
hw = text("V33HardwareManager.cpp")

# All 15 top-level architecture sections have a software seam before PCB hardware.
for token in (
    "Scan128", "Kelvin", "SelfTestCalibration", "Tdr", "PairIntegrity",
    "UsbC", "PeDs", "FixtureSpc", "SmartProbe", "Network", "Voice",
    "EnvironmentAwg", "BarcodeQr", "ComponentTest", "HardwareValidation",
):
    require(backbone_h, token, "FeatureBackbone.h")

# Production-specific workflows that sit inside those sections also have demos.
for token in ("CableLearn", "MultiConnector", "FlexGlitch"):
    require(backbone_h, token, "FeatureBackbone.h")

for token in (
    "class BackboneModuleDriver", "attachDriver", "preferHardware",
    "DEMO fallback active", "Sender LOW / other 127 INPUT",
    "READY / START required", "No intermediate bypass",
):
    require(backbone_h + backbone_cpp, token, "FeatureBackbone")

# Shared rendering shell is role-aware: function-specific controls replace the
# old universal START / STEP / RESET strip. The state-machine seam remains.
for token in (
    "ÖLÇ", "ZERO / OFFSET", "HIZLI TEST", "TAM TEST", "KABLOYU OKU",
    "PROBU BAŞLAT", "PR ARA", "HI MG", "KOD OKU", "GPIO TESTİ",
    "ÖĞRENMEYİ BAŞLAT", "SEGMENT SEÇ", "LATCH TEMİZLE",
    "backbone_.pauseResume()", "backbone_.reset()",
    "digitalCore_.scan().step()", "digitalCore_.tick(now)",
    "DEMO MODE - real PCB outputs disabled",
):
    require(screen, token, "BackboneDemoScreen.cpp")
if "Action::StartPause" in screen or 'tr() ? "SIFIRLA" : "RESET"' in screen:
    raise AssertionError("BackboneDemoScreen.cpp: stale universal-control UI remains")

# Previously disabled main-menu workflows are now live demo routes.
for token in ("CableLearn", "QrBarcode", "Reports", "MultiConnector"):
    require(main_h, token, "MainMenuScreen.h")
for token in (
    "MainMenuAction::CableLearn, true", "MainMenuAction::QrBarcode, true",
    "MainMenuAction::Reports, true", "MainMenuAction::MultiConnector, true",
):
    require(main_cpp, token, "MainMenuScreen.cpp")

# Permanent language-button regression contract remains untouched.
for token in (
    "lv_obj_set_size(button, compact ? 340 : 312, compact ? 72 : 118)",
    "lv_obj_set_ext_click_area(button, 8)", "LV_OBJ_FLAG_PRESS_LOCK",
    "lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE)",
):
    require(main_cpp, token, "MainMenuScreen.cpp")

# Advanced and Extra menus route into the common backbone instead of dead dialogs.
require(adv_h, "consumeBackboneRequest", "AdvancedAnalysisScreen.h")
require(adv_h, "consumeConnectionScanRequest", "AdvancedAnalysisScreen.h")
for token in ("BackboneModuleId::Kelvin", "BackboneModuleId::Tdr",
              "BackboneModuleId::PairIntegrity", "BackboneModuleId::FlexGlitch"):
    require(adv_cpp, token, "AdvancedAnalysisScreen.cpp")
require(extra_h, "consumeBackboneRequest", "ExtraFeaturesScreen.h")
for token in ("BackboneModuleId::SelfTestCalibration", "BackboneModuleId::SmartProbe",
              "BackboneModuleId::PeDs", "BackboneModuleId::FixtureSpc",
              "BackboneModuleId::Voice", "BackboneModuleId::EnvironmentAwg",
              "BackboneModuleId::UsbC"):
    require(extra_cpp, token, "ExtraFeaturesScreen.cpp")

# Hardware-validation entry exists under Settings, while real carrier outputs remain gated.
require(settings_h, "HardwareStatus", "SettingsScreen.h")
require(settings_cpp, "DONANIM / PIN FREEZE", "SettingsScreen.cpp")
require(board, "constexpr bool kHardwareEnabled = false", "V33BoardConfig.h")
for token in ("MCP23S17", "ADS122C04", "kGpioReservedLcdReset = 5",
              "kGpioMcpDigCsN = 45", "kGpioGlitchEventQ = 46", "kGpioFusb302IntN = 47"):
    require(board + hw, token, "V33 hardware contract")
if "MCP23017" in hw or "ADS1115" in hw:
    raise AssertionError("V33HardwareManager.cpp: stale MCP23017/ADS1115 architecture text remains")

# App routing and demo/hardware seam are initialized at boot.
for token in (
    '#include "FeatureBackbone.h"', '#include "BackboneDemoScreen.h"',
    "FeatureBackbone featureBackbone", "BackboneDemoScreen backboneDemoScreen",
    "featureBackbone.begin(v33Hardware)", "ActiveScreen::BackboneDemo",
    "showBackboneDemo", "returnFromBackboneDemo",
    "SettingsAction::HardwareStatus",
):
    require(ino, token, "Stage03.ino")

print("Extra15 backbone batch1 contract passed.")
