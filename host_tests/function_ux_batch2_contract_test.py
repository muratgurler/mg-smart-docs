#!/usr/bin/env python3
from pathlib import Path
import sys

project = Path(sys.argv[1]).resolve()
screen = (project / "BackboneDemoScreen.cpp").read_text(encoding="utf-8")
header = (project / "BackboneDemoScreen.h").read_text(encoding="utf-8")
ino = (project / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")
main = (project / "MainMenuScreen.cpp").read_text(encoding="utf-8")

def req(token, where="screen"):
    src = screen if where == "screen" else header if where == "header" else ino
    if token not in src:
        raise AssertionError(f"{where}: missing {token!r}")

# No one-size-fits-all operator strip.
for stale in ('Action::StartPause', 'Action::Step', 'Action::Reset',
              'tr() ? "SIFIRLA" : "RESET"'):
    if stale in screen or stale in header:
        raise AssertionError(f"stale universal operator control remains: {stale}")

# Every top-level module/workflow has controls matching its role.
for token in (
    # Scan / digital core
    "TEK ADIM", "YENİ TARAMA", "sender LOW, kalan 127 node INPUT/read",
    # Kelvin / self test
    "ZERO / OFFSET", "AKIM SEÇ", "REFERANS / ΔR",
    "HIZLI TEST", "TAM TEST", "KALİBRASYON", "SONUÇLAR",
    # TDR / Pair
    "PİN -", "PİN +", "TDR ÖLÇÜM", "FREKANS", "ÇİFT +",
    # USB-C / PE-dS
    "KABLOYU OKU", "PROFİLLE KARŞILAŞTIR", "BOND KURALI",
    # Fixture / Probe
    "FİKSTÜR TESTİ", "MASTER REF", "PROBU BAŞLAT", "NETİ GÖSTER",
    # Network / Voice / Environment
    "AĞ DURUMU", "PR ARA", "SUNUCU TESTİ", "HI MG: ON",
    "MİKROFON TESTİ", "CANLI: AÇIK", "KAYIT: AÇIK", "WIRE SPEC",
    # Barcode / component / hardware
    "KOD OKU", "MANUEL GİR", "TÜR SEÇ", "GPIO TESTİ", "BUS TESTİ", "BOOT TESTİ",
    # Learn / multiconnector / glitch
    "ÖĞRENMEYİ BAŞLAT", "HARİTAYI İNCELE", "SEGMENT SEÇ", "SONRAKİ SEGMENT",
    "İZLEMEYİ BAŞLAT", "LATCH TEMİZLE", "OLAY LİSTESİ",
):
    req(token)

# Safety statements must stay explicit in demo UI.
for token in (
    "real PCB outputs disabled",
    "gerçek PCB çıkışları kapalı",
    "otomatik START yok",
    "sahte map yazılmaz",
    "GPIO5 rezerve",
    "No Gerber freeze",
):
    req(token)

# Shared screen remains future hardware-driver seam, not a throwaway mockup.
for token in ("DemoDriver -> HardwareDriver", "Real drivers will receive equivalent selections"):
    req(token, "header")

# Existing 128-node Connection Faults path still uses ScanScreen; the new role-aware
# backbone does not replace the mature scan demo.
for token in ("consumeConnectionScanRequest", "startScanUi(ScanProfileKind::Normal)"):
    req(token, "ino")

# Permanent main-menu language touch regression remains frozen.
for token in (
    "lv_obj_set_size(button, compact ? 340 : 312, compact ? 72 : 118)",
    "lv_obj_set_ext_click_area(button, 8)",
    "LV_OBJ_FLAG_PRESS_LOCK",
    "lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE)",
):
    if token not in main:
        raise AssertionError(f"language-touch regression missing: {token}")

print("Extra16 function-specific UX batch2 contract passed.")
