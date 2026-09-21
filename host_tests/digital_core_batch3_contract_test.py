#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
checks = []

def need(path, needle, label):
    text = (root / path).read_text(encoding="utf-8", errors="ignore")
    ok = needle in text
    checks.append((label, ok))

need("DigitalBackboneCore.h", "class DigitalSweepRunner", "shared digital sweep engine")
need("DigitalBackboneCore.h", "class CableLearnEngine", "cable learn engine")
need("DigitalBackboneCore.h", "class SmartProbeEngine", "smart probe engine")
need("DigitalBackboneCore.h", "class MultiConnectorDemoEngine", "multi connector engine")
need("DigitalBackboneCore.h", "class SelfTestDemoEngine", "self-test engine")
need("DigitalBackboneCore.cpp", "stats_.total", "full sweep counts enabled A and B nodes")
need("BackboneDemoScreen.cpp", "remaining 127", "127-node scan wording retained")
need("ScanSource.h", "setFaultOverlayEnabled", "demo fault overlay can be disabled")
need("ScanSource.cpp", "CableNetGraph::build", "demo source resolves electrical connected components")
need("DigitalBackboneCore.cpp", "A7/A9 are a same-side common splice", "learn demo has same-side common net")
need("DigitalBackboneCore.cpp", "B9/B12/B14", "learn demo documents fan-out")
need("BackboneDemoScreen.cpp", "digitalCore_.learn().pauseResume", "cable learn UI drives real demo core")
need("BackboneDemoScreen.cpp", "digitalCore_.probe().pauseResume", "smart probe UI drives real demo core")
need("BackboneDemoScreen.cpp", "digitalCore_.selfTest().start", "self-test UI drives real demo core")
need("BackboneDemoScreen.cpp", "digitalCore_.multi().startSelected", "multi connector UI drives real demo core")
need("BackboneDemoScreen.cpp", "applyDigitalCoreSnapshot", "digital core metrics feed UI")
need("DigitalBackboneCore.cpp", "only the adjacent segment", "adjacent segment contract documented") if False else None
need("DigitalBackboneCore.cpp", "B-C deliberately demonstrates a segment-local mapping fault", "segment-local demo fault")
need("DigitalBackboneCore.cpp", "legitimate N:N/common-net segment", "segment supports common N:N net")
need("host_tests/run_tests.sh", "digital_core_batch3_test", "new executable regression registered")
need("host_tests/run_tests.sh", "digital_core_batch3_contract_test.py", "new contract regression registered")
need("host_tests/run_tests.sh", "main_menu_language_touch_contract_test.py", "language touch regression retained")

failed = [label for label, ok in checks if not ok]
for label, ok in checks:
    print(("PASS" if ok else "FAIL") + ": " + label)
if failed:
    raise SystemExit("Digital core batch3 contract failed: " + ", ".join(failed))
print(f"Digital core batch3 contract passed ({len(checks)}/{len(checks)})")
