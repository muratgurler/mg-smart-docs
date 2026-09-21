#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]

def read(rel):
    return (root / rel).read_text(encoding="utf-8")

version = read("ProjectVersion.h")
board = read("V33BoardConfig.h")
hw = read("V33HardwareManager.cpp")
scan_h = read("ScanSource.h")
ino = read("MG_Test_ESP32P4_JC1060_Beta_1_0.ino")
cmake = read("CMakeLists.txt")

checks = {
    "canonical Beta 1.0 release name": 'kReleaseName[] = "Beta 1.0"' in version,
    "canonical Beta 1.0 release id": 'kReleaseId[] = "MG_TESTER_BETA_1_0"' in version,
    "boot uses canonical release name": "mg::p4::build::kReleaseName" in ino,
    "build message says Beta 1.0": "MG Smart Tester Beta 1.0" in cmake,
    "real digital expander is MCP23S17": "MCP23S17" in board and "MCP23S17" in hw and "MCP23S17" in scan_h,
    "stale MCP23017 removed from production sources": "MCP23017" not in scan_h and "MCP23017" not in hw,
    "PCB remains unvalidated": "kCarrierPcbValidated = false" in board,
    "pin map remains unfrozen": "kExternalPinMapFrozen = false" in board,
    "physical hardware remains disabled": "kHardwareEnabled = false" in board,
    "compile-time hardware enable guard exists": "Physical hardware cannot be enabled before PCB validation and pin freeze" in board,
}

# Historical top-level release-note debris must not return to the Beta baseline.
allowed_docs = {"README.md", "MG_MOBILE_PROFILE_JSON_V1.md", "THIRD_PARTY_NOTICES.md"}
legacy = []
for p in root.iterdir():
    if not p.is_file():
        continue
    upper = p.name.upper()
    if p.suffix.lower() in {".txt", ".md"} and p.name not in allowed_docs and p.name != "CMakeLists.txt":
        if any(token in upper for token in ("FIX", "EXTRA", "STAGE", "CHANGED_FILES", "BUILD_VERIFICATION", "NOTES")):
            legacy.append(p.name)
    if ".HOTFIX" in upper:
        legacy.append(p.name)
checks["historical top-level fix/stage/extra note files removed"] = not legacy

failed = []
for label, ok in checks.items():
    print(("PASS" if ok else "FAIL") + ": " + label)
    if not ok:
        failed.append(label)
if legacy:
    print("legacy files:", ", ".join(sorted(legacy)))
if failed:
    raise SystemExit("Beta 1.0 baseline contract failed: " + ", ".join(failed))
print(f"Beta 1.0 baseline contract: PASS ({len(checks)} checks)")
