from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]

def read(name):
    return (root / name).read_text(encoding="utf-8")

ble_h = read("MgBleProfileServer.h")
ble = read("MgBleProfileServer.cpp")
portal_h = read("MapWebEditor.h")
portal = read("MapWebEditor.cpp")
prepared = read("PreparedProfileService.cpp")
ui = read("DocumentImportScreen.cpp")
ino = read("MG_Test_ESP32P4_JC1060_Beta_1_0.ino")
sdk = read("sdkconfig.defaults")
preflight = read("tools/pio_hosted_memory_config.py")
analysis_ui = read("DocumentAnalysisScreen.cpp")

checks = {
    "hosted NimBLE enabled": all(x in sdk for x in (
        "CONFIG_BT_ENABLED=y",
        "CONFIG_BT_CONTROLLER_DISABLED=y",
        "CONFIG_BT_NIMBLE_ENABLED=y",
        "CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE=y",
        "CONFIG_ESP_HOSTED_NIMBLE_HCI_VHCI=y",
        "CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y",
        "# CONFIG_BT_BLUEDROID_ENABLED is not set",
        "# CONFIG_BT_NIMBLE_TRANSPORT_UART is not set",
    )),
    "cached sdkconfig gets BLE settings": all(x in preflight for x in (
        '"CONFIG_BT_ENABLED": "y"',
        '"CONFIG_BT_CONTROLLER_DISABLED": "y"',
        '"CONFIG_BT_NIMBLE_ENABLED": "y"',
        '"CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE": "y"',
        '"CONFIG_ESP_HOSTED_NIMBLE_HCI_VHCI": "y"',
        '"CONFIG_BT_BLUEDROID_ENABLED": None',
        '"CONFIG_BT_NIMBLE_TRANSPORT_UART": None',
        'wanted = f"# {key} is not set" if value is None',
    )),
    "phone service UUID": '6d675000-7465-7374-6572-000000000001' in ble,
    "phone GATT UUID set": all(f'00000000000{i}' in ble for i in range(2, 7)),
    "MG-TEST advertisement name": 'MG-TEST-%04X' in ble,
    "session contract": 'MG1|' in ble and 'kMaxProfileBytes = 256U * 1024U' in ble_h and 'kMaxDataPayload = 180U' in ble_h,
    "START accepted before data": 'READY|' in ble and 'NO_START' in ble,
    "per chunk ACK": 'ACK|' in ble,
    "received and CRC status": 'RECEIVED|' in ble and 'CRC_OK' in ble,
    "electrical validation before final ready": 'PreparedProfileService::loadAndValidate' in ble and ble.index('PreparedProfileService::loadAndValidate') < ble.index('notifyStatus("PROFILE_READY")'),
    "terminal success": 'notifyStatus("PROFILE_READY")' in ble,
    "error framing": 'ERR|' in ble,
    "transport callback queues work": 'xQueueSend' in ble and 'xQueueReceive' in ble,
    "oversize callback writes are rejected not truncated": 'length > sizeof(event.data)' in ble and 'min(length' not in ble,
    "single shared profile store": 'commitExternalPreparedProfile' in ble and 'commitExternalPreparedProfile' in portal_h and '/mgdocs/profile_%03u.json' in portal,
    "same JSON + electrical pipeline": 'CanonicalWorkplaceJsonAdapter' in prepared and 'ProductionProfileContract::buildRuntimeProfile' in prepared,
    "BLE primary UI": 'Bluetooth ana aktarım | Wi-Fi yedek' in ui and 'kontrol ekranı otomatik açılır' in ui,
    "operator UI says MG Smart Tester not P4": '→ P4' not in analysis_ui and 'MG Smart Tester Cihazı' in analysis_ui,
    "BLE init before display": ino.index('bleProfileServer.begin') < ino.index('displayPort.begin'),
    "BLE tick in main loop": 'bleProfileServer.tick();' in ino,
    "BLE session opened only by import flow": 'documentImportScreen.begin(mapWebEditor, networkManager, bleProfileServer)' in ino,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("PASS" if ok else "FAIL") + ": " + name)
if failed:
    raise SystemExit("Stage03 BLE contract failed: " + ", ".join(failed))
print(f"ble_profile_stage03_contract_test: PASS ({len(checks)} checks)")
