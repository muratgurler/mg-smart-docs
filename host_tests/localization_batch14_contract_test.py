from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
files = [
    "MainMenuScreen.cpp",
    "AdvancedAnalysisScreen.cpp",
    "CompletionReportScreen.cpp",
    "AnalysisReportScreen.cpp",
    "SettingsScreen.cpp",
    "ExtraFeaturesScreen.cpp",
    "NetworkStatusScreen.cpp",
    "WorkflowOverviewScreen.cpp",
    "WorkflowArchiveScreen.cpp",
    "WorkplaceServerSettingsScreen.cpp",
]
for name in files:
    text = (root / name).read_text(encoding="utf-8")
    forbidden = ["tr() ?", "turkish() ?", "P4Language::Turkish ?", "==P4Language::Turkish ?"]
    for token in forbidden:
        if token in text:
            raise SystemExit(f"{name}: legacy TR/EN fallback remains: {token}")

loc = (root / "P4Localization.cpp").read_text(encoding="utf-8")
if "const char* p4SelectText" not in loc:
    raise SystemExit("p4SelectText missing")

menu = (root / "MainMenuScreen.cpp").read_text(encoding="utf-8")
for language in ["KABELANALYSE", "ANALYSE DU CÂBLE", "ANÁLISIS DE CABLE", "ANALIZA KABLA"]:
    if language not in menu:
        raise SystemExit(f"main-menu localization missing: {language}")

# Permanent language touch guard must remain untouched during localization work.
for token in ["lv_obj_set_ext_click_area(button, 8)", "LV_OBJ_FLAG_PRESS_LOCK", "compact ? 72"]:
    if token not in menu:
        raise SystemExit(f"language touch regression: {token}")

print("Extra28 localization batch14 core contract passed (10 screens, 7-language selector, touch guard).")
