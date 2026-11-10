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


# Answers only when it was handed the helper in its own build tree.
WORKSHOP_STAND_IN = """#!/bin/sh
for argument in "$@"; do
  case "$argument" in
    */src/build/app-release-tests/helper) echo "ATTENDED_FILE_SHOWN:x"; echo "ATTENDED_WORKSHOP_SHOWN"; exec sleep 30 ;;
  esac
done
echo "ATTENDED_WORKSHOP_FAILED"
exec sleep 30
"""


# Opens the pull request only when asked for it and for no file, as the
# application refuses the two together.
OVERVIEW_STAND_IN = """#!/bin/sh
opened=no
for argument in "$@"; do
  case "$argument" in
    --attended-open-file) echo "ATTENDED_LAUNCH_CONFIGURATION_FAILED"; exit 1 ;;
    --attended-open-pull-request) opened=yes ;;
  esac
done
if [ "$opened" = yes ]; then echo "ATTENDED_PULL_REQUEST_OPENED"; else echo "ATTENDED_FILE_FAILED:none"; fi
exec sleep 30
"""


def frozen_root(directory: Path, name: str, *, required: list[str], next_push: bool, stand_in: str = STAND_IN,
                extra: list[str] | None = None, ready: str = "ATTENDED_FILE_SHOWN",
                open_file: str | None = "src/engine/renderer.cpp") -> Path:
    root = directory / name
    executable = root / "src" / freeze.EXECUTABLE
    executable.parent.mkdir(parents=True)
    executable.write_text(stand_in)
    executable.chmod(executable.stat().st_mode | stat.S_IXUSR)
    (root / "src/tests/fixtures/github/contracts/v1/conventional-comments-v1").mkdir(parents=True)
    (root / "launch.py").write_text(freeze.render_launcher(
        root=root, title="test", commit="0" * 40, fixture="conventional-comments-v1",
        open_file=open_file, required=required, next_push=next_push, extra=extra, ready=ready))
    return root


# A tests-off application: its self-test says the CLI entry was offered;
# otherwise it makes its data where the macOS standard paths put it (under
# CFFIXED_USER_HOME when set) and records the HOME it was given. Like the
# application, it keeps an update control folder beside its bundle where it
# may write one, and refuses to start on one it can no longer write
# (bpr-live-v001). Each start's home is appended to BPR_STAND_IN_HOMES.
LIVE_STAND_IN = """#!/bin/sh
bundle_folder="$(cd "$(dirname "$0")/../../.." && pwd)"
control="$bundle_folder/.bpr-update-0"
if [ -d "$control" ] && [ ! -w "$control" ]; then echo STARTUP_FAILED; exit 1; fi
if [ -w "$bundle_folder" ]; then mkdir -p "$control"; fi
for argument in "$@"; do
  if [ "$argument" = --self-test ]; then echo CLI_ENTRY_OFFERED_OK; exit 0; fi
done
home="${CFFIXED_USER_HOME:-$HOME}"
if [ -n "$BPR_STAND_IN_HOMES" ]; then echo "$home" >> "$BPR_STAND_IN_HOMES"; fi
mkdir -p "$home/Library/Application Support/BetterPullReview"
echo "$HOME" > "$home/home-was"
exec sleep 30
"""


def live_root(directory: Path, name: str, stand_in: str = LIVE_STAND_IN, sealed: bool = True) -> Path:
    root = directory / name
    executable = root / "src" / freeze.LIVE_EXECUTABLE
    executable.parent.mkdir(parents=True)
    executable.write_text(stand_in)
    executable.chmod(executable.stat().st_mode | stat.S_IXUSR)
    (root / "launch.py").write_text(freeze.render_live_launcher(root=root, title="test", commit="0" * 40, wait=15))
    if sealed:
        freeze.set_writable(root / "src", False)
    return root


