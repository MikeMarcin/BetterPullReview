#!/usr/bin/env python3
"""[[BPR-1178]]. RC59-07's publication crash, drift and recovery matrix.

Each case is one course root and several real processes of
`test_review_cycle_publication_matrix`: the desktop's cycle host publishes to a
simulated GitHub whose state is a file in the root, and dies where the case
says, killed by this driver (SIGKILL, TerminateProcess) while it waits at that
boundary. A restart reads before it writes, and sends what remains only after
the reviewer confirms the plan again; another process then reopens the outcome
and sends nothing. The store a killed process left is checked by SQLite before
the next process opens it. No live GitHub is contacted.

[[BPR-1273]] adds the promotion courses: after one clean publication, Add
Notes to Review Tour adds the cycle's five notes to the pull request's tour,
and its process is killed before or after a note's stop is durable (or loses
an answer). A restart asks again; a last process reopens and finds every stop
exactly once. The tour store a killed process left is checked by SQLite too.
"""

from __future__ import annotations

import argparse
from contextlib import closing
from pathlib import Path
import sqlite3
import subprocess
import sys


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "end_to_end"))
from bpr_narrow_owner_private import make_root_private  # noqa: E402

from process_kill_support import kill_at_checkpoint  # noqa: E402


# The case whose restart is itself killed while recovery reads GitHub.
RECOVERY_KILLED = "recovery-killed"
RECOVERY_KILL_POINT = "after-read-blob"


def run(executable: Path, phase: str, root: Path, arguments: tuple[str, ...]) -> str:
    completed = subprocess.run(
        [str(executable), phase, *arguments, str(root)],
        check=False,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if completed.returncode != 0 or f"{phase}:" not in completed.stdout:
        raise RuntimeError(
            f"{phase} failed ({completed.returncode}):\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed.stdout


def kill(executable: Path, phase: str, root: Path, arguments: tuple[str, ...]) -> None:
    kill_at_checkpoint(executable, phase, root, arguments=arguments, checkpoint_timeout=120)
    # The next kill in this root waits for its own marker.
    (root / "kill-ready").unlink()


def check_store(root: Path, case: str) -> None:
    """The store a killed process left opens read-only and is consistent.

    Run before the next process, so production storage is the first writer
    to open it after the death, as on a real restart.
    """
    databases = sorted((root / "profile").rglob("review-cycles.sqlite3"))
    if len(databases) != 1:
        raise RuntimeError(f"{case}: expected one cycle store, found {databases}")
    with closing(sqlite3.connect(databases[0].as_uri() + "?mode=ro", uri=True)) as connection:
        if connection.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
            raise RuntimeError(f"{case}: SQLite integrity failed after the kill")
        if connection.execute("PRAGMA foreign_key_check").fetchall():
            raise RuntimeError(f"{case}: SQLite foreign keys failed after the kill")
        if connection.execute("SELECT count(*) FROM publications").fetchone() != (1,):
            raise RuntimeError(f"{case}: one recorded publication expected after the kill")


def check_tours(root: Path, case: str) -> None:
    """The tour store a killed promotion left opens read-only and is consistent."""
    databases = sorted((root / "profile").rglob("review-tours.sqlite3"))
    if len(databases) != 1:
        raise RuntimeError(f"{case}: expected one tour store, found {databases}")
    with closing(sqlite3.connect(databases[0].as_uri() + "?mode=ro", uri=True)) as connection:
        if connection.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
            raise RuntimeError(f"{case}: tour store integrity failed after the kill")
        if connection.execute("SELECT count(*) FROM targets").fetchone() != (1,):
            raise RuntimeError(f"{case}: one recorded tour expected after the kill")
        if connection.execute("SELECT count(*) FROM tours").fetchone()[0] > 1:
            raise RuntimeError(f"{case}: at most one tour draft expected after the kill")


def cases(executable: Path, listing: str = "--cases") -> list[tuple[str, str]]:
    listed = subprocess.run(
        [str(executable), listing], check=True, capture_output=True, text=True, timeout=30
    ).stdout.split("\n")
    return [tuple(line.split(" ", 1)) for line in listed if line]


def run_promotion(executable: Path, git: Path, root: Path, case: str, point: str) -> None:
    course = root / case
    for directory in (course, course / "profile", course / "git-isolation"):
        make_root_private(directory)
    common = ("--git", str(git), "--case", case)
    run(executable, "--publish", course, common)
    if point == "-":
        run(executable, "--promote", course, common)
    else:
        kill(executable, "--promote", course, (*common, "--kill", point))
        check_store(course, case)
        check_tours(course, case)
        run(executable, "--promote-again", course, common)
    run(executable, "--promote-verify", course, common)
    print(f"review-cycle-publication-matrix:{case}:pass", flush=True)


def run_case(executable: Path, git: Path, root: Path, case: str, point: str) -> None:
    course = root / case
    for directory in (course, course / "profile", course / "git-isolation"):
        make_root_private(directory)
    common = ("--git", str(git), "--case", case)
    if point == "-":
        run(executable, "--publish", course, common)
    else:
        kill(executable, "--publish", course, (*common, "--kill", point))
        check_store(course, case)
    if case == RECOVERY_KILLED:
        kill(executable, "--restart", course, (*common, "--kill", RECOVERY_KILL_POINT))
        check_store(course, case)
        run(executable, "--restart-again", course, common)
    else:
        run(executable, "--restart", course, common)
    run(executable, "--verify", course, common)
    print(f"review-cycle-publication-matrix:{case}:pass", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--git", required=True, type=Path)
    parser.add_argument("--only", action="append", default=[], help="run only these cases")
    arguments = parser.parse_args()
    selected = [
        (case, point)
        for case, point in cases(arguments.executable)
        if not arguments.only or case in arguments.only
    ]
    promotions = [
        (case, point)
        for case, point in cases(arguments.executable, "--promotion-cases")
        if not arguments.only or case in arguments.only
    ]
    if not selected and not promotions:
        raise RuntimeError("no matrix case selected")
    with contained_temporary_directory(prefix="bpr-publication-matrix-") as raw:
        # The stores refuse a path through a link, and macOS reaches the
        # temporary directory through /var -> /private/var.
        root = Path(raw).resolve()
        make_root_private(root)
        for case, point in selected:
            run_case(arguments.executable, arguments.git, root, case, point)
        for case, point in promotions:
            run_promotion(arguments.executable, arguments.git, root, case, point)
    print(
        "review-cycle-publication-matrix-tests:pass "
        f"({len(selected)} cases, {len(promotions)} promotion cases)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
