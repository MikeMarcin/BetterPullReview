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

Passed straight through: `--version`, `-t` tools, `-n` dry runs and `-h`;
and a Ninja nested inside a jobserver job (MAKEFLAGS already names one),
which must not take a second token from the pool its parent draws on.

The real Ninja is $BPR_NINJA or the first `ninja` on PATH that is not this
file. Runs on Python 3.9 and newer.
"""

from __future__ import annotations

import os
import re
import select
import shutil
import signal
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_jobserver  # noqa: E402

PASS_THROUGH = {"--version", "-t", "-n", "-h", "--help"}


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

    paths = build_jobserver.Paths()
    fd = token = lease = None
    if build_jobserver.live_server(paths):
        lease = build_jobserver.take_lease(paths)
        fd, token = take_token(paths)
    if fd is None:
        if lease is not None:
            build_jobserver.drop_lease(paths, lease)
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
    child = subprocess.Popen([ninja, *arguments], env=environment, pass_fds=(lease,))

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
