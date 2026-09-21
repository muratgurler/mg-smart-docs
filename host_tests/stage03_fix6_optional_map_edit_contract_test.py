from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


ui = read("DocumentImportScreen.cpp")
analysis = read("DocumentAnalysisScreen.cpp")
analysis_h = read("DocumentAnalysisScreen.h")
editor = read("MapEditorScreen.cpp")
editor_h = read("MapEditorScreen.h")
ino = read("MG_Test_ESP32P4_JC1060_Beta_1_0.ino")

checks = []

def check(name, condition):
    ok = bool(condition)
    checks.append((name, ok))
    print(("PASS" if ok else "FAIL") + ": " + name)

check("completed transfer no longer says waiting for phone",
      "count > 0U" in ui and "Telefon aktarımı tamamlandı" in ui and
      ui.find("count > 0U") < ui.find("Telefon bağlantısı bekleniyor"))
check("successful BLE auto-open remains terminal PROFILE_READY gated",
      "ble_->profileReady()" in ui and "autoOpenedForCurrentProfile_" in ui)
check("profile summary exposes optional view/edit map action",
      '"HARİTAYI GÖR / DÜZENLE"' in analysis)
check("document map review starts locked",
      "editUnlocked_ = context_ != MapEditorContext::DocumentReview;" in editor and
      "bool editUnlocked_ = true;" in editor_h)
check("locked review requires explicit EDIT action",
      "UnlockEdit" in editor_h and '"DÜZENLE"' in editor and
      "LocalAction::Back, LocalAction::UnlockEdit, LocalAction::Graph" in editor)
check("locked review cannot mutate target pins",
      "context_ == MapEditorContext::DocumentReview && !editUnlocked_" in editor and
      "void MapEditorScreen::toggleTarget" in editor)
check("document one-to-one fill respects active profile points",
      "pointEnabledForSide('A', i) && pointEnabledForSide('B', i)" in editor and
      "working_.connectAtoB(i, i, true);" in editor)
check("operator-edited map is heap-backed before validation",
      "std::unique_ptr<ProductionProfileRecord> candidate" in analysis and
      "new (std::nothrow) ProductionProfileRecord(record_)" in analysis)
check("operator-edited map is electrically revalidated",
      "ProductionProfileContract::validate(*candidate)" in analysis and
      "operator map revalidated=OK" in analysis)
check("rejected edit does not overwrite last valid map",
      analysis.find("if (!validation.valid)") < analysis.find("record_ = *candidate;") and
      "editValidationError_" in analysis_h)
check("final prepare-for-test is blocked after rejected edit",
      "!editValidationError_.isEmpty()" in analysis and
      "lv_obj_add_state(applyButton_, LV_STATE_DISABLED)" in analysis)
check("rejected edit returns to summary with actionable state",
      "if ((action == MapEditorAction::DocumentDone && accepted) || !accepted)" in ino and
      "documentAnalysisScreen.activate();" in ino)
check("accepted edit is marked saved in editor",
      "if (accepted)" in ino and "mapEditorScreen.markSaved();" in ino)
check("normal operator path still has one final TESTE HAZIRLA action",
      '"TESTE HAZIRLA"' in analysis and '"PROFİLİ UYGULA"' not in analysis)
check("physical GPIO/backbone remains outside Fix6",
      "V33HardwareManager" not in analysis and "DigitalBackbone" not in analysis and
      "V33HardwareManager" not in editor)

failed = [name for name, ok in checks if not ok]
if failed:
    raise SystemExit("Stage03 Fix6 optional-map-edit contract FAILED: " + ", ".join(failed))
print(f"Stage03 Fix6 optional-map-edit contract: PASS ({len(checks)} checks)")
