#!/usr/bin/env python3
"""[[BPR-1340]]. Drag the Updates line's handles and read its dates through the composed app.

The owner's report: the newest mark on the Updates line said "Date unavailable",
and neither handle could be dragged to compare other versions.

The drag course opens the recorded pull request with three force-push updates,
opens the Updates line from the review bar's disclosure, and starts from the
whole-PR pair, whose two handles share the newest node. Through the window, as
a pointer is delivered, it drags the left handle to the oldest update and the
right handle back one update, then steps the right handle with Shift+Right; each
time it waits for the comparison the core publishes back to put the handle
there.

The date course observes one ordinary update, then clears its recorded date the
way an earlier session that could not read the commit left it, and opens the
review again at the same head in a new process. The newest update must carry
its commit date again; before the fix it stayed "Date unavailable" until the
author pushed.
"""

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


def run(application, root, budget, phase, log, course):
    make_root_private(root)
    command = [str(application), "--self-test", "--github-fixture", "github-com",
               "--public-preview-probe", "--public-preview-enabled-course",
               *course,
               "--epoch-fixture-root", str(root), "--epoch-fixture-phase", phase,
               "--automated-run-budget-ms", str(budget)]
    result = subprocess.run(command, text=True, capture_output=True, check=False,
                            timeout=budget / 1000 + 13,
                            env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
    log.write_text(result.stdout + result.stderr)
    require(result.returncode == 0, f"composed read failed: {log}")
    require("PUBLIC_PREVIEW_ENABLED_OK" in result.stdout, f"anonymous read not accepted: {log}")
    return result.stdout


def labels(stdout, log):
    found = [line.removeprefix("PUBLIC_PREVIEW_EPOCH_LINE_LABELS:")
             for line in stdout.splitlines() if line.startswith("PUBLIC_PREVIEW_EPOCH_LINE_LABELS:")]
    require(len(found) == 1, f"the run named no line labels: {log}")
    return found[0].split(" | ")


def drag_course(application, root, budget, logs):
    log = logs / "drag.log"
    stdout = run(application, root, budget, "single", log, ["--public-preview-handle-drag-course"])
    print(next(line for line in stdout.splitlines() if line.startswith("PUBLIC_PREVIEW_HANDLE_DRAG_STEPS:")))
    require("PUBLIC_PREVIEW_HANDLE_DRAG_MOVED:left=1:right=1:key=1\n" in stdout,
            f"a handle did not move the comparison: {log}")
    require("PUBLIC_PREVIEW_HANDLE_DRAG_OK\n" in stdout, f"handle drag course failed: {log}")
    shown = labels(stdout, log)
    require(len(shown) >= 3 and "Date unavailable" not in shown, f"the line's labels: {shown}")
    print("EPOCH_UPDATE_LINE_DRAG_OK:1")


def date_course(application, root, budget, logs):
    ordinary = ["--public-preview-empty-timeline-course", "--public-preview-history-course", "ordinary"]
    run(application, root, budget, "single", logs / "date-initial.log", ordinary)
    run(application, root, budget, "reuse", logs / "date-updated.log", ordinary)
    database = root / "state/v1/epoch-catalog.sqlite3"
    with closing(sqlite3.connect(str(database))) as db:
        rows = db.execute("SELECT kind, hex(head_digest), date_source FROM epoch_occurrence").fetchall()
        require(rows == [(1, UPDATED, 2)], f"the ordinary update was not dated from its commit: {rows}")
        # What a session that could not read the commit left behind.
        db.execute("UPDATE epoch_occurrence SET date_source = 0, date_utc = 0")
        db.commit()
    log = logs / "date-reopened.log"
    stdout = run(application, root, budget, "reuse", log, ordinary)
    shown = labels(stdout, log)
    require(len(shown) == 1 and shown[0].startswith("Commit date "),
            f"reopening at the same head left the update undated: {shown}")
    with closing(sqlite3.connect(database.as_uri() + "?mode=ro", uri=True)) as db:
        rows = db.execute("SELECT kind, hex(head_digest), date_source FROM epoch_occurrence").fetchall()
    require(rows == [(1, UPDATED, 2)], f"the reopened review did not record the commit date: {rows}")
    print("EPOCH_UPDATE_LINE_DATE_OK:1")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True, type=Path)
    parser.add_argument("--budget-ms", required=True, type=int)
    parser.add_argument("--logs", required=True, type=Path)
    args = parser.parse_args()
    args.logs.mkdir(parents=True, exist_ok=True)
    with contained_temporary_directory(prefix="bpr-update-line-") as directory:
        # The application trusts the root's parent only when it is private too.
        make_root_private(Path(directory))
        drag = Path(directory) / "drag"
        drag_course(args.application, drag, args.budget_ms, args.logs)
        shutil.rmtree(drag, **rmtree_callback_arguments(clear_read_only_and_retry))
        date_course(args.application, Path(directory) / "date", args.budget_ms, args.logs)
    print("EPOCH_UPDATE_LINE_OK:1")


if __name__ == "__main__":
    main()
