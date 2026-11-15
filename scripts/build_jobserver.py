#!/usr/bin/env python3
"""A machine-wide GNU make jobserver that every development build joins.

Several agents build at once on one Mac, and full-width Ninja builds side by
side once used 83 GB of RAM. This process owns a FIFO in a fixed per-user
location and seeds it with N tokens (default 12). Ninja 1.13 and newer is a
jobserver client of a FIFO named in MAKEFLAGS (`--jobserver-auth=fifo:PATH`):
it takes a token before each job beyond its first and returns it afterwards.
`scripts/jobserver_ninja.py`, the dev preset's CMAKE_MAKE_PROGRAM, joins a
build to this pool when the server runs and otherwise builds at -j 8.

    python3 scripts/build_jobserver.py start [--tokens N]
    python3 scripts/build_jobserver.py status
    python3 scripts/build_jobserver.py stop

`start` is idempotent: a live server makes it print the status and exit 0.
`stop` signals only the process its own pidfile names, and only while that
process still holds the server lock.

Token leaks. A client killed with SIGKILL never returns the tokens it holds.
The server cannot tell who holds a token, so it reconciles when idle. Each
build through jobserver_ninja.py holds a lease, a locked file under
`clients/` that its Ninja inherits; every few seconds the server counts the
leases still locked, and when there are none every token must be back, so it
tops the FIFO up to N (or takes back a surplus). Limits: under back-to-back
overlapping builds a leaked token stays missing until the first idle moment;
a Ninja given this MAKEFLAGS by hand holds no lease, so a reconciliation
during its run can add tokens it holds (taken back at the next idle moment).
`stop` then `start` always resets the pool. (lsof cannot find a FIFO by path
on macOS, so open-file listings are no substitute for the leases.)

Bound. Each Ninja client also runs one job without a token (the protocol's
implicit slot). jobserver_ninja.py takes one token for the build's lifetime to
pay for that slot, so machine-wide concurrency stays at N. A Ninja started
some other way with this MAKEFLAGS gets its implicit slot for free.

Runs on Python 3.9 (macOS's /usr/bin/python3) and newer; macOS and Linux.
"""

from __future__ import annotations

import argparse
import errno
import fcntl
import json
import os
import signal
import stat
import subprocess
import sys
import termios
import time
from pathlib import Path
from typing import Optional

DEFAULT_TOKENS = 12
FALLBACK_JOBS = 8
TOKEN = b"+"
RECONCILE_SECONDS = 5.0


def state_directory() -> Path:
    """The fixed per-user location. Not $TMPDIR: sandboxed agents see a
    different TMPDIR, and every build on the machine must find one FIFO."""
    override = os.environ.get("BPR_JOBSERVER_DIR")
    if override:
        return Path(override)
    cache = os.environ.get("XDG_CACHE_HOME") or str(Path.home() / ".cache")
    return Path(cache) / "bpr" / "jobserver"


class Paths:
    def __init__(self, root: Optional[Path] = None) -> None:
        self.root = root or state_directory()
        self.fifo = self.root / "fifo"
        self.pidfile = self.root / "server.pid"
        self.lock = self.root / "server.lock"
        self.log = self.root / "server.log"
        self.clients = self.root / "clients"


def lock_held(paths: Paths) -> bool:
    """True when a server holds its exclusive lock. Unlike a bare pid this
    survives pid reuse: the lock dies with the process."""
    try:
        fd = os.open(str(paths.lock), os.O_RDONLY)
    except OSError:
        return False
    try:
        fcntl.flock(fd, fcntl.LOCK_SH | fcntl.LOCK_NB)
    except OSError as error:
        return error.errno in (errno.EWOULDBLOCK, errno.EAGAIN)
    else:
        fcntl.flock(fd, fcntl.LOCK_UN)
        return False
    finally:
        os.close(fd)


