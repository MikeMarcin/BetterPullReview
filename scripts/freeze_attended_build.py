#!/usr/bin/env python3
"""Freezes a tests-on build for an attended look, and removes one.

A frozen attended build is a commit exported into its own root, built there,
and opened by a launcher the owner runs. A tests-on build compiles absolute
paths to its own `src/tests/fixtures`, so the launcher checks the fixture tree
before it starts the application. That check used to run only when the owner
launched: BPR-1331 and BPR-1339's v001 launchers checked
`tests/fixtures/github/pull-requests/conventional-comments-v1`, a directory no
commit ever had (the fixture's contract is
`tests/fixtures/github/contracts/v1/conventional-comments-v1`), and both
reviews came back as `FIXTURES_MISSING`. The application, launched by hand with
the same arguments, had shown the file.

So the launcher is written here, from one template, and the freeze runs it:
`launch.py --check` makes the launcher's own fixture check and then starts the
application offscreen with the launcher's own arguments until the file shows
(and, for `--next-push`, again on the pushed head). A freeze whose launcher
cannot reach the file fails here, not in front of the owner.

Usage:
    freeze_attended_build.py freeze --name NAME --fixture FIXTURE --open-file PATH
        [--commit REV] [--title TEXT] [--next-push] [--require RELPATH]... [--parent DIR]
    freeze_attended_build.py remove ROOT

`freeze` prints `FROZEN_ROOT:<root>` and `LAUNCH:python3 <root>/launch.py`, or
`FREEZE_FAILED:<reason>` and exits 1 (the root is kept for diagnosis).
`remove` deletes only a root this script froze.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[1]
MARKER = ".frozen-attended-build.json"
BUILD = "build/app-release-tests"
EXECUTABLE = f"{BUILD}/src/app/better_pull_review.app/Contents/MacOS/better_pull_review"
FIXTURE_TREES = ("tests/fixtures/github/contracts/v1", "tests/fixtures/github/pull-requests")

LAUNCHER = '''#!/usr/bin/env python3
"""Open the frozen build for the owner's look: {title}.

Frozen from {commit} by scripts/freeze_attended_build.py. Starts the
fixture pull request {fixture} signed in, with {open_file} open in Review,
and leaves the window to you. Checks the build and the fixture tree first and
reports within seconds if the application exits.
{next_push_doc}
    --check     make the same checks, then open it offscreen until the file
                shows and close it (what the freeze ran){next_push_check}
