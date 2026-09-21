#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
checks = []

def need(path, needle, label):
    text = (root / path).read_text(encoding="utf-8", errors="ignore")
    checks.append((label, needle in text))

need("HighSpeedBackboneCore.h", "class TdrDemoEngine", "TDR engine")
need("HighSpeedBackboneCore.h", "class PairIntegrityDemoEngine", "Pair Integrity engine")
need("HighSpeedBackboneCore.h", "class UsbCIdentityDemoEngine", "USB-C identity engine")
need("HighSpeedBackboneCore.h", "class FlexGlitchDemoEngine", "Flex/glitch engine")
need("HighSpeedBackboneCore.h", "HighSpeedBackboneCore", "group core")
need("HighSpeedBackboneCore.h", "ADG904/TDC7200", "TDR hardware seam wording")
need("HighSpeedBackboneCore.cpp", "result_.vop = 0.66f", "TDR VOP calibration model")
need("HighSpeedBackboneCore.cpp", "Pair 13", "split-pair deterministic fault")
need("HighSpeedBackboneCore.cpp", "250000U", "250 kHz Pair frequency")
need("HighSpeedBackboneCore.h", "SOP'", "USB-C SOP prime identity flow")
need("HighSpeedBackboneCore.h", "never models a 5 A/EPR load test", "USB-C no high-power load rule")
need("HighSpeedBackboneCore.cpp", "snapshot_.latched = true", "hardware latch event model")
need("HighSpeedBackboneCore.h", "does not erase the recorded event history", "latch clear preserves event history")
need("BackboneDemoScreen.h", "HighSpeedBackboneCore highSpeedCore_", "high-speed core attached to UI")
need("BackboneDemoScreen.cpp", "applyHighSpeedCoreSnapshot", "high-speed snapshot feeds UI")
need("BackboneDemoScreen.cpp", "highSpeedCore_.tdr().start", "TDR action drives engine")
need("BackboneDemoScreen.cpp", "highSpeedCore_.pair().start", "Pair action drives engine")
need("BackboneDemoScreen.cpp", "highSpeedCore_.usbC().readCable", "USB-C read action drives engine")
need("BackboneDemoScreen.cpp", "highSpeedCore_.usbC().compareProfile", "USB-C compare is explicit")
need("BackboneDemoScreen.cpp", "highSpeedCore_.flexGlitch().pauseResume", "Flex monitor drives engine")
need("BackboneDemoScreen.cpp", "highSpeedCore_.flexGlitch().clearLatch", "Flex latch clear action")
need("BackboneDemoScreen.cpp", "not CAT6 cert", "Pair scope is not certification")
need("BackboneDemoScreen.cpp", "mechanical-length certification", "TDR is not mechanical length certification")
need("host_tests/run_tests.sh", "high_speed_core_batch5_test", "high-speed executable regression registered")
need("host_tests/run_tests.sh", "high_speed_core_batch5_contract_test.py", "high-speed contract registered")
need("host_tests/run_tests.sh", "main_menu_language_touch_contract_test.py", "language selector regression retained")

failed = [label for label, ok in checks if not ok]
for label, ok in checks:
    print(("PASS" if ok else "FAIL") + ": " + label)
if failed:
    raise SystemExit("High-speed core batch5 contract failed: " + ", ".join(failed))
print(f"High-speed core batch5 contract passed ({len(checks)}/{len(checks)})")
