# MG Smart Tester — ESP32-P4 / JC1060 — Beta 1.0

This directory is the clean **Beta 1.0** baseline of the MG Smart Tester ESP32-P4 firmware. Historical Fix/Extra/Stage note files have been removed from the production package; their implemented behavior remains in the source and regression tests.

## Current architecture

- JC1060P470C_I_W_Y / ESP32-P4 user interface, 1024×600 touch display.
- Seven-language UI with the protected large/reliable language-selector touch area.
- Normal cable test, custom electrical maps, cable learning and adjacent-segment multi-connector workflows.
- Bidirectional A→B / B→A scan model with open, short, wrong mapping and high-resistance classifications.
- PE and drain/shield (`dS`) support controlled by the active profile.
- Scrollable detailed reports, physical-fault summary for strict 1:1 maps, and expected/measured result graph.
- Production identity/profile workflow and persistent result/profile archive.
- Android/iOS companion application performs document/photo/PDF recognition. The P4 accepts only a prepared electrical profile and validates it before test preparation.
- BLE through the onboard ESP32-C6 is the primary phone-profile transport. Temporary Wi-Fi/AP upload remains an operator-selected fallback.

## Real hardware status

The intended digital I/O expander is **MCP23S17 over SPI**. It is not MCP23017.

The external carrier PCB is not yet validated and the final GPIO/bus assignment is not frozen. Therefore `V33BoardConfig.h` intentionally keeps:

```text
kCarrierPcbValidated = false
kExternalPinMapFrozen = false
kHardwareEnabled = false
```

With those gates false, firmware must not drive the external carrier. The current scan and backbone demonstrations remain deterministic simulation sources and are explicitly presented as DEMO/SIM rather than physical measurements. Candidate GPIO assignments may remain documented for PCB review, but they are not authorization to access hardware.

The physical hardware layer can be enabled only after both PCB validation and pin freeze are complete. A compile-time assertion prevents accidental enabling before those two conditions are true.

## Version identity

Canonical firmware version values are in `ProjectVersion.h`:

```text
Release: Beta 1.0
ID:      MG_TESTER_BETA_1_0
```

The boot banner and build-system status message identify this baseline as Beta 1.0. Future corrections should be applied on top of this baseline instead of carrying the previous historical note-file chain forward.

## Build target

PlatformIO environment:

```text
jc1060p4-core
```

Framework: ESP-IDF with Arduino as a managed component.

Main dependencies include:

- `espressif/arduino-esp32 3.3.6`
- `lvgl/lvgl 8.4.0`

Normal workflow is **Build → Upload**. A clean build is only needed when there is an actual cache/dependency reason.

## Mobile profile transfer

The P4 does not run OCR or document-recognition AI. The phone application produces the prepared electrical profile before transfer.

Primary/fallback transport model:

```text
Primary:  BLE profile transfer via onboard ESP32-C6
Fallback: temporary local Wi-Fi/AP upload, enabled by operator action
```

Stable HTTP fallback API:

```text
GET  /api/profile/session
POST /api/profile/upload?token=<session-token>
POST /api/profile/chunk?token=<session-token>&id=<transfer-id>&part=<n>&parts=<count>&name=<name>&size=<total-bytes>
```

Only one prepared profile, up to 256 KiB, is accepted per transfer session. File integrity, JSON parsing and electrical-topology validation remain internal; the production operator sees the simplified profile summary and `TESTE HAZIRLA` flow.

See `MG_MOBILE_PROFILE_JSON_V1.md` and `mobile_profile_example.json`.

## Regression protection

`host_tests/` is intentionally retained. Some regression-test filenames still contain historical fix/stage identifiers because they identify the bug that the test permanently guards; these are executable regression tests, not historical release-note files.

The permanent main-menu language selector constraint remains protected: 72 px height at Y=504, +8 px extended click area, PRESS_LOCK, and non-clickable transparent child row/labels.

## Beta 1.0 Fix1 - MCP23S17 digital hardware seam

Fix1 prepares the real 64 A + 64 B digital matrix without enabling an unvalidated PCB.

- `DigitalNodeMap.h` freezes only the logical mapping: A uses U1-U4, B uses U5-U8; node 0=PE, 1..62=signal, 63=dS.
- `Mcp23s17Hal.*` implements shared-CS MCP23S17 HAEN/addressing, safe all-input boot state, pull-up sensing, one-sender-LOW scan, full eight-device readback, and release-to-input cleanup.
- `Mcp23s17ScanSource.*` converts the physical 128-node observation into the existing `ScanSource` contract. Every sender scan reads the other 127 nodes, so same-side shorts remain detectable.
- `RoutedScanSource` keeps DEMO and REAL backends separate. Beta 1.0 Fix1 still boots in DEMO because the carrier PCB and external pin map are not validated/frozen.
- `Mcp23s17PhysicalBackend` cannot touch GPIO/SPI while `kHardwareEnabled=false`. The existing compile-time guard still requires both PCB validation and pin freeze before physical enable.
- Kelvin, TDR, Pair Integrity, Flex/Glitch and USB-C physical drivers are intentionally not enabled by this fix.

Run `host_tests/run_tests.sh` to validate the MCP23S17 protocol/mapping and the existing regression suite.
