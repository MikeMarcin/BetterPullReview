#!/usr/bin/env python3
"""Checks the fixture marker reports the fixture and not the run.

`GITHUB_FIXTURE_FAILED` used to be printed from a verdict already narrowed by
presentation, shutdown, epoch delivery and render facts, so a run whose fixture
worked perfectly and whose presentation failed blamed the fixture. That cost
BPR-0154 three recorded occurrences and two wrong hypotheses before anyone
decoded the evidence bits. BPR-0213.

The two verdicts have to be able to disagree for this to be testable at all. A
render capture aimed at an unwritable path gives exactly that: the fixture does
its work and passes, and `passed = passed && renderSaved` fails the run.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

BINARY = Path(sys.argv[1]).resolve()
# The command-line name and the name the marker prints are not the same string:
# the flag takes github-com and the marker reports the fixture's own github.com.
FIXTURE_ARGUMENT = "github-com"
FIXTURE_NAME = "github.com"
BUDGET_MS = 40000

FAILURES: list[str] = []


def expect(condition: bool, message: str) -> None:
    if not condition:
        FAILURES.append(message)


def run(*arguments: str) -> str:
    completed = subprocess.run(
        [str(BINARY), "--github-fixture", FIXTURE_ARGUMENT,
         "--automated-run-budget-ms", str(BUDGET_MS), *arguments],
        capture_output=True,
        text=True,
        env={**os.environ, "QT_QPA_PLATFORM": "offscreen"},
        timeout=300,
        check=False,
    )
    return completed.stdout + completed.stderr


def test_a_failed_run_with_a_good_fixture_does_not_blame_it() -> None:
    # A directory that cannot be written to, so saving the render fails while
    # everything the fixture does succeeds.
    unwritable = "/dev/null/bpr-render-cannot-exist.png"
    output = run("--render-evidence", unwritable)

    expect(
        "PUBLIC_PATH_FAILED" in output,
        "the run fails, which is what makes the two verdicts able to disagree",
    )
    expect(
        "RENDER_FAILED" in output,
        f"the render is what failed: {output.strip()[-200:]}",
    )
    expect(
        f"GITHUB_FIXTURE_OK:{FIXTURE_NAME}" in output,
        "the fixture marker reports the fixture, which worked, rather than the "
        "run, which did not",
    )
    expect(
        "GITHUB_FIXTURE_FAILED" not in output,
        "nothing blames the fixture for a failure that was not the fixture's",
    )

    # And the run-level verdict is still reachable, so nothing was lost by
    # making the fixture marker narrower.
    expect(
        "PUBLIC_PATH_GROUPS:" in output,
        "the groups line still names which narrowing group rejected the run",
    )


def test_a_passing_run_reports_both() -> None:
    """The ordinary case, so the marker keeps its teeth.

    Every CTest gate matching `GITHUB_FIXTURE_OK` pairs it with
    `PUBLIC_PATH_OK`, so a marker that had become unconditionally true would
    still be caught there — but only if it is still printed at all.
    """
    output = run()
    expect(
        f"GITHUB_FIXTURE_OK:{FIXTURE_NAME}" in output and "PUBLIC_PATH_OK" in output,
        f"a clean run reports both verdicts: {output.strip()[-200:]}",
    )


def main() -> int:
    test_a_failed_run_with_a_good_fixture_does_not_blame_it()
    test_a_passing_run_reports_both()
    for failure in FAILURES:
        print(f"FAILED: {failure}", file=sys.stderr)
    if FAILURES:
        return 1
    print("FIXTURE_VERDICT_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