"""
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "src"
EXECUTABLE = SOURCE / {executable!r}
REQUIRED = [SOURCE / relative for relative in {required!r}]
FIXTURE = {fixture!r}
OPEN_FILE = {open_file!r}
NEXT_PUSH = {next_push!r}
DATA = ROOT / "review-data"
PUSHED = DATA / "pushed"
LOG = ROOT / "launch.log"


def preflight() -> bool:
    if not EXECUTABLE.is_file():
        print(f"BUILD_MISSING: {{EXECUTABLE}}")
        return False
    for path in REQUIRED:
        if not path.exists():
            print(f"FIXTURES_MISSING: {{path}}")
            return False
    return True


def arguments(data, phase):
    result = [str(EXECUTABLE), "--github-fixture", FIXTURE, "--attended-launch", "--attended-open-file", OPEN_FILE]
    if data is not None:
        result += ["--epoch-fixture-root", str(data / "data"), "--epoch-fixture-phase", phase]
    return result


def private_directory(path):
    path.mkdir(mode=0o700, exist_ok=True)
    os.chmod(path, 0o700)
    return path


def start(command, log_path, offscreen):
    environment = dict(os.environ)
    environment.pop("QT_QPA_PLATFORM", None)
    environment.pop("QT_STYLE_OVERRIDE", None)
    if offscreen:
        environment["QT_QPA_PLATFORM"] = "offscreen"
    with log_path.open("w", encoding="utf-8") as log:
        return subprocess.Popen(command, cwd=str(ROOT), env=environment, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)


def await_shown(process, log_path):
    """Returns (shown line or None, head or None, message)."""
    deadline = time.monotonic() + 60.0
    while time.monotonic() < deadline:
        text = log_path.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines()
        head = next((line.split(":", 1)[1] for line in lines if line.startswith("ATTENDED_HEAD:")), None)
        for line in lines:
            if line.startswith("ATTENDED_FILE_SHOWN") or line.startswith("ATTENDED_FILE_FAILED"):
                return line, head, ""
        if process.poll() is not None:
            return None, head, f"EXITED {{process.returncode}} within seconds; log {{log_path}}:\\n{{text}}"
        time.sleep(0.25)
    return None, None, f"No setup line after 60 seconds (pid {{process.pid}}). Log: {{log_path}}"


def stop(process):
    if process.poll() is None:
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()


def check(native):
    """Opens the build as the launcher does and closes it once the file shows."""
    if not preflight():
        return 1
    scratch = Path(tempfile.mkdtemp(prefix="check.", dir=str(ROOT)))
    try:
        courses = [("single", "first head")]
        if NEXT_PUSH:
            courses.append(("reuse", "next push"))
        heads = []
        for phase, label in courses:
            data = scratch if NEXT_PUSH else None
            log_path = scratch / f"{{phase}}.log"
            process = start(arguments(data, phase), log_path, offscreen=not native)
            try:
                line, head, message = await_shown(process, log_path)
            finally:
                stop(process)
            if line is None or not line.startswith("ATTENDED_FILE_SHOWN"):
                print(f"CHECK_FAILED ({{label}}): {{line or message}}")
                return 1
            heads.append(head)
            print(f"CHECK_OK ({{label}}): {{line}} head {{head}}")
        if NEXT_PUSH and (not heads[0] or heads[0] == heads[1]):
            print(f"CHECK_FAILED: the next push showed head {{heads[1]}}, the first head {{heads[0]}}")
            return 1
        return 0
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    options = set(sys.argv[1:])
    known = {{"--check", "--native"}} | ({{"--next-push", "--fresh"}} if NEXT_PUSH else set())
    if not options <= known or ("--native" in options and "--check" not in options):
        print(__doc__)
        return 2
    if "--check" in options:
        return check("--native" in options)
    if not preflight():
        return 1
    data = None
    phase = "single"
    if NEXT_PUSH:
        if "--fresh" in options and DATA.exists():
            shutil.rmtree(DATA)
        data = private_directory(DATA)
        if "--next-push" in options:
            phase = "reuse"
            PUSHED.touch()
        elif PUSHED.exists():
            print("This review data has already seen the push. Run with --next-push to open it again,")
            print("or with --fresh to start over on the first head.")
            return 1
    process = start(arguments(data, phase), LOG, offscreen=False)
    line, head, message = await_shown(process, LOG)
    if line is None:
        print(message)
        return 1
    print(f"{{line}}\\nThe window is yours (pid {{process.pid}}, head {{head}}). Log: {{LOG}}")
    return 0 if line.startswith("ATTENDED_FILE_SHOWN") else 1


if __name__ == "__main__":
    sys.exit(main())
'''

NEXT_PUSH_DOC = """
Your marks are kept between launches. To see the pull request get its next
push (only src/engine/renderer.cpp changes), close the window and run:

    python3 {root}/launch.py --next-push

    --fresh     forget the marks and start over on the first head
"""
NEXT_PUSH_CHECK = "; with the push, it\n                opens the pushed head too, on scratch data"


def render_launcher(*, root: Path, title: str, commit: str, fixture: str, open_file: str, required: list[str], next_push: bool) -> str:
    """The launcher for one frozen root, from the one template."""
    return LAUNCHER.format(
        title=title,
        commit=commit,
        fixture=fixture,
        open_file=open_file,
        executable=EXECUTABLE,
        required=required,
        next_push=next_push,
        next_push_doc=NEXT_PUSH_DOC.format(root=root) if next_push else "",
        next_push_check=NEXT_PUSH_CHECK if next_push else "",
    )


def git(*arguments: str) -> str:
    return subprocess.run(["git", "-C", str(REPOSITORY), *arguments], check=True, capture_output=True, text=True).stdout.strip()


def fail(reason: str) -> int:
    print(f"FREEZE_FAILED:{reason}")
    return 1


def fixture_paths(source: Path, fixture: str) -> list[str]:
    """The fixture's own tree, looked up where the build serves fixtures from."""
    found = []
    for tree in FIXTURE_TREES:
        candidate = source / tree / fixture
        if candidate.is_dir():
            found.append(f"{tree}/{fixture}")
    return found