def read_record(paths: Paths) -> Optional[dict]:
    try:
        return json.loads(paths.pidfile.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def is_fifo(path: Path) -> bool:
    try:
        return stat.S_ISFIFO(path.lstat().st_mode)
    except OSError:
        return False


def live_server(paths: Paths) -> Optional[dict]:
    """The server's record when it is running and its FIFO exists."""
    if not lock_held(paths):
        return None
    record = read_record(paths)
    if not record or record.get("fifo") != str(paths.fifo) or not is_fifo(paths.fifo):
        return None
    return record


def available_tokens(fd: int) -> int:
    buffer = bytearray(4)
    fcntl.ioctl(fd, termios.FIONREAD, buffer)
    return int.from_bytes(bytes(buffer), sys.byteorder)


def take_lease(paths: Paths) -> int:
    """Register the calling build as a client for as long as the returned fd
    (or an inherited copy of it) stays open. The file is locked before it gets
    its final name, so the server never sees an unlocked live lease."""
    paths.clients.mkdir(parents=True, exist_ok=True, mode=0o700)
    final = paths.clients / str(os.getpid())
    temporary = paths.clients / f".{os.getpid()}.new"
    fd = os.open(str(temporary), os.O_RDWR | os.O_CREAT | os.O_TRUNC, 0o600)
    fcntl.flock(fd, fcntl.LOCK_EX)
    os.replace(str(temporary), str(final))
    return fd


def drop_lease(paths: Paths, fd: int) -> None:
    try:
        (paths.clients / str(os.getpid())).unlink()
    except OSError:
        pass
    os.close(fd)


def live_clients(paths: Paths) -> int:
    """Builds that hold a lease. A lease whose lock is free belongs to a
    client that died; it is removed."""
    count = 0
    try:
        entries = list(paths.clients.iterdir())
    except OSError:
        return 0
    for entry in entries:
        if entry.name.startswith("."):
            continue
        try:
            fd = os.open(str(entry), os.O_RDONLY)
        except OSError:
            continue
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            count += 1
        else:
            try:
                entry.unlink()
            except OSError:
                pass
        finally:
            os.close(fd)
    return count


def log(paths: Paths, message: str) -> None:
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    with open(paths.log, "a", encoding="utf-8") as handle:
        handle.write(f"{stamp} [{os.getpid()}] {message}\n")


def serve(paths: Paths, tokens: int) -> int:
    paths.root.mkdir(parents=True, exist_ok=True, mode=0o700)
    lock_fd = os.open(str(paths.lock), os.O_RDWR | os.O_CREAT, 0o600)
    try:
        fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        return 0  # Another server won the race; it serves.

    # A previous server killed with SIGKILL left its FIFO behind. Clients that
    # still hold the old one keep their own pipe; new ones see the fresh FIFO.
    try:
        paths.fifo.unlink()
    except FileNotFoundError:
        pass
    os.mkfifo(str(paths.fifo), 0o600)
    fifo_fd = os.open(str(paths.fifo), os.O_RDWR | os.O_NONBLOCK)
    # FIONREAD on a read-write FIFO descriptor reads 0 on macOS; count and
    # take back through a read-only one.
    count_fd = os.open(str(paths.fifo), os.O_RDONLY | os.O_NONBLOCK)
    os.write(fifo_fd, TOKEN * tokens)

    record = {"pid": os.getpid(), "fifo": str(paths.fifo), "tokens": tokens, "started": time.time()}
    temporary = paths.pidfile.with_suffix(f".{os.getpid()}")
    temporary.write_text(json.dumps(record) + "\n", encoding="utf-8")
    os.replace(str(temporary), str(paths.pidfile))
    log(paths, f"serving {tokens} tokens at {paths.fifo}")

    stopping = []

    def request_stop(signum, _frame) -> None:
        stopping.append(signum)

    signal.signal(signal.SIGTERM, request_stop)
    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGHUP, signal.SIG_IGN)

    while not stopping:
        time.sleep(RECONCILE_SECONDS)
        if stopping or live_clients(paths):
            continue
        have = available_tokens(count_fd)
        if have < tokens:
            os.write(fifo_fd, TOKEN * (tokens - have))
            log(paths, f"idle with {have} of {tokens} tokens: restored {tokens - have} leaked")
        elif have > tokens:
            os.read(count_fd, have - tokens)
            log(paths, f"idle with {have} of {tokens} tokens: took back {have - tokens}")

    log(paths, f"stopping on signal {stopping[0]}")
    for path in (paths.pidfile, paths.fifo):
        try:
            path.unlink()
        except FileNotFoundError:
            pass
    os.close(count_fd)
    os.close(fifo_fd)
    os.close(lock_fd)
    return 0


