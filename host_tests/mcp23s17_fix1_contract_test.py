#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])


def text(rel):
    return (root / rel).read_text(encoding="utf-8", errors="ignore")


def require(rel, token, label):
    if token not in text(rel):
        raise AssertionError(f"{label}: missing {token!r} in {rel}")


require("ProjectVersion.h", 'kPatchName[] = "Fix1"', "Fix1 version tag")
require("ProjectVersion.h", 'kBuildLabel[] = "Beta 1.0 Fix1"', "Fix1 build label")
require("V33BoardConfig.h", "kHardwareEnabled = false", "physical output gate stays OFF")
require("V33BoardConfig.h", "kCarrierPcbValidated = false", "PCB is still unvalidated")
require("V33BoardConfig.h", "kExternalPinMapFrozen = false", "pin freeze is still open")

for token in (
    "A_PE must map to U1",
    "A_dS must map to U4",
    "B_PE must map to U5",
    "B_dS must map to U8",
):
    require("DigitalNodeMap.h", token, "64+64 node map")

for token in (
    "enableHardwareAddressing",
    "verifyAddresses",
    "safeAllInputs",
    "driveNodeLow",
    "readAllLowMasks",
    "scanFromNode",
    "kIoconHaen",
):
    require("Mcp23s17Hal.h", token, "MCP23S17 HAL contract")

for token in (
    "all eight devices accept this same write",
    "Input pull-ups make every undriven node HIGH",
    "direction is changed last",
):
    require("Mcp23s17Hal.cpp", token, "MCP23S17 safety contract")

for token in (
    "other 127 nodes are observed",
    "same-side shorts",
):
    require("Mcp23s17ScanSource.cpp", token, "127-node scan regression")

for token in (
    "class RoutedScanSource",
    "attachPhysical",
    "preferPhysical",
    "usingPhysical",
):
    require("ScanSource.h", token, "DEMO/REAL routing seam")

require("Mcp23s17PhysicalBackend.cpp", "if (!kHardwareEnabled) return false;",
        "hard physical safety boundary")
require("V33HardwareManager.cpp", "MCP23S17 HAL ready", "manager exposes prepared HAL")
require("MG_Test_ESP32P4_JC1060_Beta_1_0.ino", "scanSource.attachPhysical",
        "app attaches physical source without forcing it active")
require("MG_Test_ESP32P4_JC1060_Beta_1_0.ino", "scanSource.preferPhysical",
        "app selects physical source only behind hardware gate")
require("host_tests/run_tests.sh", "mcp23s17_fix1_test", "Fix1 executable test registered")
require("host_tests/run_tests.sh", "mcp23s17_fix1_contract_test.py", "Fix1 contract test registered")

print("Beta 1.0 Fix1 MCP23S17 contract: PASS")
