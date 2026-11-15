#!/usr/bin/env python3
"""The dev preset's CMAKE_MAKE_PROGRAM: Ninja that joins the build jobserver.

`cmake --build --preset dev` runs this with `-j 8` (the preset's "jobs").
Ninja ignores a jobserver whenever -j is given, so this wrapper decides:

* The server of scripts/build_jobserver.py runs: take one token (it pays for
  Ninja's implicit first job, keeping the machine at N jobs in total), drop
  -j, name the FIFO in MAKEFLAGS and run Ninja, which then takes a token for
  every further job. The token goes back when Ninja exits.
* It does not run, or its FIFO is gone: run Ninja as asked, adding -j 8 when
  no -j was given. A missing server never fails or stalls a build.

All tokens taken: the build waits here for one, checking every second that
the server still lives, and falls back to -j 8 if it dies meanwhile.

One build per tree. Two Ninjas in one build directory race on the same
outputs (a static archive rewritten mid-link reads as "malformed archive").
Every build, joined or fallback, first takes an exclusive flock on
`.bpr-build.lock` in its build directory and holds it until Ninja exits, so
builds of one tree run one after another while builds of different trees
share the token pool. The lock is taken before any token, so a waiting build
holds none; the holder records its pid and targets in the file and a waiter
prints one line naming them. Ninja inherits the locked descriptor, so the
lock lasts as long as the build does and the kernel drops it on any exit,
SIGKILL included.

Passed straight through, without lock or token: `--version`, `-t` tools
(CMake runs `-t restat` and `-t recompact` while regenerating inside a locked
build), `-n` dry runs and `-h`; a Ninja nested inside a jobserver job
(MAKEFLAGS already names one), which must not take a second token from the
pool its parent draws on; and a Ninja run inside a build that already holds
its directory's lock ($BPR_BUILD_LOCK_HELD).

The real Ninja is $BPR_NINJA or the first `ninja` on PATH that is not this
file. Runs on Python 3.9 and newer.
"""

from __future__ import annotations

import fcntl
import json
import os
import re
import select
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_jobserver  # noqa: E402

PASS_THROUGH = {"--version", "-t", "-n", "-h", "--help"}
LOCK_NAME = ".bpr-build.lock"
HELD_VARIABLE = "BPR_BUILD_LOCK_HELD"
VALUED_OPTIONS = {"-C", "-f", "-j", "-k", "-l", "-d", "-t", "-w"}


def build_directory(arguments: list) -> Path:
    directory = Path.cwd()
    for index, argument in enumerate(arguments):
        if argument == "-C" and index + 1 < len(arguments):
            directory = directory / arguments[index + 1]
        elif argument.startswith("-C") and len(argument) > 2:
            directory = directory / argument[2:]
    return directory.resolve()


def targets(arguments: list) -> list:
    found = []
    skip = False
    for argument in arguments:
        if skip:
            skip = False
        elif argument in VALUED_OPTIONS:
            skip = True
        elif not argument.startswith("-"):
            found.append(argument)
    return found


def lock_build_directory(directory: Path, arguments: list) -> int:
    """Exclusive lock on the build directory, waiting for the build that holds
    it. Returns the inheritable locked fd."""
    path = directory / LOCK_NAME
    fd = os.open(str(path), os.O_RDWR | os.O_CREAT, 0o600)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        try:
            holder = json.loads(path.read_text(encoding="utf-8") or "{}")
        except (OSError, ValueError):
            holder = {}
        named = " ".join(holder.get("targets") or []) or "default targets"
        sys.stderr.write(
            f"jobserver_ninja: waiting for the build already running in {directory} "
            f"(pid {holder.get('pid', '?')}, {named})\n"
        )
        sys.stderr.flush()
        fcntl.flock(fd, fcntl.LOCK_EX)
    record = {"pid": os.getpid(), "targets": targets(arguments), "started": time.time()}
    os.ftruncate(fd, 0)
    os.pwrite(fd, (json.dumps(record) + "\n").encode("utf-8"), 0)
    os.set_inheritable(fd, True)
    return fd


