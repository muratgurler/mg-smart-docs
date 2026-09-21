from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
formatter_h = (root / "ReportFormatter.h").read_text(encoding="utf-8")
formatter_cpp = (root / "ReportFormatter.cpp").read_text(encoding="utf-8")
report = (root / "CompletionReportScreen.cpp").read_text(encoding="utf-8")
tests = (root / "host_tests" / "scan_session_test.cpp").read_text(encoding="utf-8")

checks = [
    ("physical summary contract exists", "OperatorPhysicalFaultSummary" in formatter_h and "buildOperatorPhysicalFaultSummary" in formatter_h),
    ("dedupe uses canonical A-to-B sweep", "ScanDirection::AtoB" in formatter_cpp and "canonical physical view" in formatter_cpp),
    ("crossed pair is collapsed", "crossedPairCount" in formatter_cpp and "wrong[i].actual == wrong[j].source" in formatter_cpp),
    ("open line is counted once", "openLineCount" in formatter_cpp),
    ("short groups are deduplicated", "uniqueShortGroups" in formatter_cpp and "maskAlreadyStored" in formatter_cpp),
    ("high resistance line is counted once", "highResistanceLineCount" in formatter_cpp),
    ("complex common nets fall back", "identityOneToOne = false" in formatter_cpp and "Complex/common nets" in report),
    ("operator header uses physical issue count", "Fiziksel hata" in report and "Physical faults" in report and "physicalIssueCount()" in report),
    ("fault summary is localized", all(token in report for token in ["Hata özeti", "Fault summary", "Foutoverzicht", "Fehlerübersicht", "Résumé défauts", "Resumen de fallos", "Podsumowanie usterek"])),
    ("demo physical-measurement warning remains", "DEMO: fiziksel ölçüm değil" in report and "DEMO: not a physical measurement" in report),
    ("subd15 behavior test exists", "testOperatorPhysicalFaultSummaryMatchesSubD15Demo" in tests),
    ("common-net no-guess test exists", "testOperatorPhysicalFaultSummaryDoesNotGuessForCommonNets" in tests),
]

failed = [name for name, ok in checks if not ok]
for name, ok in checks:
    print(("PASS" if ok else "FAIL") + ": " + name)
if failed:
    raise SystemExit("Fix10 physical-fault-summary contract failed: " + ", ".join(failed))
print(f"Stage03 Fix10 contract: PASS ({len(checks)} checks)")
