#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SCRIPT = PROJECT / "tools" / "repair_libsodium_component.py"


def run(root: Path, *extra: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--project-root", str(root), *extra],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def main() -> int:
    checks = 0
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        target = root / "managed_components" / "espressif__libsodium"
        target.mkdir(parents=True)
        (target / "marker.txt").write_text("broken", encoding="utf-8")

        result = run(root, "--dry-run")
        assert result.returncode == 0, result.stdout
        checks += 1
        assert target.exists()
        checks += 1
        assert "Only this managed component" in result.stdout
        checks += 1

        result = run(root)
        assert result.returncode == 0, result.stdout
        checks += 1
        assert not target.exists()
        checks += 1
        quarantined = list((root / ".mg_component_quarantine").glob("espressif__libsodium_*"))
        assert len(quarantined) == 1
        checks += 1
        assert (quarantined[0] / "marker.txt").read_text(encoding="utf-8") == "broken"
        checks += 1

        result = run(root)
        assert result.returncode == 0, result.stdout
        checks += 1
        assert "Nothing to repair" in result.stdout
        checks += 1

    print(f"MG targeted libsodium-repair tests: PASS ({checks}/{checks})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