def status_text(paths: Paths) -> tuple:
    record = live_server(paths)
    if not record:
        return False, (
            f"jobserver: not running (state {paths.root}); "
            f"builds through the dev preset run at -j {FALLBACK_JOBS}"
        )
    fd = os.open(str(paths.fifo), os.O_RDONLY | os.O_NONBLOCK)
    try:
        free = available_tokens(fd)
    finally:
        os.close(fd)
    clients = live_clients(paths)
    return True, (
        f"jobserver: running pid {record['pid']}, {record['tokens']} tokens, {free} free, "
        f"clients {clients}\n"
        f"  MAKEFLAGS=--jobserver-auth=fifo:{record['fifo']}\n"
        f"  log {paths.log}"
    )


def start(paths: Paths, tokens: int) -> int:
    if tokens < 1:
        print("build_jobserver: --tokens must be at least 1", file=sys.stderr)
        return 2
    if live_server(paths):
        print(status_text(paths)[1])
        return 0
    paths.root.mkdir(parents=True, exist_ok=True, mode=0o700)
    with open(paths.log, "a", encoding="utf-8") as log_handle:
        subprocess.Popen(
            [sys.executable, os.path.abspath(__file__), "serve", "--tokens", str(tokens)],
            stdin=subprocess.DEVNULL,
            stdout=log_handle,
            stderr=log_handle,
            start_new_session=True,
            close_fds=True,
            env={**os.environ, "BPR_JOBSERVER_DIR": str(paths.root)},
        )
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if live_server(paths):
            print(status_text(paths)[1])
            return 0
        time.sleep(0.05)
    print(f"build_jobserver: the server did not come up; see {paths.log}", file=sys.stderr)
    return 1


def stop(paths: Paths) -> int:
    record = read_record(paths)
    if not lock_held(paths):
        for path in (paths.pidfile, paths.fifo):
            if path.exists() or path.is_symlink():
                path.unlink()
        print("jobserver: not running")
        return 0
    if not record or not isinstance(record.get("pid"), int):
        print(f"build_jobserver: the lock is held but {paths.pidfile} names no pid", file=sys.stderr)
        return 1
    pid = record["pid"]
    os.kill(pid, signal.SIGTERM)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if not lock_held(paths):
            print(f"jobserver: stopped pid {pid}")
            return 0
        time.sleep(0.05)
    print(f"build_jobserver: pid {pid} did not stop within 10 s", file=sys.stderr)
    return 1


def main(argv: Optional[list] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("start", "serve"):
        command = commands.add_parser(name, help="start the server (idempotent)" if name == "start" else argparse.SUPPRESS)
        command.add_argument(
            "--tokens",
            type=int,
            default=int(os.environ.get("BPR_JOBSERVER_TOKENS", DEFAULT_TOKENS)),
            help=f"concurrent jobs machine-wide (default {DEFAULT_TOKENS}, or $BPR_JOBSERVER_TOKENS)",
        )
    commands.add_parser("status", help="print whether the server runs; exit 0 when it does, 3 when not")
    commands.add_parser("stop", help="stop the server named by its own pidfile")
    arguments = parser.parse_args(argv)
    paths = Paths()
    if arguments.command == "start":
        return start(paths, arguments.tokens)
    if arguments.command == "serve":
        return serve(paths, arguments.tokens)
    if arguments.command == "status":
        running, text = status_text(paths)
        print(text)
        return 0 if running else 3
    return stop(paths)


if __name__ == "__main__":
    sys.exit(main())
