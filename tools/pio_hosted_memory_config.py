# MG Smart Tester P4 core
# Force the low-memory ESP-Hosted/SDIO Kconfig values into an already-generated
# PlatformIO per-environment sdkconfig before CMake runs.  sdkconfig.defaults is
# sufficient for a fresh environment, but PlatformIO preserves sdkconfig.<env>
# between builds and existing values override changed defaults.

Import("env")

if env.IsIntegrationDump():
    Return()

from pathlib import Path
import re

PROJECT_DIR = Path(env.subst("$PROJECT_DIR")).resolve()
BUILD_DIR = Path(env.subst("$BUILD_DIR")).resolve()
PIOENV = env.subst("$PIOENV")

# Upload-only guard: if a successfully built firmware already exists, do not
# touch sdkconfig. Rewriting sdkconfig here forces a full ESP-IDF/CMake
# reconfigure before every upload and can make the upload task appear hung.
if "upload" in COMMAND_LINE_TARGETS and (BUILD_DIR / "firmware.bin").is_file():
    print("[P4-CORE] upload-only: existing firmware.bin found; sdkconfig preflight skipped")
    Return()

FORCED = {
    "CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL": "32768",
    "CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP": "y",
    "CONFIG_ESP_HOSTED_SDIO_TX_Q_SIZE": "8",
    "CONFIG_ESP_HOSTED_SDIO_RX_Q_SIZE": "8",
    "CONFIG_ESP_HOSTED_DFLT_TASK_FROM_SPIRAM": "y",
    "CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM": "8",
    "CONFIG_WIFI_RMT_DYNAMIC_RX_BUFFER_NUM": "32",
    "CONFIG_WIFI_RMT_DYNAMIC_TX_BUFFER_NUM": "32",
    "CONFIG_WIFI_RMT_TX_BA_WIN": "16",
    "CONFIG_WIFI_RMT_RX_BA_WIN": "16",
    "CONFIG_LWIP_TCP_SND_BUF_DEFAULT": "32768",
    "CONFIG_LWIP_TCP_WND_DEFAULT": "32768",
    "CONFIG_LWIP_TCP_RECVMBOX_SIZE": "32",
    "CONFIG_LWIP_UDP_RECVMBOX_SIZE": "32",
    "CONFIG_LWIP_TCPIP_RECVMBOX_SIZE": "32",
    # Stage03 BLE-first profile transfer. Keep these values synchronized even
    # when PlatformIO reuses sdkconfig.<env> from the pre-BLE firmware.
    "CONFIG_BT_ENABLED": "y",
    "CONFIG_BT_CONTROLLER_DISABLED": "y",
    "CONFIG_BT_NIMBLE_ENABLED": "y",
    "CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED": "y",
    "CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE": "y",
    "CONFIG_ESP_HOSTED_NIMBLE_HCI_VHCI": "y",
    # Boolean false values are emitted using Kconfig's canonical not-set form.
    "CONFIG_BT_BLUEDROID_ENABLED": None,
    "CONFIG_BT_NIMBLE_TRANSPORT_UART": None,
}


def patch_sdkconfig(path: Path) -> bool:
    if not path.is_file():
        return False
    text = path.read_text(encoding="utf-8", errors="strict")
    original = text
    for key, value in FORCED.items():
        wanted = f"# {key} is not set" if value is None else f"{key}={value}"
        # Replace either active assignment or '# KEY is not set'.
        pattern = re.compile(rf"(?m)^(?:{re.escape(key)}=.*|# {re.escape(key)} is not set)$")
        if pattern.search(text):
            text = pattern.sub(wanted, text, count=1)
        else:
            if text and not text.endswith("\n"):
                text += "\n"
            text += wanted + "\n"
    if text != original:
        path.write_text(text, encoding="utf-8", newline="\n")
        print(f"[P4-CORE] patched active sdkconfig: {path}")
    else:
        print(f"[P4-CORE] active sdkconfig already current: {path}")
    return True


print("[P4-CORE] ESP-Hosted low-memory + Stage03 BLE configuration preflight")
found = False
for candidate in (
    PROJECT_DIR / f"sdkconfig.{PIOENV}",
    PROJECT_DIR / "sdkconfig",
    BUILD_DIR / "sdkconfig",
):
    found = patch_sdkconfig(candidate) or found

if not found:
    print("[P4-CORE] no generated sdkconfig yet; sdkconfig.defaults will seed P4 core values")

print("[P4-CORE] SDIO TX/RX=8 | DMA reserve=32KiB | hosted NimBLE=ON | local controller=OFF")
