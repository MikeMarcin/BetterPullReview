#!/usr/bin/env python3
"""Prove review recovery after both orderly exit and forced process death."""

from __future__ import annotations

import argparse
from pathlib import Path
import sqlite3
import sys


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "end_to_end"))
# [[BPR-1233]]. On Windows `mkdir(mode=0o700)` writes CPython's owner-only
# descriptor, which the submission store refuses; this writes the stores' own.
from bpr_narrow_owner_private import make_root_private  # noqa: E402


from process_kill_support import kill_at_checkpoint, run  # noqa: E402


def verify_killed_recovery(executable: Path, root: Path) -> None:
    for boundary in ("prepared", "witnessed", "published", "resolved", "published-moved"):
        course = root / f"killed-{boundary}"
        make_root_private(course)
        kill_boundary = "published" if boundary == "published-moved" else boundary
        kill_at_checkpoint(executable, f"--kill-{kill_boundary}", course)
        server = course / "server-reviews"
        expected_server = b"" if boundary in ("prepared", "witnessed") else b"45\n"
        before = server.read_bytes() if server.exists() else b""
        if before != expected_server:
            raise RuntimeError(f"{boundary}: incorrect server state at kill boundary")
        phase = f"--recover-killed-{boundary}"
        if f"{phase}:pass" not in run(executable, phase, course):
            raise RuntimeError(f"{boundary}: missing recovery result")
        # Reopen once more to verify the recovered receipt (or uncertainty)
        # survives another process, then independently check the send journal.
        terminal = boundary if boundary in ("prepared", "witnessed") else "resolved"
        phase = f"--recover-killed-{terminal}"
        if f"{phase}:pass" not in run(executable, phase, course):
            raise RuntimeError(f"{boundary}: missing durable repeat result")
        after = server.read_bytes() if server.exists() else b""
        if after != before:
            raise RuntimeError(f"{boundary}: recovery sent another review")
        database = course / "state/v1/review-mutation-recovery.sqlite3"
        # Production storage opens the killed database first, above. This
        # independent check must not repair it before that public path runs.
        connection = sqlite3.connect(database.as_uri() + "?mode=ro", uri=True)
        try:
            if connection.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
                raise RuntimeError(f"{boundary}: SQLite integrity failed")
            if connection.execute("PRAGMA foreign_key_check").fetchall():
                raise RuntimeError(f"{boundary}: SQLite foreign keys failed")
        finally:
            connection.close()
        print(f"review-submission-kill-recovery:{boundary}:pass")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    arguments = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-review-submission-restart-") as raw:
        root = Path(raw)
        make_root_private(root)
        verify_killed_recovery(arguments.executable, root)
        first = run(arguments.executable, "--write-unresolved", root)
        second = run(arguments.executable, "--recover-unresolved", root)
        moved_root = root / "moved"
        make_root_private(moved_root)
        run(arguments.executable, "--write-unresolved", moved_root)
        moved = run(arguments.executable, "--recover-moved", moved_root)
        prepared_root = root / "prepared"
        make_root_private(prepared_root)
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
