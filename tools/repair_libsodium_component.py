#!/usr/bin/env python3
"""One-time targeted repair for an interrupted ESP-IDF libsodium component copy.

Does not invoke PlatformIO and does not delete the whole managed_components tree.
It moves only espressif__libsodium aside so Component Manager can recreate it on
next Build.
"""
from __future__ import annotations

import argparse
import datetime as dt
import shutil
import sys
from pathlib import Path

COMPONENT = "espressif__libsodium"


def main() -> int:
    parser = argparse.ArgumentParser(description="Quarantine a broken managed libsodium component.")
    parser.add_argument("--project-root", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    root = (args.project_root or Path(__file__).resolve().parents[1]).resolve()
    source = root / "managed_components" / COMPONENT
    quarantine_root = root / ".mg_component_quarantine"

    print("[MG COMPONENT REPAIR] Targeted libsodium repair")
    print(f"Project : {root}")
    print(f"Target  : {source}")

    if not source.exists():
        print("Nothing to repair: managed libsodium folder is not present.")
        return 0

    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    destination = quarantine_root / f"{COMPONENT}_{stamp}"
    suffix = 1
    while destination.exists():
        destination = quarantine_root / f"{COMPONENT}_{stamp}_{suffix}"
        suffix += 1

    print(f"Move to : {destination}")
    print("Only this managed component will be moved. .pio and other components are untouched.")

    if args.dry_run:
        print("[DRY RUN] Nothing was moved.")
        return 0

    try:
        quarantine_root.mkdir(parents=True, exist_ok=True)
        shutil.move(str(source), str(destination))
    except PermissionError as exc:
        print("\nERROR: Windows still has a file handle open in this component.", file=sys.stderr)
        print("Do not restart VS Code first. Make sure no Build/Upload/metadata task is active,", file=sys.stderr)
        print("then run this repair again. If needed, end only stale cmake/ninja/python child processes.", file=sys.stderr)
        print(f"Details: {exc}", file=sys.stderr)
        return 32
    except OSError as exc:
        print(f"\nERROR: Could not quarantine component: {exc}", file=sys.stderr)
        return 5

    print("\nRepair staging complete.")
    print("Next step: run one normal PlatformIO Build. Component Manager should recreate libsodium cleanly.")
    print("After that Build succeeds, the quarantined folder can be deleted later.")
    print("Do NOT Clean just for this repair.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
