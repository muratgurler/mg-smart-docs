from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


ui = read("DocumentImportScreen.cpp")
analysis = read("DocumentAnalysisScreen.cpp")
ino = read("MG_Test_ESP32P4_JC1060_Beta_1_0.ino")
header = read("DocumentImportScreen.h")

checks = []

def check(name, condition):
    ok = bool(condition)
    checks.append((name, ok))
    print(("PASS" if ok else "FAIL") + ": " + name)

# Normal successful path: transfer screen -> one consolidated operator summary.
import_branch = ino[ino.find("if (activeScreen == ActiveScreen::DocumentImport)"):
                    ino.find("if (activeScreen == ActiveScreen::DocumentReview)")]
check("normal import flow skips engineering review screen",
      "showDocumentAnalysis();" in import_branch and "showDocumentReview();" not in import_branch)
check("summary auto-validates without READ PROFILE button",
      "if (validationReport_.ok)" in analysis and "importProfile();" in analysis and "PROFİLİ OKU" not in analysis)
check("single summary is vertically scrollable",
      "lv_obj_set_scroll_dir(panel, LV_DIR_VER);" in analysis and
      "LV_SCROLLBAR_MODE_AUTO" in analysis)
check("operator summary shows core production fields",
      all(text in analysis for text in ("Üretim no: ", "Profil: ", "Eşleme: ", "Sinyal pinleri: ")))
check("connections are included on same screen",
      "appendConnectionList(detail, record_, draftProfile_, draftMap_)" in analysis and
      '"Bağlantılar"' in analysis)
check("map review is optional",
      ('"HARİTA (İSTEĞE BAĞLI)"' in analysis or '"HARİTAYI GÖR / DÜZENLE"' in analysis))
check("single final approval wording",
      '"TESTE HAZIRLA"' in analysis and '"PROFİLİ UYGULA"' not in analysis)
check("apply remains fail-closed",
      "if (!profileReady_ || !editValidationError_.isEmpty()) lv_obj_add_state(applyButton_, LV_STATE_DISABLED);" in analysis and
      "if (profileReady_ && editValidationError_.isEmpty())" in analysis and
      "action_ = DocumentAnalysisAction::ApplyProfile;" in analysis)
check("transfer screen hides normal-flow engineering terms",
      "BLE session:" not in ui and "Manifest:" not in ui and "CRC " not in ui and
      "PROFİLİ İNCELE" not in ui)
check("summary hides JSON/CRC operator instructions",
      "PROFİLİ OKU ile JSON" not in analysis and "CRC bilgisini" not in analysis)
check("BLE auto-open waits for terminal PROFILE_READY state",
      "ble_->profileReady()" in ui and "autoOpenedForCurrentProfile_" in (ui + header))
check("return from summary goes directly to transfer screen",
      "documentImportScreen.activate();" in ino and "activeScreen = ActiveScreen::DocumentImport;" in ino)
check("physical GPIO/backbone layer untouched by workflow screen",
      "V33HardwareManager" not in analysis and "DigitalBackbone" not in analysis)

failed = [name for name, ok in checks if not ok]
if failed:
    raise SystemExit("Stage03 Fix5 operator-workflow contract FAILED: " + ", ".join(failed))
print(f"Stage03 Fix5 operator-workflow contract: PASS ({len(checks)} checks)")
