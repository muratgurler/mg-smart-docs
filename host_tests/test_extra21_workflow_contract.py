from pathlib import Path

root = Path(__file__).resolve().parents[1]
controller_h = (root / "TestWorkflowController.h").read_text(encoding="utf-8")
controller_cpp = (root / "TestWorkflowController.cpp").read_text(encoding="utf-8")
ui = (root / "WorkflowOverviewScreen.cpp").read_text(encoding="utf-8")
ino = (root / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")
backbone = (root / "BackboneDemoScreen.cpp").read_text(encoding="utf-8")

checks = {
    "unified controller exists": "class TestWorkflowController" in controller_h,
    "manual source": "ManualNormal" in controller_h and "ManualSubD" in controller_h,
    "document source": "DocumentImport" in controller_h,
    "cable learn source": "CableLearn" in controller_h,
    "PR source": "ProductionPr" in controller_h,
    "barcode MG source": "BarcodeMgProfile" in controller_h,
    "stale clear before source": "clearForNewSource" in controller_cpp,
    "barcode PR waits lookup": "TestWorkflowState::NeedsLookup" in controller_cpp,
    "no autostart invariant": "autoStartRequested = false" in controller_h and "AUTO=NO" in controller_cpp,
    "operator start gate": "markStartRequest" in controller_cpp and "startAllowed" in controller_cpp,
    "run state tracked": "observeRunState" in controller_cpp,
    "electrical result tracked": "electricalErrors" in controller_h,
    "document bridge": "prepareDocument" in ino,
    "manual bridge": "prepareManual" in ino,
    "scan result bridge": "observeRunState(scanSession.runState()" in ino,
    "barcode bridge": "acceptBarcodeClassification" in backbone,
    "PR resolved bridge": "acceptNetworkResolvedProfile" in backbone,
    "learned map bridge": "prepareCableLearn" in backbone,
    "workflow overview": "TEST / PROFIL / SONUC OMURGASI" in ui,
    "overview safety": "otomatik test BASLAMAZ" in ui,
    "reports routes overview": "showWorkflowOverview();" in ino,
    "SPC still reachable": "WorkflowOverviewAction::OpenSpc" in ino,
    "language touch regression files retained": (root / "host_tests" / "main_menu_language_touch_contract_test.py").exists(),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
if failed:
    raise SystemExit("Extra21 contract failed: " + ", ".join(failed))
print(f"Extra21 workflow contract: {len(checks)}/{len(checks)} PASS")
