#!/usr/bin/env python3
"""Observe one three-commit update through the composed anonymous public path."""

import argparse
from contextlib import closing
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import tempfile


UPDATED = "B7B8DC0E5C4FBAC8079B2C44BACFFF5D36519C13"


def require(condition, diagnostic):
    if not condition:
        raise AssertionError(diagnostic)


def observe(application, root, budget, phase, log):
    root.mkdir(mode=0o700, exist_ok=True)
    command = [str(application), "--self-test", "--github-fixture", "github-com",
               "--public-preview-probe", "--public-preview-enabled-course",
               "--public-preview-empty-timeline-course",
               "--public-preview-history-course", "ordinary",
               "--epoch-fixture-root", str(root), "--epoch-fixture-phase", phase,
               "--automated-run-budget-ms", str(budget)]
    result = subprocess.run(command, text=True, capture_output=True, check=False,
                            timeout=budget / 1000 + 13,
                            env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
    log.write_text(result.stdout + result.stderr)
    require(result.returncode == 0, f"composed read failed: {log}")
    require("PUBLIC_PREVIEW_ENABLED_OK" in result.stdout, f"anonymous read not accepted: {log}")
    count = 4 if phase == "reuse" else 1
    require(result.stdout.count(f"ORDINARY_SURVIVING_COMMITS:{count}\n") == 1, log)
    require(result.stdout.count("PUBLIC_PREVIEW_TIMELINE_PAGE:1\n") == 1, log)
    require("PUBLIC_PREVIEW_TIMELINE_PAGE:2" not in result.stdout, log)
    database = root / "state/v1/epoch-catalog.sqlite3"
    with closing(sqlite3.connect(database.as_uri() + "?mode=ro", uri=True)) as db:
        occurrences = db.execute(
            "SELECT hex(occurrence_id), kind, hex(head_digest), local_ordinal "
            "FROM epoch_occurrence ORDER BY occurrence_id").fetchall()
        history = db.execute("SELECT incomplete, ambiguous FROM epoch_occurrence_history").fetchall()
        epochs = db.execute("SELECT count(*) FROM epoch").fetchone()[0]
        incarnation = db.execute("SELECT hex(incarnation) FROM epoch LIMIT 1").fetchone()[0]
        scope = db.execute("SELECT latest_live_ordinal, previous_distinct_head_ordinal FROM pull_request_scope").fetchall()
        if phase == "reuse":
            require(db.execute("SELECT primary_cause FROM epoch WHERE ordinal = 2").fetchall() == [(5,)], "descendant update must be author_upload")
    return occurrences, history, epochs, incarnation, scope


def course(application, root, budget, logs, label):
    initial = observe(application, root, budget, "single", logs / f"{label}-initial.log")
    require(initial[0] == [] and initial[2] == 1, initial)
    updated = observe(application, root, budget, "reuse", logs / f"{label}-updated.log")
    require(len(updated[0]) == 1, updated)
    require(updated[0][0][1:] == (1, UPDATED, 2), updated)
    require(updated[1] == [(1, 0)] and updated[2] == 2 and updated[4] == [(2, 1)], updated)
    returned = observe(application, root, budget, "single", logs / f"{label}-return.log")
    require(returned[0] == updated[0] and returned[1] == [(1, 1)], returned)
    require(returned[2] == 3 and returned[4] == [(3, 2)], returned)
    return updated


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True, type=Path)
    parser.add_argument("--budget-ms", required=True, type=int)
    parser.add_argument("--logs", required=True, type=Path)
    args = parser.parse_args()
    args.logs.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="bpr-ordinary-update-") as directory:
        root = Path(directory) / "catalogue"
        first = course(args.application, root, args.budget_ms, args.logs, "first")
        shutil.rmtree(root)
        require(not root.exists(), "catalogue recreation must physically remove the old root")
        recreated = course(args.application, root, args.budget_ms, args.logs, "recreated")
        require(first[0] == recreated[0], "portable occurrence changed after physical recreation")
        require(first[3] != recreated[3], "catalogue incarnation was reused")
        print(f"EPOCH_ORDINARY_ID:{first[0][0][0]}")
        print(f"EPOCH_ORDINARY_INCARNATIONS:{first[3]}:{recreated[3]}")
    print("EPOCH_ORDINARY_RECREATED_OK:1")


if __name__ == "__main__":
    main()
