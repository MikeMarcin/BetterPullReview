#!/usr/bin/env python3
"""Prove unresolved live-review recovery across two fresh processes."""

from __future__ import annotations

from pathlib import Path
import sys
import argparse
import pathlib
import subprocess


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

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
    with contained_temporary_directory(prefix="bpr-review-mutation-restart-") as raw:
        root = pathlib.Path(raw)
        first = run(arguments.executable, "--write-unresolved", root)
        second = run(arguments.executable, "--recover-unresolved", root)
        prepared_root = root / "prepared"
        prepared_root.mkdir(mode=0o700)
        prepared_first = run(
            arguments.executable, "--write-prepared-crash", prepared_root
        )
        prepared_second = run(
            arguments.executable, "--recover-prepared", prepared_root
        )
        # [[BPR-0896]]. A reply is written by GraphQL and reconciled through
        # the thread it landed in; its restart must decide from that alone.
        reply_root = root / "reply"
        reply_root.mkdir(mode=0o700)
        reply_first = run(arguments.executable, "--write-unresolved-reply", reply_root)
        reply_second = run(
            arguments.executable, "--recover-unresolved-reply", reply_root
        )
    if "restart-recovery-phase-one:unresolved" not in first:
        raise RuntimeError("first process did not persist the unresolved state")
    if "restart-recovery-phase-two:resolved-get-only" not in second:
        raise RuntimeError("second process did not resolve through GET-only recovery")
    if "restart-prepared-phase-one:crash-before-post" not in prepared_first:
        raise RuntimeError("first prepared process did not stop before POST witness")
    if "restart-prepared-phase-two:zero-transport" not in prepared_second:
        raise RuntimeError("prepared restart observed transport or external C1")
    if "restart-reply-phase-one:unresolved" not in reply_first:
        raise RuntimeError("first reply process did not persist the unresolved state")
    if "restart-reply-phase-two:resolved-thread-only" not in reply_second:
        raise RuntimeError("second reply process did not resolve from its thread alone")
    print("review-mutation-restart-recovery-tests:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