def real_ninja() -> str:
    explicit = os.environ.get("BPR_NINJA")
    if explicit:
        return explicit
    self_path = Path(__file__).resolve()
    for directory in os.environ.get("PATH", "").split(os.pathsep):
        candidate = shutil.which("ninja", path=directory) if directory else None
        if candidate and Path(candidate).resolve() != self_path:
            return candidate
    for candidate in ("/opt/homebrew/bin/ninja", "/usr/local/bin/ninja", "/usr/bin/ninja"):
        if os.access(candidate, os.X_OK):
            return candidate
    sys.stderr.write("jobserver_ninja: no ninja found on PATH; set BPR_NINJA\n")
    sys.exit(127)


def without_jobs(arguments: list) -> tuple:
    """The arguments less every -j, and whether one was there."""
    kept = []
    found = False
    index = 0
    while index < len(arguments):
        argument = arguments[index]
        if argument == "-j" and index + 1 < len(arguments):
            found = True
            index += 2
            continue
        if re.fullmatch(r"-j\d+", argument):
            found = True
            index += 1
            continue
        kept.append(argument)
        index += 1
    return kept, found


def take_token(paths: build_jobserver.Paths) -> tuple:
    """(fd, token) once a token is taken, or (None, None) when the server is
    gone. The fd stays open so the server counts this build as a client."""
    try:
        fd = os.open(str(paths.fifo), os.O_RDWR | os.O_NONBLOCK)
    except OSError:
        return None, None
    waited = False
    while True:
        try:
            token = os.read(fd, 1)
            if token:
                return fd, token
        except BlockingIOError:
            pass
        if not build_jobserver.live_server(paths):
            os.close(fd)
            return None, None
        if not waited:
            sys.stderr.write("jobserver_ninja: every jobserver token is in use; waiting for one\n")
            waited = True
        select.select([fd], [], [], 1.0)


def main() -> int:
    ninja = real_ninja()
    arguments = sys.argv[1:]
    environment = dict(os.environ)
    nested = "--jobserver-auth=" in environment.get("MAKEFLAGS", "")
    if nested or PASS_THROUGH.intersection(arguments):
        os.execv(ninja, [ninja, *arguments])

    directory = build_directory(arguments)
    if not directory.is_dir() or environment.get(HELD_VARIABLE) == str(directory):
        os.execv(ninja, [ninja, *arguments])
    lock = lock_build_directory(directory, arguments)
    os.environ[HELD_VARIABLE] = environment[HELD_VARIABLE] = str(directory)

    paths = build_jobserver.Paths()
    fd = token = lease = None
    if build_jobserver.live_server(paths):
        fd, token = take_token(paths)
    if fd is not None:
        # The lease comes after the token: a build still waiting for one is
        # not a client, so a pool that a SIGKILLed build drained reads as
        # idle and the server refills it instead of leaving waiters stuck.
        lease = build_jobserver.take_lease(paths)
    else:
        if not without_jobs(arguments)[1]:
            arguments = ["-j", str(build_jobserver.FALLBACK_JOBS), *arguments]
        os.execv(ninja, [ninja, *arguments])

    arguments = without_jobs(arguments)[0]
    flags = environment.get("MAKEFLAGS", "").strip()
    environment["MAKEFLAGS"] = f"{flags} --jobserver-auth=fifo:{paths.fifo}".strip()
    # Ninja shares the terminal's process group, so Ctrl-C reaches it
    # directly; this process only waits to hand the token back.
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    # Ninja inherits the lease, so an orphaned Ninja still counts as a client.
    # It inherits the build-directory lock for the same reason.
    child = subprocess.Popen([ninja, *arguments], env=environment, pass_fds=(lease, lock))

    def forward(signum, _frame) -> None:
        child.send_signal(signum)

    signal.signal(signal.SIGTERM, forward)
    signal.signal(signal.SIGHUP, forward)
    try:
        code = child.wait()
    finally:
        try:
            os.write(fd, token)
        except OSError:
            pass
        os.close(fd)
        build_jobserver.drop_lease(paths, lease)
    return code if code >= 0 else 128 - code


if __name__ == "__main__":
    sys.exit(main())
