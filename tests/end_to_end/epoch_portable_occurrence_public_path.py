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
                   *, empty_timeline: bool = False, unfetchable_first: bool = False,
                   course: str = "", phase: str = "single"):
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
        phase,
        "--automated-run-budget-ms",
        str(budget_ms),
    ]
    if empty_timeline or course in ("failed", "refused", "capped"):
        command.append("--public-preview-empty-timeline-course")
    else:
        command.append("--public-preview-historical-course")
    if course:
        command.extend(["--public-preview-history-course", course])
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
            or (not empty_timeline and not course in ("failed", "refused", "capped")
                and "PUBLIC_PREVIEW_HISTORICAL_HANDLE_OK" not in completed.stdout)):
        raise AssertionError(
            f"composed public read failed ({completed.returncode})\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )

    expected_pages = 10 if course == "capped" else 1
    pages = [line for line in completed.stdout.splitlines()
             if line.startswith("PUBLIC_PREVIEW_TIMELINE_PAGE:")]
    if pages != [f"PUBLIC_PREVIEW_TIMELINE_PAGE:{page}" for page in range(1, expected_pages + 1)]:
        raise AssertionError(f"timeline course {course!r} did not consume exact page budget: {pages}")
    line_history = [line.removeprefix("PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:")
                    for line in completed.stdout.splitlines()
                    if line.startswith("PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:")]
    if (len(line_history) != 1 or not line_history[0].startswith("Incomplete history")
            or "display order is not push order" not in line_history[0]):
        raise AssertionError(f"epoch line hid the partial-history qualification in {course!r}: {line_history}")
    if course == "missing-base" and not all(marker in completed.stdout for marker in (
            "PUBLIC_PREVIEW_MISSING_BASE_OK", "PUBLIC_PREVIEW_DISCONNECTED_HEAD_FETCHED")):
        raise AssertionError(f"fetchable history without a common base was not unavailable: {completed.stdout}")
    if course == "rewritten-base" and "PUBLIC_PREVIEW_DISCONNECTED_HEAD_FETCHED" not in completed.stdout:
        raise AssertionError("rewritten base did not fetch the disconnected historical head")
    print(f"EPOCH_COMPOSED_COURSE:{course or ('empty' if empty_timeline else 'historical')}:{phase}:pages={len(pages)}")

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
        previous = connection.execute(
            "SELECT previous_distinct_head_ordinal FROM pull_request_scope"
        ).fetchone()[0]
        live_base = connection.execute(
            "SELECT hex(object_digest) FROM epoch_anchor WHERE ordinal = ? AND endpoint = 1",
            (live_ordinal,),
        ).fetchone()[0]
    return occurrences, history, incarnations, epoch_count, live_ordinal, imports, previous, live_base


def verify_rewritten_base(application: Path, root: Path, budget_ms: int):
    original = read_catalogue(application, root, budget_ms, empty_timeline=True)
    rewritten = read_catalogue(application, root, budget_ms,
                               course="rewritten-base", phase="reuse")
    historical = [row for row in rewritten[0] if row[1] == 0]
    if (rewritten[4] != 2 or rewritten[7] == original[7]
            or len(historical) != 3 or historical[0][6:9] != (None, None, None)
            or any(row[7] is not None for row in historical)):
        raise AssertionError(f"rewritten selected base became fabricated historical truth: {original!r} -> {rewritten!r}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True, type=Path)
    parser.add_argument("--budget-ms", required=True, type=int)
    parser.add_argument("--rewritten-only", action="store_true")
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="bpr-portable-epoch-") as temporary:
        if arguments.rewritten_only:
            verify_rewritten_base(arguments.application, Path(temporary) / "rewritten-base",
                                  arguments.budget_ms)
            print("EPOCH_REWRITTEN_BASE_OK")
            return
        first_root = Path(temporary) / "first"
        seed = read_catalogue(arguments.application, first_root, arguments.budget_ms,
                              empty_timeline=True)
        first = read_catalogue(arguments.application, first_root, arguments.budget_ms)
        second = read_catalogue(arguments.application, Path(temporary) / "second", arguments.budget_ms)
        for course in ("failed", "refused", "capped"):
            partial = read_catalogue(arguments.application, Path(temporary) / course,
                                     arguments.budget_ms, course=course)
            if partial[3:7] != (1, 1, 0, None) or partial[1][0][0] != 1:
                raise AssertionError(f"{course} history did not retain today's live epoch and partial state")
        missing_base = read_catalogue(arguments.application, Path(temporary) / "missing-base",
                                      arguments.budget_ms, course="missing-base")
        if missing_base[0][0][6:9] != (None, None, None) or missing_base[4] != 1:
            raise AssertionError("missing common base fabricated a historical anchor or changed live authority")
        verify_rewritten_base(arguments.application, Path(temporary) / "rewritten-base",
                              arguments.budget_ms)
        retained_root = Path(temporary) / "retained"
        observed = read_catalogue(arguments.application, retained_root, arguments.budget_ms,
                                  empty_timeline=True)
        overwritten = read_catalogue(arguments.application, retained_root, arguments.budget_ms,
                                     phase="reuse")
        if overwritten[4] <= observed[4] or overwritten[6] != observed[4]:
            raise AssertionError("force-push import lost the earlier observed update or changed previous-live authority")
        gap = read_catalogue(arguments.application, Path(temporary) / "gap", arguments.budget_ms,
                             unfetchable_first=True)

    if seed[3:7] != (1, 1, 0, None):
        raise AssertionError("empty-timeline seed did not leave one unchanged live epoch")

    left, left_history, left_incarnations, left_epoch_count, left_live, left_imports, left_previous, left_base = first
    right, right_history, right_incarnations, right_epoch_count, right_live, right_imports, right_previous, right_base = second
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
    if left_previous is not None or right_previous is not None:
        raise AssertionError("historical imports advanced previous-distinct-head authority")
    if left_base != seed[7] or left_base != right_base:
        raise AssertionError("historical imports changed the live observed base")
    gap_events = gap[0]
    if (gap_events[0][6] is not None or gap_events[1][6] is None or gap[4] != 1):
        raise AssertionError("unfetchable first head starved the later retained update")
    print("EPOCH_PORTABLE_TWO_CATALOGUES_OK:3")


if __name__ == "__main__":
    main()
