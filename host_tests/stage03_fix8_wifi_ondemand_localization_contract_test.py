from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
ui = (root / "DocumentImportScreen.cpp").read_text(encoding="utf-8")
header = (root / "DocumentImportScreen.h").read_text(encoding="utf-8")

checks = []

def check(name, condition):
    ok = bool(condition)
    checks.append((name, ok))
    print(("PASS" if ok else "FAIL") + ": " + name)

begin = ui[ui.find("void DocumentImportScreen::begin"):ui.find("void DocumentImportScreen::buildUi")]
activate = ui[ui.find("void DocumentImportScreen::activate"):ui.find("void DocumentImportScreen::backCallback")]
callback = ui[ui.find("void DocumentImportScreen::wifiFallbackCallback"):ui.find("void DocumentImportScreen::reviewCallback")]

check("BLE-first import does not auto-start Wi-Fi AP",
      "startDocumentPortal()" not in begin and
      "clearDocumentItems()" in begin and
      "ble_->openSession()" in begin and
      "Wi-Fi fallback=OFF" in begin)
check("return to transfer screen does not auto-resume Wi-Fi",
      "resumeDocumentPortal()" not in activate)
check("Wi-Fi fallback is operator-controlled",
      "wifiFallbackCallback" in header and
      "startDocumentPortal()" in callback and
      "stopDocumentPortal()" in callback)
check("QR is hidden until Wi-Fi fallback is active",
      "LV_OBJ_FLAG_HIDDEN" in ui and
      "refreshWifiFallbackUi" in ui and
      "wifiFallbackButton_" in ui)
check("old hard-coded Turkish Wi-Fi prefix removed",
      'snprintf(buffer, sizeof(buffer), "Wi-Fi yedek: %s"' not in ui)
check("old hard-coded Turkish BLE connection suffix removed",
      '? "  [BAĞLI]" : ""' not in ui and
      '"  [CONNECTED]"' in ui and '"  [VERBONDEN]"' in ui and '"  [POŁĄCZONO]"' in ui)
check("dynamic Wi-Fi label has seven-language selector",
      'p4SelectText("Wi-Fi yedek", "Wi-Fi fallback", "Wi-Fi-reserve", "Wi-Fi-Ersatz"' in ui and
      '"Secours Wi-Fi", "Respaldo Wi-Fi", "Zapasowe Wi-Fi"' in ui)
check("Wi-Fi open button is localized in all seven languages",
      all(token in ui for token in [
          "WI-FI YEDEĞİNİ AÇ",
          "OPEN WI-FI FALLBACK",
          "WI-FI-RESERVE OPENEN",
          "WI-FI-ERSATZ ÖFFNEN",
          "OUVRIR LE SECOURS WI-FI",
          "ABRIR RESPALDO WI-FI",
          "WŁĄCZ ZAPASOWE WI-FI",
      ]))
check("Wi-Fi close button is localized in all seven languages",
      all(token in ui for token in [
          "WI-FI YEDEĞİNİ KAPAT",
          "CLOSE WI-FI FALLBACK",
          "WI-FI-RESERVE SLUITEN",
          "WI-FI-ERSATZ SCHLIESSEN",
          "FERMER LE SECOURS WI-FI",
          "CERRAR RESPALDO WI-FI",
          "WYŁĄCZ ZAPASOWE WI-FI",
      ]))
check("Wi-Fi credentials are hidden while fallback is off",
      "ssidLabel_, passwordLabel_, urlLabel_, sessionLabel_" in ui and
      "lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN)" in ui)
check("Wi-Fi upload can auto-open the operator summary",
      "wifiProfileReady" in ui and 'transport=%s' in ui)

failed = [name for name, ok in checks if not ok]
if failed:
    raise SystemExit("Stage03 Fix8 Wi-Fi/localization contract FAILED: " + ", ".join(failed))
print(f"Stage03 Fix8 Wi-Fi/localization contract: PASS ({len(checks)} checks)")
