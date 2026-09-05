#!/usr/bin/env python3
"""Prove unresolved live-review recovery across two fresh processes."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def run(executable: Path, phase: str, root: Path) -> str:
    completed = subprocess.run(
        [str(executable), phase, str(root)],
        check=False,
        capture_output=True,
        text=True,
        timeout=15,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{phase} failed ({completed.returncode}):\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    arguments = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-review-submission-restart-") as raw:
        root = Path(raw)
        first = run(arguments.executable, "--write-unresolved", root)
        second = run(arguments.executable, "--recover-unresolved", root)
        moved_root = root / "moved"
        moved_root.mkdir(mode=0o700)
        run(arguments.executable, "--write-unresolved", moved_root)
        moved = run(arguments.executable, "--recover-moved", moved_root)
        prepared_root = root / "prepared"
        prepared_root.mkdir(mode=0o700)
        prepared_first = run(
            arguments.executable, "--write-prepared", prepared_root
        )
        prepared_second = run(
            arguments.executable, "--recover-prepared", prepared_root
        )
    for output, phase in (
        (first, "--write-unresolved"),
        (second, "--recover-unresolved"),
        (moved, "--recover-moved"),
        (prepared_first, "--write-prepared"),
        (prepared_second, "--recover-prepared"),
    ):
        if f"{phase}:pass" not in output:
            raise RuntimeError(f"missing successful phase: {phase}")
    print("review-submission-restart-recovery-tests:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
