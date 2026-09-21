#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
checks = []

def text(path):
    return (root / path).read_text(encoding="utf-8")

def need(path, token, label):
    data = text(path)
    ok = token in data
    checks.append((label, ok))
    if not ok:
        print(f"FAIL: {label}: missing {token!r} in {path}")

analysis_h = text("AnalysisReportScreen.h")
analysis_cpp = text("AnalysisReportScreen.cpp")
shared_cpp = text("MgResultTable.cpp")
backbone_cpp = text("BackboneDemoScreen.cpp")
ino = text("MG_Test_ESP32P4_JC1060_Beta_1_0.ino")

checks.extend([
    ("analysis screen uses shared MG table", '#include "MgResultTable.h"' in analysis_h and "MgResultTable resultTable_" in analysis_h),
    ("analysis screen has no private LVGL table renderer", "lv_table_create" not in analysis_cpp and "tableDrawCallback" not in analysis_cpp),
    ("shared table vertical-only scrolling", "lv_obj_set_scroll_dir(body_, LV_DIR_VER)" in shared_cpp and "LV_SCROLLBAR_MODE_AUTO" in shared_cpp),
    ("shared table header/body center alignment", "lv_obj_set_style_text_align(header_, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS)" in shared_cpp and "lv_obj_set_style_text_align(body_, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS)" in shared_cpp),
    ("shared table draw-time centering", "dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER" in shared_cpp),
    ("shared table explicit 2px separator overlay", "createHeaderSeparators" in shared_cpp and "lv_obj_set_size(separator, 2, tableHeight)" in shared_cpp),
    ("shared separator high contrast", "0xA8D2E0" in shared_cpp),
    ("PASS green matches scan palette", "0x35D273" in shared_cpp),
    ("OPEN white matches scan palette", "0xE9EEF2" in shared_cpp),
    ("SHORT warning yellow matches scan palette", "0xFFD34D" in shared_cpp),
    ("WRONG fail red matches scan palette", "0xF04444" in shared_cpp),
    ("HIGH-R orange matches scan palette", "0xFF922E" in shared_cpp),
    ("return-to-measurement control", "ÖLÇÜME DÖN" in analysis_cpp),
    ("generic previous-menu control", "ÖNCEKİ MENÜ" in analysis_cpp and "ReturnToOriginMenu" in analysis_h),
    ("application returns report to launch origin", "returnFromBackboneDemo();" in ino and "ReturnToOriginMenu" in ino),
    ("application accepts result request from all backbone origins", "consumeAnalysisReportRequest()" in ino and "backboneReturn == BackboneReturn::AdvancedAnalysis" not in ino),
    ("finite analysis completion edge", "finiteAnalysisModule" in backbone_cpp),
    ("Kelvin report supported", "case BackboneModuleId::Kelvin" in backbone_cpp),
    ("TDR report supported", "case BackboneModuleId::Tdr" in backbone_cpp),
    ("pair report supported", "case BackboneModuleId::PairIntegrity" in backbone_cpp),
    ("calibration report supported", "case BackboneModuleId::SelfTestCalibration" in backbone_cpp),
    ("USB-C report supported", "case BackboneModuleId::UsbC" in backbone_cpp),
    ("PE-dS report supported", "case BackboneModuleId::PeDs" in backbone_cpp),
    ("Fixture SPC report supported", "case BackboneModuleId::FixtureSpc" in backbone_cpp),
    ("Smart Probe snapshot report supported", "case BackboneModuleId::SmartProbe" in backbone_cpp),
    ("Environment AWG snapshot report supported", "case BackboneModuleId::EnvironmentAwg" in backbone_cpp),
    ("Component test report supported", "case BackboneModuleId::ComponentTest" in backbone_cpp),
    ("Hardware validation report supported", "case BackboneModuleId::HardwareValidation" in backbone_cpp),
    ("multi-connector report supported", "case BackboneModuleId::MultiConnector" in backbone_cpp),
    ("voice service report supported", "case BackboneModuleId::Voice" in backbone_cpp),
    ("intermittent live report supported", "case BackboneModuleId::FlexGlitch" in backbone_cpp and "analysisReport_.liveSnapshot = true" in backbone_cpp),
    ("full-cable rows retained", "DigitalSweepReportEntry" in text("DigitalBackboneCore.h") and "reportEntries_[reportEntryCount_++]" in text("DigitalBackboneCore.cpp")),
    ("application analysis report screen state", "ActiveScreen::AnalysisReport" in ino and "showAnalysisCompletionReport" in ino),
    ("large analysis report reset is in-place", "resetAnalysisReportData(analysisReport_)" in backbone_cpp),
])

unsafe_reset = "analysisReport_ = AnalysisReportData{};"
checks.append(("GCC14 large aggregate assignment ICE guard", unsafe_reset not in backbone_cpp))

# Guard against the old full-cable sweep cursor bug: one increment only.
digital = text("DigitalBackboneCore.cpp")
old_double = "if (cursorIndex_ < 63U) {\n        ++cursorIndex_;\n        ++cursorIndex_;"
checks.append(("full cable natural contiguous pin order", old_double not in digital))

failed = [label for label, ok in checks if not ok]
for label, ok in checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {label}")
if failed:
    raise SystemExit(1)
print(f"Analysis report batch13 contract: {len(checks)}/{len(checks)} PASS")