def launch(root: Path, *arguments: str, homes: Path | None = None) -> subprocess.CompletedProcess[str]:
    environment = {key: value for key, value in os.environ.items() if key not in ("QT_QPA_PLATFORM", "BPR_STAND_IN_HOMES")}
    if homes is not None:
        environment["BPR_STAND_IN_HOMES"] = str(homes)
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

        workshop = frozen_root(directory, "workshop", required=[], next_push=False, stand_in=WORKSHOP_STAND_IN,
                               extra=["--attended-workshop-agent", "{build}/helper"], ready="ATTENDED_WORKSHOP_SHOWN")
        checked = launch(workshop, "--check")
        expect(checked.returncode == 0 and "CHECK_OK (first head): ATTENDED_WORKSHOP_SHOWN" in checked.stdout,
               f"further arguments name the root's own build tree and the look waits for its own line: {checked.stdout!r}")

        overview = frozen_root(directory, "overview", required=[], next_push=False, stand_in=OVERVIEW_STAND_IN,
                               ready=freeze.OPENED, open_file=None)
        checked = launch(overview, "--check")
        expect(checked.returncode == 0 and "CHECK_OK (first head): ATTENDED_PULL_REQUEST_OPENED" in checked.stdout,
               f"a look on the pull request's Overview opens it with no file: {checked.stdout!r}")
        expect("on its Overview" in (overview / "launch.py").read_text(), "the Overview launcher says where it opens")

    # The live freeze: tests off, its own home, the launcher's own check.
    refused = subprocess.run([sys.executable, str(repository / "scripts/freeze_attended_build.py"), "freeze", "--live", "--name", "x",
                              "--fixture", "conventional-comments-v1"], capture_output=True, text=True, check=False, timeout=60)
    expect(refused.returncode == 1 and "FREEZE_FAILED:--live freezes no fixture" in refused.stdout,
           f"a live freeze refuses a fixture before it exports anything: {refused.stdout!r}")
    unopened = subprocess.run([sys.executable, str(repository / "scripts/freeze_attended_build.py"), "freeze", "--name", "x",
                               "--fixture", "conventional-comments-v1"], capture_output=True, text=True, check=False, timeout=60)
    expect(unopened.returncode == 1 and "FREEZE_FAILED:a fixture look needs" in unopened.stdout,
           f"a fixture look still needs what it opens: {unopened.stdout!r}")
    expect(freeze.LIVE_BUILD == "build/app-release" and "app-release-tests" not in freeze.LIVE_EXECUTABLE,
           "a live freeze builds the tests-off app-release tree")
    with contained_temporary_directory() as scratch:
        directory = Path(scratch)
        live = live_root(directory, "live")
        homes = directory / "homes.log"
        checked = launch(live, "--check", homes=homes)
        expect(checked.returncode == 0 and "CHECK_OK (self-test): exit 0 with CLI_ENTRY_OFFERED_OK" in checked.stdout
               and f"CHECK_OK (launch): up with its data under {live.resolve() / 'home'}, emptied again" in checked.stdout,
               f"a tests-off build passes its self-test and comes up on the owner's home: {checked.stdout!r}")
        started = homes.read_text().split() if homes.is_file() else []
        expect(started == [str(live.resolve() / "home")], f"the check's launch is the owner's: the root's own home: {started!r}")
        expect(not (live / "home").exists() and not list(live.glob("check.*")), "the check empties the owner's home and leaves no scratch")
        again = launch(live, "--check")
        expect(again.returncode == 0, f"a checked build checks again, as the owner's launch then starts: {again.stdout!r}")

        # bpr-live-v001: checked before the seal, its launch left the update
        # control folder in the build, and the seal made every start refuse.
        unsealed = live_root(directory, "unsealed", sealed=False)
        checked = launch(unsealed, "--check")
        expect(checked.returncode == 1 and "BUILD_NOT_SEALED:" in checked.stdout and "is writable" in checked.stdout,
               f"an unsealed build is refused: {checked.stdout!r}")
        leftover = live_root(directory, "leftover", sealed=False)
        (leftover / "src" / freeze.LIVE_EXECUTABLE).parents[3].joinpath(".bpr-update-0").mkdir()
        freeze.set_writable(leftover / "src", False)
        checked = launch(leftover, "--check")
        expect(checked.returncode == 1 and "BUILD_NOT_SEALED:" in checked.stdout and ".bpr-update-0" in checked.stdout,
               f"a build sealed over a launch's update folder is refused: {checked.stdout!r}")
        source = (repository / "scripts/freeze_attended_build.py").read_text()
        live_freeze = source[source.index("def freeze_live("):source.index("def remove(")]
        expect(live_freeze.index("set_writable(source, False)") < live_freeze.index('"--check"'),
               "a live freeze seals the build before its check, so the check starts what the owner will")
        expect("CFFIXED_USER_HOME" in (live / "launch.py").read_text() and "--github-fixture" not in (live / "launch.py").read_text(),
               "the live launcher gives the application its own home and passes no fixture")

        tests_on = live_root(directory, "tests-on", stand_in=LIVE_STAND_IN.replace("exec sleep 30", "exec sleep 30 # --github-fixture"))
        checked = launch(tests_on, "--check")
        expect(checked.returncode == 1 and "TESTS_ON_BUILD:" in checked.stdout, f"a tests-on image is refused: {checked.stdout!r}")

        silent = live_root(directory, "silent", stand_in=LIVE_STAND_IN.replace("echo CLI_ENTRY_OFFERED_OK; ", ""))
        checked = launch(silent, "--check")
        expect(checked.returncode == 1 and "CHECK_FAILED (self-test)" in checked.stdout and "CLI_ENTRY_OFFERED_OK" in checked.stdout,
               f"a self-test that does not offer the CLI entry fails the check: {checked.stdout!r}")

        shared = live_root(directory, "shared", stand_in=LIVE_STAND_IN.replace('home="${CFFIXED_USER_HOME:-$HOME}"', f'home="{directory}/elsewhere"'))
        checked = launch(shared, "--check")
        expect(checked.returncode == 1 and "CHECK_FAILED (launch): Not up with its data under" in checked.stdout,
               f"an application that keeps its data outside the root's home fails the check: {checked.stdout!r}")

        owner = launch(live)
        expect(owner.returncode == 0 and "The window is yours" in owner.stdout and (live / "home/Library/Application Support/BetterPullReview").is_dir(),
               f"the owner's launch opens on the root's own home: {owner.stdout!r}")
        recorded = (live / "home/home-was").read_text().strip() if (live / "home/home-was").is_file() else ""
        expect(recorded == os.environ.get("HOME", ""), f"HOME is kept, so gh finds the person's sign-in: {recorded!r}")
        if "pid " in owner.stdout:
            pid = int(owner.stdout.split("pid ", 1)[1].split(")", 1)[0])
            os.killpg(pid, 15)
        in_use = launch(live, "--check")
        expect(in_use.returncode == 0 and "under a scratch home (the root's home holds a profile)" in in_use.stdout
               and (live / "home/home-was").is_file(),
               f"a check after the owner signed in leaves their home alone: {in_use.stdout!r}")
        freeze.set_writable(directory, True)

    if FAILURES:
        for failure in FAILURES:
            print(f"FAILED: {failure}")
        return 1
    print("FREEZE_ATTENDED_BUILD_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
