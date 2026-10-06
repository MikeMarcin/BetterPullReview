#!/usr/bin/env python3
"""Checks the attended launch holds a window open and refuses to share the wheel.

Every owner-attended UI card on this board waited on a window that never stayed
open: a fixture launch scheduled an automated run and exited, and a launch
without a fixture took the real credentialed path. BPR-0186.

The two things worth asserting are that it outlives the budget an automated run
would have died at, and that it cannot be combined with anything that drives
itself — an attended launch and an automated run are contradictory instructions
about who is driving, and one quietly winning is worse than a refusal.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# [[BPR-1050]]. The data root's descriptor, and the contained temporary root.
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

BINARY = Path(sys.argv[1]).resolve()

# Comfortably past the budget the fixture courses run under, so a scheduled run
# would have reached shutdown and exited well before this elapses.
BUDGET_MS = 8000
OUTLIVE_SECONDS = 20.0
# A refusal is an argument-parsing decision, so it is immediate. Anything that
# takes longer has not refused.
REFUSAL_SECONDS = 25.0

FAILURES: list[str] = []


def expect(condition: bool, message: str) -> None:
    if not condition:
        FAILURES.append(message)


def environment() -> dict[str, str]:
    env = dict(os.environ)
    env["QT_QPA_PLATFORM"] = "offscreen"
    return env


def refused(*arguments: str) -> tuple[bool, str]:
    """Runs one illegal combination and reports whether it was refused.

    A missing refusal does not merely return the wrong answer — an attended
    launch without a fixture takes the real credentialed path and waits for a
    person, so the process never exits. Timing out is therefore a *result* here
    and not an accident, and is reported as the failure it is rather than
    raised as a traceback. An earlier draft let it propagate, which turned a
    clean assertion failure into a stack trace after two minutes.
    """
    try:
        completed = subprocess.run(
            [str(BINARY), *arguments],
            capture_output=True,
            text=True,
            env=environment(),
            timeout=REFUSAL_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired as expired:
        captured = expired.stdout or b""
        text = captured.decode(errors="replace") if isinstance(captured, bytes) else captured
        return False, f"still running after {REFUSAL_SECONDS:.0f}s; {text.strip()[:120]}"
    output = completed.stdout + completed.stderr
    return (
        completed.returncode != 0
        and "ATTENDED_LAUNCH_CONFIGURATION_FAILED" in output,
        output,
    )


def test_it_refuses_to_share_the_wheel() -> None:
    # r5 critical review. These paths are reached only if a refusal fails to
    # come first, so they must not be literal /tmp names: two build trees
    # running the suite at once would share them, the runner already points
    # TMPDIR at a per-test fixture root, and on Windows /tmp is not a
    # directory at all. The capture assertion below uses the same path.
    unreached = Path(tempfile.gettempdir()) / "bpr-attended-none"
    capture   = unreached.with_suffix(".png")
    combinations = {
        "no fixture at all": ["--attended-launch"],
        "the diff workbench probe": [
            "--github-fixture", "github-com", "--attended-launch",
            "--diff-workbench-probe", "--diff-interaction-evidence", str(unreached),
        ],
        "the shutdown probe": [
            "--github-fixture", "github-com", "--attended-launch",
            "--diff-workbench-shutdown-probe",
        ],
        "the appearance gesture probe": [
            "--github-fixture", "github-com", "--attended-launch",
            "--appearance-gesture-probe",
        ],
        "the stall probe": [
            "--github-fixture", "github-com", "--attended-launch",
            "--automated-stall-probe",
        ],
        "a render capture": [
            "--github-fixture", "github-com", "--attended-launch",
            "--render-evidence", str(capture),
        ],
        # [[BPR-1085]]. The preflight look needs its file and its fixture,
        # and names one of three looks.
        "a preflight look with no file": [
            "--github-fixture", "conventional-comments-v1", "--attended-launch",
            "--attended-preflight-look", "native",
        ],
        "a preflight look on another fixture": [
            "--github-fixture", "github-com", "--attended-launch",
            "--attended-open-file", "src/engine/renderer.cpp",
            "--attended-preflight-look", "review",
        ],
        "an unknown preflight look": [
            "--github-fixture", "conventional-comments-v1", "--attended-launch",
            "--attended-open-file", "src/engine/renderer.cpp",
            "--attended-preflight-look", "everything",
        ],
    }
    for name, arguments in combinations.items():
        ok, output = refused(*arguments)
        expect(ok, f"an attended launch with {name} is refused: {output.strip()[:160]}")

    # And no window is shown on refusal. The render path is the observable
    # proxy: a refused launch that had composed a window would have had one to
    # capture.
    expect(
        not capture.exists(),
        "a refused attended launch composes nothing, so no capture appears",
    )


def test_it_outlives_the_automated_budget() -> None:
    # [[BPR-1051]]. This course ends the application with a signal, so nothing
    # the application removes at its own exit is removed. Left to itself, the
    # fixture made a `bpr-public-path-*` data root in the temporary directory,
    # and every run left one. The course names a root it owns instead, one
    # owned level down ([[BPR-1050]]), and removes it after the process ends.
    with contained_temporary_directory(prefix="bpr-attended-launch-") as temporary:
        parent = Path(temporary).resolve()
        make_root_private(parent)
        root = parent / "data"
        make_root_private(root)
        process = subprocess.Popen(
            [
                str(BINARY),
                "--github-fixture",
                "github-com",
                "--attended-launch",
                "--epoch-fixture-root",
                str(root),
                "--epoch-fixture-phase",
                "single",
                "--automated-run-budget-ms",
                str(BUDGET_MS),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env=environment(),
        )
        try:
            deadline = time.monotonic() + OUTLIVE_SECONDS
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    break
                time.sleep(0.25)
            expect(
                process.poll() is None,
                "the attended launch is still running after the automated budget "
                "would have driven a scheduled run to shutdown",
            )
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=30)


def main() -> int:
    test_it_refuses_to_share_the_wheel()
    test_it_outlives_the_automated_budget()
    for failure in FAILURES:
        print(f"FAILED: {failure}", file=sys.stderr)
    if FAILURES:
        return 1
    print("ATTENDED_LAUNCH_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
