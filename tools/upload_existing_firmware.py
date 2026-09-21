#!/usr/bin/env python3
"""MG Smart Tester direct uploader.

Uploads already-built ESP32-P4 binaries without invoking PlatformIO/SCons/CMake
or ESP-IDF Component Manager. This avoids managed_components file-lock failures
on Windows after a successful build.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Iterable

DEFAULT_ENV = "jc1060p4-core"
DEFAULT_BOOT_OFFSET = "0x2000"
DEFAULT_PART_OFFSET = "0x8000"
DEFAULT_APP_OFFSET = "0x10000"
DEFAULT_FLASH_MODE = "dio"
DEFAULT_FLASH_FREQ = "80m"
DEFAULT_FLASH_SIZE = "16MB"
DEFAULT_BAUD = "460800"


def _norm_hex(value: str) -> str:
    value = value.strip()
    try:
        return f"0x{int(value, 0):x}"
    except ValueError:
        return value


def _extract_option(text: str, name: str, default: str) -> str:
    # Accept both legacy --flash_mode and esptool-v5 --flash-mode spellings.
    pattern = rf"--{re.escape(name).replace('_', '[-_]')}\s+([^\s]+)"
    match = re.search(pattern, text, flags=re.IGNORECASE)
    return match.group(1) if match else default


def _extract_offset(text: str, kind: str, default: str) -> str:
    # flash_args consists of address/path pairs. We only reuse the address;
    # PlatformIO's top-level .bin files are deliberately used for the paths.
    pairs = re.findall(r"(0x[0-9a-fA-F]+)\s+([^\s]+\.bin)", text)
    for address, raw_path in pairs:
        path = raw_path.replace("\\", "/").lower()
        if kind == "boot" and "bootloader" in path:
            return _norm_hex(address)
        if kind == "part" and ("partition" in path or "partitions" in path):
            return _norm_hex(address)
        if kind == "app" and "bootloader" not in path and "partition" not in path:
            return _norm_hex(address)
    return default


def _read_upload_speed(platformio_ini: Path) -> str:
    try:
        text = platformio_ini.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return DEFAULT_BAUD
    match = re.search(r"(?mi)^\s*upload_speed\s*=\s*(\d+)\s*$", text)
    return match.group(1) if match else DEFAULT_BAUD


def _candidate_sources(root: Path) -> Iterable[Path]:
    for relative in (
        "CMakeLists.txt",
        "platformio.ini",
        "sdkconfig.defaults",
        "partitions_p4.csv",
        "main/idf_component.yml",
    ):
        p = root / relative
        if p.is_file():
            yield p

    main_dir = root / "main"
    if main_dir.is_dir():
        allowed = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ino"}
        for p in main_dir.rglob("*"):
            if p.is_file() and p.suffix.lower() in allowed:
                yield p


def _newer_sources(root: Path, firmware: Path) -> list[Path]:
    try:
        fw_time = firmware.stat().st_mtime
    except OSError:
        return []
    newer: list[Path] = []
    for src in _candidate_sources(root):
        try:
            if src.stat().st_mtime > fw_time + 1.0:
                newer.append(src)
        except OSError:
            pass
    return sorted(newer, key=lambda p: p.stat().st_mtime, reverse=True)


def _quote_for_display(arg: str) -> str:
    if not arg or any(ch.isspace() for ch in arg):
        return f'"{arg}"'
    return arg


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Upload existing MG Smart Tester ESP32-P4 binaries directly with esptool."
    )
    parser.add_argument("--port", help="Serial port, e.g. COM8. Omit for esptool auto-detect.")
    parser.add_argument("--baud", help="Upload baud. Defaults to platformio.ini upload_speed.")
    parser.add_argument("--env", default=DEFAULT_ENV, help="PlatformIO environment/build directory name.")
    parser.add_argument("--app-only", action="store_true", help="Write firmware.bin only; keep bootloader/partitions unchanged.")
    parser.add_argument("--allow-stale", action="store_true", help="Allow upload even if source files are newer than firmware.bin.")
    parser.add_argument("--dry-run", action="store_true", help="Print the esptool command without flashing.")
    parser.add_argument("--project-root", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()

    root = (args.project_root or Path(__file__).resolve().parents[1]).resolve()
    build = root / ".pio" / "build" / args.env
    firmware = build / "firmware.bin"
    bootloader = build / "bootloader.bin"
    partitions = build / "partitions.bin"
    flash_args = build / "flash_args"

    print("[MG DIRECT UPLOAD] PlatformIO/CMake/Component Manager bypass")
    print(f"Project : {root}")
    print(f"Build   : {build}")

    required = [firmware] if args.app_only else [bootloader, partitions, firmware]
    missing = [p for p in required if not p.is_file()]
    if missing:
        print("\nERROR: Required built binary is missing:", file=sys.stderr)
        for p in missing:
            print(f"  - {p}", file=sys.stderr)
        print("Run a successful PlatformIO Build first. Do NOT Clean just for this uploader.", file=sys.stderr)
        return 2

    stale = _newer_sources(root, firmware)
    if stale and not args.allow_stale:
        print("\nERROR: Source/config files are newer than firmware.bin.", file=sys.stderr)
        print("The existing firmware may be stale. Build first, then run direct upload.", file=sys.stderr)
        for p in stale[:8]:
            try:
                shown = p.relative_to(root)
            except ValueError:
                shown = p
            print(f"  - {shown}", file=sys.stderr)
        if len(stale) > 8:
            print(f"  ... and {len(stale) - 8} more", file=sys.stderr)
        print("Use --allow-stale only when you intentionally want the older binary.", file=sys.stderr)
        return 3

    text = ""
    if flash_args.is_file():
        try:
            text = flash_args.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            text = ""

    flash_mode = _extract_option(text, "flash_mode", DEFAULT_FLASH_MODE)
    flash_freq = _extract_option(text, "flash_freq", DEFAULT_FLASH_FREQ)
    flash_size = _extract_option(text, "flash_size", DEFAULT_FLASH_SIZE)
    boot_offset = _extract_offset(text, "boot", DEFAULT_BOOT_OFFSET)
    part_offset = _extract_offset(text, "part", DEFAULT_PART_OFFSET)
    app_offset = _extract_offset(text, "app", DEFAULT_APP_OFFSET)
    baud = args.baud or _read_upload_speed(root / "platformio.ini")

    command = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        "esp32p4",
    ]
    if args.port:
        command += ["--port", args.port]
    command += [
        "--baud",
        str(baud),
        "--before",
        "default-reset",
        "--after",
        "hard-reset",
        "write-flash",
        "--flash-mode",
        flash_mode,
        "--flash-freq",
        flash_freq,
        "--flash-size",
        flash_size,
    ]

    if args.app_only:
        command += [app_offset, str(firmware)]
    else:
        command += [
            boot_offset,
            str(bootloader),
            part_offset,
            str(partitions),
            app_offset,
            str(firmware),
        ]

    print(f"Mode    : {'APP ONLY' if args.app_only else 'FULL (bootloader + partitions + app)'}")
    print(f"Port    : {args.port or 'AUTO'}")
    print(f"Baud    : {baud}")
    print(f"Flash   : mode={flash_mode}, freq={flash_freq}, size={flash_size}")
    if args.app_only:
        print(f"Address : app={app_offset}")
    else:
        print(f"Address : boot={boot_offset}, partitions={part_offset}, app={app_offset}")
    print("\nCommand:")
    print(" ".join(_quote_for_display(x) for x in command))

    if args.dry_run:
        print("\n[DRY RUN] Nothing was flashed.")
        return 0

    print("\nStarting direct flash...\n")
    try:
        completed = subprocess.run(command, cwd=build, check=False)
    except OSError as exc:
        print(f"ERROR: Could not start esptool: {exc}", file=sys.stderr)
        return 4

    if completed.returncode == 0:
        print("\n[MG DIRECT UPLOAD] SUCCESS")
    else:
        print(f"\n[MG DIRECT UPLOAD] FAILED (exit code {completed.returncode})", file=sys.stderr)
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
