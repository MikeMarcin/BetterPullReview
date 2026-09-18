#!/usr/bin/env python3
"""Observe one three-commit update through the composed anonymous public path."""

import argparse
from contextlib import closing
import os
from pathlib import Path
import shutil
import sqlite3
import stat
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
# [[BPR-0900]]. The descriptor this course needs, promoted beside it so the
# two courses that had it byte-identical now share one spelling.
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from scripts.contained_temporary_support import (  # noqa: E402
    contained_temporary_directory,
    rmtree_callback_arguments,
)


UPDATED = "B7B8DC0E5C4FBAC8079B2C44BACFFF5D36519C13"


def clear_read_only_and_retry(function, path, _error):
    os.chmod(path, stat.S_IWRITE)
    function(path)


def require(condition, diagnostic):
    if not condition:
        raise AssertionError(diagnostic)


def observe(application, root, budget, phase, log):
    make_root_private(root)
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
    with contained_temporary_directory(prefix="bpr-ordinary-update-") as directory:
        # The application trusts the root's parent only when it is private too.
        make_root_private(Path(directory))
        root = Path(directory) / "catalogue"
        first = course(args.application, root, args.budget_ms, args.logs, "first")
        # [[BPR-0834]]. Git writes its pack files read-only, which Windows
        # refuses to unlink until the attribute is cleared.
        # [[BPR-0867]]. `onexc` is 3.12 and the tree's floor is 3.10, so the
        # keyword comes from the helper that already adapts between the two.
        shutil.rmtree(root, **rmtree_callback_arguments(clear_read_only_and_retry))
        require(not root.exists(), "catalogue recreation must physically remove the old root")
        recreated = course(args.application, root, args.budget_ms, args.logs, "recreated")
        require(first[0] == recreated[0], "portable occurrence changed after physical recreation")
        require(first[3] != recreated[3], "catalogue incarnation was reused")
        print(f"EPOCH_ORDINARY_ID:{first[0][0][0]}")
        print(f"EPOCH_ORDINARY_INCARNATIONS:{first[3]}:{recreated[3]}")
    print("EPOCH_ORDINARY_RECREATED_OK:1")


if __name__ == "__main__":
    main()
