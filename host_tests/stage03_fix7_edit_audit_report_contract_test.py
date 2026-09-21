from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
analysis_h = (root / "DocumentAnalysisScreen.h").read_text(encoding="utf-8")
analysis_cpp = (root / "DocumentAnalysisScreen.cpp").read_text(encoding="utf-8")
workflow_h = (root / "TestWorkflowController.h").read_text(encoding="utf-8")
workflow_cpp = (root / "TestWorkflowController.cpp").read_text(encoding="utf-8")
main = (root / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")
report = (root / "CompletionReportScreen.cpp").read_text(encoding="utf-8")

checks = [
    ("original phone map retained", "originalMap_" in analysis_h and "originalRecord_" in analysis_h),
    ("restore original action exists", "ORİJİNALE DÖN" in analysis_cpp and "restoreOriginalMap" in analysis_cpp),
    ("edited map requires explicit second confirmation", "DEĞİŞİKLİĞİ ONAYLA" in analysis_cpp and "operatorEditConfirmed_" in analysis_cpp),
    ("first final-button press confirms rather than starts", "operator map change confirmation=OK" in analysis_cpp),
    ("edited mapping gets old-to-new summary", "appendMapDiffSummary" in analysis_cpp and "Değişiklik özeti" in analysis_cpp),
    ("document workflow accepts full identity", "const TestWorkflowIdentity& identity" in workflow_h and "TestWorkflowController::prepareDocument" in workflow_cpp),
    ("mobile PR propagated into workflow", "mobileIdentity.productionPr" in main and "mobileRecord.productionPr" in main),
    ("actual mobile profile id propagated", "mobileIdentity.profileId" in main and "mobileRecord.profileId" in main),
    ("generic mobile customer placeholder is hidden", 'strcmp(mobileRecord.customerReference, "MOBILE-IMPORT")' in main),
    ("generic unspecified revision is hidden", 'strcmp(mobileRecord.revision, "UNSPECIFIED")' in main),
    ("operator-edited source is auditable", "MOBILE-BLE-EDITED" in main),
    ("simulation report is explicit", "DEMO/SIM" in report and "SİMÜLASYON" in report),
    ("simulated resistance values are marked", "R (mOhm) [SIM]" in report),
]
failed = [name for name, ok in checks if not ok]
for name, ok in checks:
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
if failed:
    raise SystemExit("Stage03 Fix7 contract FAILED: " + ", ".join(failed))
print(f"Stage03 Fix7 contract: PASS ({len(checks)} checks)")
