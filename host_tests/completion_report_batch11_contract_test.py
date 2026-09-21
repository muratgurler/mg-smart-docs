#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
ino = (root / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")
report_h = (root / "CompletionReportScreen.h").read_text(encoding="utf-8")
report_cpp = (root / "CompletionReportScreen.cpp").read_text(encoding="utf-8")
formatter_h = (root / "ReportFormatter.h").read_text(encoding="utf-8")
formatter_cpp = (root / "ReportFormatter.cpp").read_text(encoding="utf-8")
backbone_h = (root / "BackboneDemoScreen.h").read_text(encoding="utf-8")
backbone_cpp = (root / "BackboneDemoScreen.cpp").read_text(encoding="utf-8")
session_cpp = (root / "ScanSession.cpp").read_text(encoding="utf-8")
shared_h = (root / "MgResultTable.h").read_text(encoding="utf-8")
shared_cpp = (root / "MgResultTable.cpp").read_text(encoding="utf-8")

checks = {
    "dedicated completion report screen": "class CompletionReportScreen" in report_h,
    "finite test report entry": "beginTest(TestWorkflowController& workflow" in report_h,
    "cable learn report entry": "beginCableLearn(const CableProfile& profile" in report_h,
    "finite complete guard excludes non-stop": "if (!scanSession.continuousMode()" in ino and "showTestCompletionReport();" in ino,
    "non-stop storage remains cycle-reused": "NON-STOP deliberately has no final aggregate report" in session_cpp,
    "automatic learn completion edge": "consumeCableLearnCompletion()" in backbone_h and "cableLearnCompletionPending_ = true" in backbone_cpp,
    "application auto-opens learn report": "showCableLearnCompletionReport();" in ino and "consumeCableLearnCompletion()" in ino,
    "learn save enters workflow without start": "prepareCableLearn(completionReportScreen.learnedProfile()" in ino and "no autostart" in ino,
    "report stats cover fault classes": all(token in formatter_h for token in ["open", "shortCircuit", "wrongConnection", "highResistance"]),
    "whole-net detail preserves masks": "expectedReceiverMask" in formatter_cpp and "senderGroupMask" in formatter_cpp and "actualReceiverMask" in formatter_cpp,
    "resistance travels with measurement": all(token in (root / "ScanSource.h").read_text(encoding="utf-8") for token in ["resistanceValid", "resistanceMilliOhm", "resistanceLimitMilliOhm"]),
    "table exposes resistance value and state": "R (mOhm)" in report_cpp and "R DURUM" in report_cpp and "resistanceState" in formatter_h,
    "untrusted resistance is explicit N/A": '"N/A"' in formatter_cpp and "resistanceMeaningful" in formatter_cpp,
    "common-net human row": '"%s <-> %s | GOT %s <-> %s | %s"' in formatter_cpp,
    "test rows preserve natural scan order": "Preserve the natural electrical scan order" in report_cpp and "classPass" not in report_cpp,
    "scrollable fixed-header table replaces pagination": "MgResultTable resultTable_" in report_h and "resultTable_.create" in report_cpp and "changePage" not in report_cpp,
    "normal report uses shared result-table renderer": '#include "MgResultTable.h"' in report_h and "lv_table_create" not in report_cpp,
    "normal report rows use shared status palette": all(token in report_cpp for token in ["MgResultStatus::Pass", "MgResultStatus::Open", "MgResultStatus::ShortCircuit", "MgResultStatus::WrongConnection", "MgResultStatus::HighResistance"]),
    "shared table vertically scrolls body": "lv_obj_set_scroll_dir(body_, LV_DIR_VER)" in shared_cpp and "LV_SCROLLBAR_MODE_AUTO" in shared_cpp,
    "shared table centers header and body": "lv_obj_set_style_text_align(header_, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS)" in shared_cpp and "lv_obj_set_style_text_align(body_, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS)" in shared_cpp and "dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER" in shared_cpp,
    "shared table uses physical 2px header separators": "createHeaderSeparators" in shared_cpp and "lv_obj_set_size(separator, 2, tableHeight)" in shared_cpp and "0xA8D2E0" in shared_cpp,
    "shared palette matches scan colors": all(token in shared_cpp for token in ["0x35D273", "0xE9EEF2", "0xFFD34D", "0xF04444", "0xFF922E"]),
    "shared draw callback colors whole row": all(token in shared_cpp for token in ["LV_EVENT_DRAW_PART_BEGIN", "drawCallback", "dsc->rect_dsc->bg_color", "dsc->label_dsc->color"]),
    "report has operator actions": all(token in report_h for token in ["MainMenu", "Retest", "Records", "SaveLearnedProfile", "ReturnToLearning"]),
    "learn save acknowledgement": "markLearnedProfileSaved" in report_h and "PROFİL KAYDEDİLDİ" in report_cpp,
    "operator start invariant retained": "autoStartRequested = false" in (root / "TestWorkflowController.h").read_text(encoding="utf-8"),
    "host report stats test": "testEndOfTestReportStatsMatchFiniteSweep" in (root / "host_tests/scan_session_test.cpp").read_text(encoding="utf-8"),
    "host common-net report test": "testDetailedReportPreservesWholeCommonNet" in (root / "host_tests/scan_session_test.cpp").read_text(encoding="utf-8"),
    "host resistance-table truth test": "testScrollableTableCarriesResistanceTruthfully" in (root / "host_tests/scan_session_test.cpp").read_text(encoding="utf-8"),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
if failed:
    raise SystemExit(f"{len(failed)} completion-report contract checks failed")
print(f"{len(checks)}/{len(checks)} completion-report batch11 contract checks passed.")
