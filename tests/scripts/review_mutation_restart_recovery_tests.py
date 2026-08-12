#!/usr/bin/env python3
"""Prove unresolved live-review recovery across two fresh processes."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import tempfile


def run(executable: pathlib.Path, phase: str, root: pathlib.Path) -> str:
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
    parser.add_argument("--executable", required=True, type=pathlib.Path)
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="bpr-review-mutation-restart-") as raw:
        root = pathlib.Path(raw)
        first = run(arguments.executable, "--write-unresolved", root)
        second = run(arguments.executable, "--recover-unresolved", root)
    if "restart-recovery-phase-one:unresolved" not in first:
        raise RuntimeError("first process did not persist the unresolved state")
    if "restart-recovery-phase-two:resolved-get-only" not in second:
        raise RuntimeError("second process did not resolve through GET-only recovery")
    print("review-mutation-restart-recovery-tests:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
