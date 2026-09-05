#!/usr/bin/env python3
"""Prove review recovery after both orderly exit and forced process death."""

from __future__ import annotations

import argparse
from pathlib import Path
import os
import signal
import sqlite3
import subprocess
import sys
import time


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


def kill_at_checkpoint(executable: Path, phase: str, root: Path) -> None:
    with subprocess.Popen(
        [str(executable), phase, str(root)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    ) as process:
        try:
            deadline = time.monotonic() + 15
            marker = root / "kill-ready"
            while not marker.exists() or marker.read_text() != phase:
                if process.poll() is not None:
                    stdout, stderr = process.communicate(timeout=5)
                    raise RuntimeError(
                        f"{phase} exited before checkpoint ({process.returncode}):\n"
                        f"{stdout}\n{stderr}"
                    )
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"{phase} did not reach checkpoint")
                time.sleep(0.01)
            if process.poll() is not None:
                raise RuntimeError(f"{phase} exited instead of waiting to be killed")
            # kill() uses SIGKILL on POSIX and TerminateProcess on Windows;
            # stdin remains open until after death, so the C++ scope cannot exit.
            process.kill()
            process.wait(timeout=5)
            stdout, stderr = process.communicate(timeout=5)
            expected = -signal.SIGKILL if os.name == "posix" else 1
            if process.returncode != expected:
                raise RuntimeError(
                    f"{phase} was not forcibly killed ({process.returncode}):\n"
                    f"{stdout}\n{stderr}"
                )
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


def verify_killed_recovery(executable: Path, root: Path) -> None:
    for boundary in ("prepared", "witnessed", "published", "resolved", "published-moved"):
        course = root / f"killed-{boundary}"
        course.mkdir(mode=0o700)
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
        verify_killed_recovery(arguments.executable, root)
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
