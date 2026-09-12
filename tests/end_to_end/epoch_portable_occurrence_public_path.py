#!/usr/bin/env python3
"""Check portable update identity through two composed, independent SQLite roots."""

import argparse
from contextlib import closing
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile


def read_catalogue(application: Path, root: Path, budget_ms: int):
    root.mkdir(mode=0o700)
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "offscreen"
    command = [
        str(application),
        "--self-test",
        "--public-preview-probe",
        "--public-preview-enabled-course",
        "--github-fixture",
        "github-com",
        "--epoch-fixture-root",
        str(root),
        "--epoch-fixture-phase",
        "single",
        "--automated-run-budget-ms",
        str(budget_ms),
    ]
    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        timeout=budget_ms / 1000 + 13,
        env=environment,
        check=False,
    )
    if completed.returncode != 0 or "PUBLIC_PREVIEW_ENABLED_OK" not in completed.stdout:
        raise AssertionError(
            f"composed public read failed ({completed.returncode})\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )

    database = root / "state" / "v1" / "epoch-catalog.sqlite3"
    if not database.is_file():
        raise AssertionError(f"composed read created no catalogue: {database}")
    with closing(sqlite3.connect(database.as_uri() + "?mode=ro", uri=True)) as connection:
        occurrences = connection.execute(
            "SELECT hex(occurrence_id), kind, hex(event_id), hex(head_digest), "
            "date_source, date_utc, local_ordinal FROM epoch_occurrence "
            "ORDER BY date_utc, occurrence_id"
        ).fetchall()
        history = connection.execute(
            "SELECT incomplete, ambiguous FROM epoch_occurrence_history"
        ).fetchall()
        incarnations = connection.execute(
            "SELECT DISTINCT hex(incarnation) FROM epoch_occurrence"
        ).fetchall()
        epoch_count = connection.execute("SELECT count(*) FROM epoch").fetchone()[0]
    return occurrences, history, incarnations, epoch_count


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True, type=Path)
    parser.add_argument("--budget-ms", required=True, type=int)
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="bpr-portable-epoch-") as temporary:
        first = read_catalogue(arguments.application, Path(temporary) / "first", arguments.budget_ms)
        second = read_catalogue(arguments.application, Path(temporary) / "second", arguments.budget_ms)

    left, left_history, left_incarnations, left_epoch_count = first
    right, right_history, right_incarnations, right_epoch_count = second
    if len(left) != 3 or left != right:
        raise AssertionError(f"fresh catalogues disagree on evidenced updates: {left!r} vs {right!r}")
    if [entry[2] for entry in left] != ["0000000000016635", "0000000000016636", "0000000000016637"]:
        raise AssertionError(f"unexpected force-push event sequence: {left!r}")
    if not (left[0][3] == left[2][3] != left[1][3] and left[0][0] != left[2][0]):
        raise AssertionError("A→B→A did not retain separate portable identities for A")
    if not all(entry[1] == 0 and entry[4] == 1 and entry[6] is None for entry in left):
        raise AssertionError("historical events gained a fabricated local anchor or date source")
    if left_history != [(1, 1)] or right_history != [(1, 1)]:
        raise AssertionError("repeat-head ambiguity and incomplete history were not retained")
    if len(left_incarnations) != 1 or len(right_incarnations) != 1 or left_incarnations == right_incarnations:
        raise AssertionError("the two runs did not use independent catalogue incarnations")
    if left_epoch_count != 1 or right_epoch_count != 1:
        raise AssertionError("historical occurrences unexpectedly advanced live ordinals")
    print("EPOCH_PORTABLE_TWO_CATALOGUES_OK:3")


if __name__ == "__main__":
    main()
