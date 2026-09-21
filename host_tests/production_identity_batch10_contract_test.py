#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
h = (root / "ProductionProfileContract.h").read_text(encoding="utf-8")
c = (root / "ProductionProfileContract.cpp").read_text(encoding="utf-8")
bh = (root / "ProductionWorkflowBridge.h").read_text(encoding="utf-8")
bc = (root / "ProductionWorkflowBridge.cpp").read_text(encoding="utf-8")
wh = (root / "TestWorkflowController.h").read_text(encoding="utf-8")
wc = (root / "TestWorkflowController.cpp").read_text(encoding="utf-8")
lang = (root / "host_tests" / "main_menu_language_touch_contract_test.py").read_text(encoding="utf-8")

checks = {
    "dual identity fields": "productionPr[24]" in h and "customerReference[40]" in h,
    "revision separate": "revision[20]" in h,
    "profile id separate": "profileId[40]" in h,
    "x1 to a default": "X1 -> A by default" in h,
    "x2 to b default": "X2 -> B by default" in h,
    "full 64 node topology": "aToB[kTestPointsPerSide]" in h,
    "same side common nets": "sameSideA[kTestPointsPerSide]" in h and "sameSideB[kTestPointsPerSide]" in h,
    "nc spare accepted": "Zero A->B mask is valid" in h,
    "pr cross check": "ProductionPrMismatch" in h and "expectedPr" in c,
    "customer cross check": "CustomerReferenceMismatch" in h and "expectedCustomerReference" in c,
    "disabled target guard": "DisabledTargetReferenced" in c,
    "same side symmetry guard": "SameSideAsymmetric" in c,
    "source preserving bridge": "acceptNetworkResolvedProfileForSource" in wh and "sourceCode" in wc,
    "customer source preserved": "BarcodeCustomerReference" in bc,
    "operator start only": "operator START required" in bc,
    "bridge auto start false": "out.autoStartRequested = false" in bc,
    "main language regression test retained": "PRESS_LOCK" in lang and "72" in lang,
}
failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("Production identity batch10 contract failed: " + ", ".join(failed))
print(f"Production identity batch10 contract passed: {len(checks)}/{len(checks)}")
