#!/usr/bin/env python3
"""Checks the frozen attended build's launcher, without building anything.

BPR-1331 and BPR-1339's v001 launchers checked a fixture path no commit ever
had, and the owner saw `FIXTURES_MISSING` instead of the build. The launcher is
now written from one template and its `--check` runs at freeze time. These
cases render that template into a scratch root around a stand-in executable
that prints what the application prints, and run it.
"""

from __future__ import annotations

import os
import stat
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts import freeze_attended_build as freeze  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

FAILURES: list[str] = []

# Prints the head the phase serves, then the shown line, and stays open like
# the application until the launcher closes it.
STAND_IN = """#!/bin/sh
head=1111111111111111111111111111111111111111
for argument in "$@"; do
  if [ "$argument" = reuse ]; then head=2222222222222222222222222222222222222222; fi
done
echo "ATTENDED_HEAD:$head"
echo "ATTENDED_FILE_SHOWN:src/engine/renderer.cpp"
exec sleep 30
"""


def expect(condition: bool, message: str) -> None:
    if not condition:
        FAILURES.append(message)


def frozen_root(directory: Path, name: str, *, required: list[str], next_push: bool, stand_in: str = STAND_IN) -> Path:
    root = directory / name
    executable = root / "src" / freeze.EXECUTABLE
    executable.parent.mkdir(parents=True)
    executable.write_text(stand_in)
    executable.chmod(executable.stat().st_mode | stat.S_IXUSR)
    (root / "src/tests/fixtures/github/contracts/v1/conventional-comments-v1").mkdir(parents=True)
    (root / "launch.py").write_text(freeze.render_launcher(
        root=root, title="test", commit="0" * 40, fixture="conventional-comments-v1",
        open_file="src/engine/renderer.cpp", required=required, next_push=next_push))
    return root


def launch(root: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    environment = {key: value for key, value in os.environ.items() if key != "QT_QPA_PLATFORM"}
    return subprocess.run([sys.executable, str(root / "launch.py"), *arguments], capture_output=True, text=True,
                          check=False, timeout=120, env=environment)


def main() -> int:
    repository = freeze.REPOSITORY
    expect(freeze.fixture_paths(repository, "conventional-comments-v1") == ["tests/fixtures/github/contracts/v1/conventional-comments-v1"],
           "conventional-comments-v1's fixture tree is its contract directory")
    expect(not (repository / "tests/fixtures/github/pull-requests/conventional-comments-v1").exists(),
           "the path the v001 launchers checked does not exist, so a check of it can only fail")
    expect(freeze.fixture_paths(repository, "no-such-fixture-v1") == [], "an unknown fixture has no tree")

    with contained_temporary_directory() as scratch:
        directory = Path(scratch)
        wrong = frozen_root(directory, "wrong", required=["tests/fixtures/github/pull-requests/conventional-comments-v1"], next_push=False)
        checked = launch(wrong, "--check")
        expect(checked.returncode == 1 and "FIXTURES_MISSING:" in checked.stdout,
               f"a launcher checking a missing fixture path fails its own check: {checked.stdout!r}")

        plain = frozen_root(directory, "plain", required=["tests/fixtures/github/contracts/v1/conventional-comments-v1"], next_push=False)
        checked = launch(plain, "--check")
        expect(checked.returncode == 0 and "CHECK_OK (first head): ATTENDED_FILE_SHOWN:src/engine/renderer.cpp" in checked.stdout,
               f"a launcher whose fixture is present opens the file: {checked.stdout!r}")
        expect(launch(plain, "--next-push").returncode == 2, "a launcher frozen without a push refuses --next-push")

        pushed = frozen_root(directory, "pushed", required=["tests/fixtures/github/contracts/v1/conventional-comments-v1"], next_push=True)
        checked = launch(pushed, "--check")
        expect(checked.returncode == 0 and "CHECK_OK (next push)" in checked.stdout and "head 2222" in checked.stdout,
               f"the check opens the pushed head too: {checked.stdout!r}")

        same_head = frozen_root(directory, "same", required=[], next_push=True, stand_in=STAND_IN.replace("head=" + "2" * 40, "head=" + "1" * 40))
        checked = launch(same_head, "--check")
        expect(checked.returncode == 1 and "CHECK_FAILED: the next push showed head" in checked.stdout,
               f"a push that serves the first head fails the check: {checked.stdout!r}")

        exits = frozen_root(directory, "exits", required=[], next_push=False, stand_in="#!/bin/sh\necho STARTUP_FAILED\nexit 3\n")
        checked = launch(exits, "--check")
        expect(checked.returncode == 1 and "EXITED 3 within seconds" in checked.stdout and "STARTUP_FAILED" in checked.stdout,
               f"an application that exits is reported within seconds: {checked.stdout!r}")

    if FAILURES:
        for failure in FAILURES:
            print(f"FAILED: {failure}")
        return 1
    print("FREEZE_ATTENDED_BUILD_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
