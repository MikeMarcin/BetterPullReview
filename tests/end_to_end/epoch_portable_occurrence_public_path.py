#!/usr/bin/env python3
"""Check portable update identity through two composed, independent SQLite roots."""

import argparse
from contextlib import closing
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile


def read_catalogue(application: Path, root: Path, budget_ms: int,
                   *, empty_timeline: bool = False, unfetchable_first: bool = False):
    root.mkdir(mode=0o700, exist_ok=True)
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
    if empty_timeline:
        command.append("--public-preview-empty-timeline-course")
    else:
        command.append("--public-preview-historical-course")
    if unfetchable_first:
        command.append("--public-preview-unfetchable-first-head-course")
    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        timeout=budget_ms / 1000 + 13,
        env=environment,
        check=False,
    )
    if (completed.returncode != 0 or "PUBLIC_PREVIEW_ENABLED_OK" not in completed.stdout
            or (not empty_timeline and "PUBLIC_PREVIEW_HISTORICAL_HANDLE_OK" not in completed.stdout)):
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
            "date_source, date_utc, local_ordinal, historical_base, derived_base FROM epoch_occurrence "
            "ORDER BY date_utc, occurrence_id"
        ).fetchall()
        history = connection.execute(
            "SELECT incomplete, ambiguous FROM epoch_occurrence_history"
        ).fetchall()
        incarnations = connection.execute(
            "SELECT DISTINCT hex(incarnation) FROM epoch_occurrence"
        ).fetchall()
        epoch_count = connection.execute("SELECT count(*) FROM epoch").fetchone()[0]
        live_ordinal = connection.execute(
            "SELECT latest_live_ordinal FROM pull_request_scope"
        ).fetchone()[0]
        imports = connection.execute(
            "SELECT count(*) FROM epoch WHERE origin = 3"
        ).fetchone()[0]
    return occurrences, history, incarnations, epoch_count, live_ordinal, imports


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True, type=Path)
    parser.add_argument("--budget-ms", required=True, type=int)
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="bpr-portable-epoch-") as temporary:
        first_root = Path(temporary) / "first"
        seed = read_catalogue(arguments.application, first_root, arguments.budget_ms,
                              empty_timeline=True)
        first = read_catalogue(arguments.application, first_root, arguments.budget_ms)
        second = read_catalogue(arguments.application, Path(temporary) / "second", arguments.budget_ms)
        gap = read_catalogue(arguments.application, Path(temporary) / "gap", arguments.budget_ms,
                             unfetchable_first=True)

    if seed[3:] != (1, 1, 0):
        raise AssertionError("empty-timeline seed did not leave one unchanged live epoch")

    left, left_history, left_incarnations, left_epoch_count, left_live, left_imports = first
    right, right_history, right_incarnations, right_epoch_count, right_live, right_imports = second
    if len(left) != 3 or [row[:6] for row in left] != [row[:6] for row in right]:
        raise AssertionError(f"fresh catalogues disagree on evidenced updates: {left!r} vs {right!r}")
    if [entry[2] for entry in left] != ["0000000000016635", "0000000000016636", "0000000000016637"]:
        raise AssertionError(f"unexpected force-push event sequence: {left!r}")
    if not (left[0][3] == left[2][3] != left[1][3] and left[0][0] != left[2][0]):
        raise AssertionError("A→B→A did not retain separate portable identities for A")
    if not all(entry[1] == 0 and entry[4] == 1 and entry[7] is None for entry in left):
        raise AssertionError("historical event dates or original-base provenance changed")
    for events in (left, right):
        if not any(entry[6] is not None and entry[8] is not None for entry in events):
            raise AssertionError("historical event still lacks a derived comparison and local epoch")
    if left_history != [(1, 1)] or right_history != [(1, 1)]:
        raise AssertionError("repeat-head ambiguity and incomplete history were not retained")
    if len(left_incarnations) != 1 or len(right_incarnations) != 1 or left_incarnations == right_incarnations:
        raise AssertionError("the two runs did not use independent catalogue incarnations")
    if not (left_live == right_live == 1 and left_imports >= 1 and right_imports >= 1
            and left_epoch_count == left_imports + 1 and right_epoch_count == right_imports + 1):
        raise AssertionError("historical append changed live authority or failed to retain an imported epoch")
    gap_events = gap[0]
    if (gap_events[0][6] is not None or gap_events[1][6] is None or gap[4] != 1):
        raise AssertionError("unfetchable first head starved the later retained update")
    print("EPOCH_PORTABLE_TWO_CATALOGUES_OK:3")


if __name__ == "__main__":
    main()
