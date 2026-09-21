#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SCRIPT = PROJECT / "tools" / "upload_existing_firmware.py"


def write(path: Path, data: bytes | str = b"x") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(data, str):
        path.write_text(data, encoding="utf-8")
    else:
        path.write_bytes(data)


def run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def main() -> int:
    checks = 0
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        build = root / ".pio" / "build" / "jc1060p4-core"
        write(root / "platformio.ini", "upload_speed = 460800\n")
        write(root / "partitions_p4.csv", "factory,app,factory,0x10000,0xC00000,\n")
        write(root / "main" / "dummy.cpp", "int dummy = 1;\n")
        write(build / "bootloader.bin", b"boot")
        write(build / "partitions.bin", b"part")
        write(build / "firmware.bin", b"firm")
        write(
            build / "flash_args",
            "--flash_mode dio --flash_freq 80m --flash_size 16MB "
            "0x2000 bootloader/bootloader.bin "
            "0x10000 mg_smart_tester_p4_core.bin "
            "0x8000 partition_table/partition-table.bin\n",
        )

        # Make the built firmware newer than source/config for the normal case.
        old = 1_700_000_000
        new = old + 100
        for p in (root / "main" / "dummy.cpp", root / "platformio.ini", root / "partitions_p4.csv"):
            os.utime(p, (old, old))
        for p in (build / "bootloader.bin", build / "partitions.bin", build / "firmware.bin"):
            os.utime(p, (new, new))

        result = run("--project-root", str(root), "--dry-run", "--port", "COM8")
        assert result.returncode == 0, result.stdout
        checks += 1
        assert "PlatformIO/CMake/Component Manager bypass" in result.stdout
        checks += 1
        assert "Port    : COM8" in result.stdout
        checks += 1
        assert "Baud    : 460800" in result.stdout
        checks += 1
        assert "mode=dio, freq=80m, size=16MB" in result.stdout
        checks += 1
        assert "boot=0x2000, partitions=0x8000, app=0x10000" in result.stdout
        checks += 1
        assert str(build / "bootloader.bin") in result.stdout
        checks += 1
        assert str(build / "partitions.bin") in result.stdout
        checks += 1
        assert str(build / "firmware.bin") in result.stdout
        checks += 1
        assert "platformio.exe" not in result.stdout.lower()
        checks += 1
        assert "[DRY RUN] Nothing was flashed." in result.stdout
        checks += 1

        # App-only must omit bootloader and partition image arguments.
        result = run("--project-root", str(root), "--dry-run", "--app-only")
        assert result.returncode == 0, result.stdout
        checks += 1
        cmd_line = result.stdout.split("Command:\n", 1)[1].split("\n", 1)[0]
        assert str(build / "firmware.bin") in cmd_line
        checks += 1
        assert str(build / "bootloader.bin") not in cmd_line
        checks += 1
        assert str(build / "partitions.bin") not in cmd_line
        checks += 1

        # Stale source must block accidental upload.
        future = new + 100
        os.utime(root / "main" / "dummy.cpp", (future, future))
        result = run("--project-root", str(root), "--dry-run")
        assert result.returncode == 3, result.stdout
        checks += 1
        assert "Source/config files are newer than firmware.bin" in result.stdout
        checks += 1

        # Explicit stale override remains available.
        result = run("--project-root", str(root), "--dry-run", "--allow-stale")
        assert result.returncode == 0, result.stdout
        checks += 1

        # Missing required binary must fail before esptool.
        (build / "bootloader.bin").unlink()
        result = run("--project-root", str(root), "--dry-run", "--allow-stale")
        assert result.returncode == 2, result.stdout
        checks += 1
        assert "Required built binary is missing" in result.stdout
        checks += 1

    print(f"MG direct-upload reliability tests: PASS ({checks}/{checks})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
