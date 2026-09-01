#!/usr/bin/env python3
"""Exclusive writer lease for remote stations and the project integrator.

`run` wraps a command in an OS-level exclusive lock that lives exactly as long
as the wrapped process, so a second writer is denied mechanically and a crash
releases the lease without cleanup. `status` reports whether a lease is held
and by whom. The lease guarantees single-writer per scope; it does not
authenticate the writer — ownership rules and push credentials do that.

Scopes:
- station-writer: one writer per checkout (lock lives in the checkout's own
  git dir, so each worktree leases independently).
- project-integrator: one integrator per repository (lock lives in the common
  git dir, shared by every worktree).

Exit codes: 0 success/free, 10 held (status), 75 lease denied (run),
2 usage or environment error.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import os
import socket
import subprocess
import sys
from pathlib import Path

ROLES = ("station-writer", "project-integrator")

sys.path.insert(0, str(Path(__file__).resolve().parent))
import project_config  # noqa: E402

PROJECT_TOKEN = project_config.load()["project"]

if os.name == "posix":
    import fcntl

    def _try_lock(fd: int) -> bool:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            return True
        except OSError:
            return False

    def _unlock(fd: int) -> None:
        fcntl.flock(fd, fcntl.LOCK_UN)

else:  # native Windows
    import msvcrt

    def _try_lock(fd: int) -> bool:
        try:
            os.lseek(fd, 0, os.SEEK_SET)
            msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
            return True
        except OSError:
            return False

    def _unlock(fd: int) -> None:
        os.lseek(fd, 0, os.SEEK_SET)
        msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)


def _git_dir(role: str, repo: Path) -> Path:
    flag = "--git-common-dir" if role == "project-integrator" else "--git-dir"
    proc = subprocess.run(
        ["git", "-C", str(repo), "rev-parse", flag],
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        sys.stderr.write(f"not a git repository: {repo}\n{proc.stderr}")
        raise SystemExit(2)
    path = Path(proc.stdout.strip())
    if not path.is_absolute():
        path = (repo / path).resolve()
    return path


def _lock_path(role: str, repo: Path) -> Path:
    return _git_dir(role, repo) / f"{PROJECT_TOKEN}-{role}.lock"


def _holder_path(lock_path: Path) -> Path:
    return lock_path.with_suffix(".holder.json")


def _holder_record(args: argparse.Namespace, command: list[str]) -> dict:
    return {
        "schema": "implement-rfc.station-lease.v1",
        "project": PROJECT_TOKEN,
        "role": args.role,
        "station": args.station,
        "pid": os.getpid(),
        "host": socket.gethostname(),
        "command": command[0] if command else None,
        "acquired_at": _dt.datetime.now(_dt.timezone.utc)
        .isoformat(timespec="seconds")
        .replace("+00:00", "Z"),
    }


def _read_holder(path: Path) -> str:
    try:
        text = path.read_text(encoding="utf-8").strip()
        return text or "(no holder record)"
    except OSError:
        return "(no holder record)"


def cmd_run(args: argparse.Namespace) -> int:
    if not args.command:
        sys.stderr.write("run: missing command after --\n")
        return 2
    path = _lock_path(args.role, args.repo)
    holder_path = _holder_path(path)
    fd = os.open(path, os.O_RDWR | os.O_CREAT, 0o644)
    if not _try_lock(fd):
        sys.stderr.write(
            f"{args.role} lease is held; refusing a second writer.\n"
            f"holder: {_read_holder(holder_path)}\n"
        )
        os.close(fd)
        return 75
    record = json.dumps(_holder_record(args, args.command), indent=2) + "\n"
    holder_path.write_text(record, encoding="utf-8")
    if os.name == "posix":
        # Drop close-on-exec so the lock outlives the exec into the command.
        flags = fcntl.fcntl(fd, fcntl.F_GETFD)
        fcntl.fcntl(fd, fcntl.F_SETFD, flags & ~fcntl.FD_CLOEXEC)
        try:
            os.execvp(args.command[0], args.command)
        except OSError as error:
            sys.stderr.write(f"exec failed: {error}\n")
            return 2
    proc = subprocess.run(args.command)
    _unlock(fd)
    os.close(fd)
    return proc.returncode


def cmd_status(args: argparse.Namespace) -> int:
    path = _lock_path(args.role, args.repo)
    holder_path = _holder_path(path)
    if not path.exists():
        print(f"{args.role}: free")
        return 0
    fd = os.open(path, os.O_RDWR | os.O_CREAT, 0o644)
    try:
        if _try_lock(fd):
            _unlock(fd)
            print(f"{args.role}: free")
            return 0
        print(f"{args.role}: held")
        print(_read_holder(holder_path))
        return 10
    finally:
        os.close(fd)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)

    def common(p: argparse.ArgumentParser) -> None:
        p.add_argument("--role", choices=ROLES, default="station-writer")
        p.add_argument("--repo", type=Path, default=Path.cwd())

    run_p = sub.add_parser("run", help="acquire the lease and run a command")
    common(run_p)
    run_p.add_argument("--station", default=None, help="station name for the holder record")
    run_p.add_argument("command", nargs=argparse.REMAINDER, help="-- command to wrap")

    status_p = sub.add_parser("status", help="report the lease state")
    common(status_p)
    status_p.set_defaults(station=None)

    args = parser.parse_args(argv)
    if args.mode == "run":
        if args.command and args.command[0] == "--":
            args.command = args.command[1:]
        return cmd_run(args)
    return cmd_status(args)


if __name__ == "__main__":
    raise SystemExit(main())
