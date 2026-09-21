#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]

def read(name):
    return (root / name).read_text(encoding="utf-8")

shared_h = read("MgResultTable.h")
shared_cpp = read("MgResultTable.cpp")
completion_h = read("CompletionReportScreen.h")
completion_cpp = read("CompletionReportScreen.cpp")
analysis_h = read("AnalysisReportScreen.h")
analysis_cpp = read("AnalysisReportScreen.cpp")
backbone = read("BackboneDemoScreen.cpp")
ino = read("MG_Test_ESP32P4_JC1060_Beta_1_0.ino")
archive = read("WorkflowArchiveScreen.cpp")
overview = read("WorkflowOverviewScreen.cpp")

checks = {
    "single reusable detailed-result renderer exists": "class MgResultTable" in shared_h,
    "renderer uses fixed header plus scrollable body": "header_ = lv_table_create(parent)" in shared_cpp and "body_ = lv_table_create(parent)" in shared_cpp and "lv_obj_set_scroll_dir(body_, LV_DIR_VER)" in shared_cpp,
    "renderer keeps identical header body widths": "lv_table_set_col_width(header_, column, columnWidths[column])" in shared_cpp and "lv_table_set_col_width(body_, column, columnWidths[column])" in shared_cpp,
    "renderer centers headers": "lv_obj_set_style_text_align(header_, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS)" in shared_cpp,
    "renderer centers body": "lv_obj_set_style_text_align(body_, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS)" in shared_cpp,
    "renderer enforces physical draw centering": "dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER" in shared_cpp,
    "renderer has real 2px header separators": "lv_obj_set_size(separator, 2, tableHeight)" in shared_cpp,
    "renderer uses scan palette": all(x in shared_cpp for x in ["0x35D273", "0xE9EEF2", "0xFFD34D", "0xF04444", "0xFF922E"]),
    "completion report uses shared renderer": "MgResultTable resultTable_" in completion_h and "resultTable_.create" in completion_cpp,
    "analysis report uses shared renderer": "MgResultTable resultTable_" in analysis_h and "resultTable_.create" in analysis_cpp,
    "detailed report screens cannot drift to private lv tables": "lv_table_create" not in completion_cpp and "lv_table_create" not in analysis_cpp,
    "normal subd custom and cable-learn remain completion renderer": "beginTest(TestWorkflowController& workflow" in completion_h and "beginCableLearn(const CableProfile& profile" in completion_h,
    "normal report columns fill exact 976px visible width": "42, 118, 150, 250, 108, 128, 180" in completion_cpp and "Sum = 976 px" in completion_cpp,
    "backbone result route is origin-independent": "consumeAnalysisReportRequest()" in ino and "backboneReturn == BackboneReturn::AdvancedAnalysis" not in ino,
    "result returns to exact launch origin": "ReturnToOriginMenu" in analysis_h and "returnFromBackboneDemo();" in ino,
    "finite modules auto-report": all(x in backbone for x in ["BackboneModuleId::Scan128", "BackboneModuleId::Kelvin", "BackboneModuleId::SelfTestCalibration", "BackboneModuleId::Tdr", "BackboneModuleId::PairIntegrity", "BackboneModuleId::UsbC", "BackboneModuleId::PeDs", "BackboneModuleId::FixtureSpc", "BackboneModuleId::ComponentTest", "BackboneModuleId::HardwareValidation"]),
    "continuous utility modules expose snapshot result route": all(x in backbone for x in ["BackboneModuleId::SmartProbe", "BackboneModuleId::EnvironmentAwg", "BackboneModuleId::Voice", "BackboneModuleId::FlexGlitch"]),
    "multi-connector has standardized result route": "BackboneModuleId::MultiConnector" in backbone and "buildAnalysisReport();" in backbone,
    "GCC14 unsafe large aggregate reset remains forbidden": "analysisReport_ = AnalysisReportData{};" not in backbone,
    "archive intentionally remains browser not fake detailed table": "LAST-32-TEST REPORT WINDOW" in archive and "MgResultTable" not in archive,
    "workflow overview intentionally remains state summary": "TEST / PROFILE / RESULT WORKFLOW" in overview and "MgResultTable" not in overview,
}

for label, ok in checks.items():
    print(f"[{'PASS' if ok else 'FAIL'}] {label}")
failed = [label for label, ok in checks.items() if not ok]
if failed:
    raise SystemExit(f"{len(failed)} unified-result-table checks failed")
print(f"Unified result-table Fix4 contract: {len(checks)}/{len(checks)} PASS")