def set_writable(root: Path, writable: bool) -> None:
    for directory, folders, files in os.walk(root):
        for name in [*folders, *files]:
            path = Path(directory) / name
            if path.is_symlink():
                continue
            mode = path.stat().st_mode
            os.chmod(path, mode | stat.S_IWUSR if writable else mode & ~(stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH))
    mode = root.stat().st_mode
    os.chmod(root, mode | stat.S_IWUSR if writable else mode & ~(stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH))


def freeze(options: argparse.Namespace) -> int:
    commit = git("rev-parse", "--verify", f"{options.commit}^{{commit}}")
    parent = Path(options.parent).expanduser().resolve()
    root = Path(tempfile.mkdtemp(prefix=f"{options.name}.", dir=str(parent)))
    source = root / "src"
    source.mkdir()
    print(f"FREEZING:{root} from {commit}", flush=True)

    archive = subprocess.Popen(["git", "-C", str(REPOSITORY), "archive", "--format=tar", commit], stdout=subprocess.PIPE)
    extracted = subprocess.run(["tar", "-x", "-C", str(source)], stdin=archive.stdout, check=False)
    archive.stdout.close()
    if archive.wait() != 0 or extracted.returncode != 0:
        return fail(f"git archive of {commit} did not extract into {source}")

    required = ["tests/fixtures", *fixture_paths(source, options.fixture), *options.require]
    if len(required) == 1 + len(options.require):
        return fail(f"{options.fixture} has no fixture tree under {', '.join(FIXTURE_TREES)} in {commit}")
    for relative in required:
        if not (source / relative).exists():
            return fail(f"{relative} is not in the export of {commit}")

    with (root / "configure.log").open("w") as log:
        configured = subprocess.run(
            ["cmake", "--preset", "app-release", "-B", BUILD, "-DBUILD_TESTING=ON"],
            cwd=str(source), stdout=log, stderr=subprocess.STDOUT, check=False)
    if configured.returncode != 0:
        return fail(f"configure failed; see {root / 'configure.log'}")
    with (root / "build.log").open("w") as log:
        built = subprocess.run(
            ["cmake", "--build", BUILD, "--target", "app_better_pull_review"],
            cwd=str(source), stdout=log, stderr=subprocess.STDOUT, check=False)
    if built.returncode != 0:
        return fail(f"build failed; see {root / 'build.log'}")

    executable = source / EXECUTABLE
    launcher = root / "launch.py"
    launcher.write_text(render_launcher(root=root, title=options.title, commit=commit, fixture=options.fixture,
                                        open_file=options.open_file, required=required, next_push=options.next_push))
    launcher.chmod(0o755)
    digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    (root / MARKER).write_text(json.dumps({
        "commit": commit, "fixture": options.fixture, "open_file": options.open_file,
        "required": required, "next_push": options.next_push, "executable_sha256": digest,
    }, indent=1) + "\n")

    # The freeze-time check: the launcher's own preflight and its own launch.
    checked = subprocess.run([sys.executable, str(launcher), "--check"], check=False, capture_output=True, text=True)
    print(checked.stdout, end="")
    if checked.returncode != 0:
        return fail(f"the launcher's own check failed; the root is kept at {root}")
    # Nothing in the source or build may change after the check.
    set_writable(source, False)
    print(f"EXECUTABLE_SHA256:{digest}")
    print(f"FROZEN_ROOT:{root}")
    print(f"LAUNCH:python3 {launcher}")
    return 0


def remove(root_text: str) -> int:
    root = Path(root_text).expanduser().resolve()
    if not (root / MARKER).is_file() and not (root / "launch.py").is_file():
        print(f"REMOVE_REFUSED:{root} is not a frozen attended build")
        return 1
    set_writable(root, True)
    shutil.rmtree(root)
    print(f"REMOVED:{root}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    frozen = commands.add_parser("freeze")
    frozen.add_argument("--name", required=True)
    frozen.add_argument("--fixture", required=True)
    frozen.add_argument("--open-file", required=True)
    frozen.add_argument("--commit", default="HEAD")
    frozen.add_argument("--title", default="")
    frozen.add_argument("--next-push", action="store_true")
    frozen.add_argument("--require", action="append", default=[])
    frozen.add_argument("--parent", default="~")
    removed = commands.add_parser("remove")
    removed.add_argument("root")
    options = parser.parse_args()
    if options.command == "freeze":
        return freeze(options)
    return remove(options.root)


if __name__ == "__main__":
    sys.exit(main())
