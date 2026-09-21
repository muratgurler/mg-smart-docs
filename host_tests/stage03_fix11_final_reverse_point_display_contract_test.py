#!/usr/bin/env python3
from pathlib import Path
import sys

project = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
config = (project / "ConfigP4.h").read_text(encoding="utf-8")
session = (project / "ScanSession.cpp").read_text(encoding="utf-8")
main = (project / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")
tests = (project / "host_tests" / "scan_session_test.cpp").read_text(encoding="utf-8")

checks = [
    ("minimum final-point dwell", "kFinalScanPointDisplayMinMs = 300UL" in config),
    ("frame guard", "kFinalScanPointDisplayMinMs >= kUiFrameIntervalMs" in config),
    ("completion hold state", "completionDisplayHoldActive" in main),
    ("completion hold timestamp", "completionDisplayHoldStartedMs" in main),
    ("finite scan completion gate", "const bool finiteScanComplete" in main),
    ("selected speed participates", "scanSession.scanStepIntervalMs()" in main),
    ("hold takes max selected/minimum", "selectedStepMs > kFinalScanPointDisplayMinMs" in main),
    ("report waits for hold", "completionNowMs - completionDisplayHoldStartedMs >= finalPointHoldMs" in main),
    ("display snapshot preserved", "DO NOT overwrite" in session and "displayDirection_ = ScanDirection::AtoB;" not in session[session.index("runState_ = RunState::Complete;"):session.index("void ScanSession::beginNextContinuousCycle")]),
    ("final return direction regression", "assert(session.displayDirection() == ScanDirection::BtoA);" in tests),
    ("final B1 shown", "assert(session.displayTestIndexB() == 1U);" in tests),
    ("final A1 shown", "assert(session.displayTestIndexA() == 1U);" in tests),
]

failed = [name for name, ok in checks if not ok]
if failed:
    raise AssertionError("Fix11 contract failed: " + ", ".join(failed))
print(f"Stage03 Fix11 final reverse-point display contract: {len(checks)}/{len(checks)} PASS")
