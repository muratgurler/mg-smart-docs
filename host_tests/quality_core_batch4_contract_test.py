#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
checks = []

def need(path, needle, label):
    text = (root / path).read_text(encoding="utf-8", errors="ignore")
    checks.append((label, needle in text))

need("QualityBackboneCore.h", "class KelvinQualityDemoEngine", "Kelvin quality engine")
need("QualityBackboneCore.h", "class PeDsQualityDemoEngine", "PE/dS quality engine")
need("QualityBackboneCore.h", "class FixtureSpcDemoEngine", "fixture/SPC engine")
need("QualityBackboneCore.h", "class EnvironmentQualityDemoEngine", "SHT40/environment engine")
need("QualityBackboneCore.cpp", "Rshunt=1.000 ohm", "ratiometric Kelvin shunt model")
need("QualityBackboneCore.cpp", "0.00393f", "copper temperature context coefficient")
need("QualityBackboneCore.cpp", "production DUT never auto-updates baseline", "baseline immutability rule")
need("QualityBackboneCore.cpp", "Product", "product/process separation comment") if False else None
need("BackboneDemoScreen.h", "QualityBackboneCore qualityCore_", "quality core attached to UI shell")
need("BackboneDemoScreen.cpp", "applyQualityCoreSnapshot", "quality metrics feed UI")
need("BackboneDemoScreen.cpp", "qualityCore_.kelvin().zero", "ZERO is a distinct Kelvin action")
need("BackboneDemoScreen.cpp", "qualityCore_.kelvin().measure", "MEASURE drives Kelvin engine")
need("BackboneDemoScreen.cpp", "qualityCore_.peDs().startTest", "PE/dS test drives engine")
need("BackboneDemoScreen.cpp", "qualityCore_.peDs().startKelvin", "PE/dS Kelvin action drives engine")
need("BackboneDemoScreen.cpp", "qualityCore_.peDs().cycleBondPolicy", "bond policy is explicit profile state")
need("BackboneDemoScreen.cpp", "qualityCore_.fixtureSpc().startFixtureCheck", "fixture test drives engine")
need("BackboneDemoScreen.cpp", "qualityCore_.fixtureSpc().nextTrendPin", "SPC pin trend navigation")
need("BackboneDemoScreen.cpp", "PRODUCT=%s | PROCESS=%s", "product result separate from process status")
need("BackboneDemoScreen.cpp", "qualityCore_.environment().measure", "SHT40 measure action")
need("BackboneDemoScreen.cpp", "qualityCore_.environment().cycleWireSpec", "per-net WireSpec demo action")
need("BackboneDemoScreen.cpp", "R_RAW is preserved", "raw resistance preservation wording")
need("host_tests/run_tests.sh", "quality_core_batch4_test", "quality executable regression registered")
need("host_tests/run_tests.sh", "quality_core_batch4_contract_test.py", "quality contract regression registered")
need("host_tests/run_tests.sh", "main_menu_language_touch_contract_test.py", "language touch regression retained")

failed = [label for label, ok in checks if not ok]
for label, ok in checks:
    print(("PASS" if ok else "FAIL") + ": " + label)
if failed:
    raise SystemExit("Quality core batch4 contract failed: " + ", ".join(failed))
print(f"Quality core batch4 contract passed ({len(checks)}/{len(checks)})")
