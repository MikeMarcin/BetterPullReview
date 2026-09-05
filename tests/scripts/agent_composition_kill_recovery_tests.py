#!/usr/bin/env python3
"""Kill an ordinary composed agent session, then inspect and reopen its state."""
from __future__ import annotations

import argparse
from contextlib import closing
import hashlib
import os
from pathlib import Path
import signal
import sqlite3
import stat
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from process_kill_support import kill_at_checkpoint  # noqa: E402
from agent_parent_death_tests import running  # noqa: E402


def inspect_state(git: Path, root: Path, session_count: int) -> tuple:
    databases = list(root.rglob("*.sqlite3"))
    sessions, sequences, epochs = [], [], []
    if not databases:
        raise RuntimeError("application retained no SQLite databases")
    for database in databases:
        with closing(sqlite3.connect(database.as_uri() + "?mode=ro", uri=True)) as connection:
            if connection.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
                raise RuntimeError("application SQLite integrity failed")
            if connection.execute("PRAGMA foreign_key_check").fetchall():
                raise RuntimeError("application SQLite foreign keys failed")
            tables = {row[0] for row in connection.execute("SELECT name FROM sqlite_master WHERE type='table'")}
            if "agent_session" in tables:
                sessions += connection.execute("SELECT * FROM agent_session ORDER BY scope_id, session_id").fetchall()
                sequences += connection.execute("SELECT * FROM agent_session_sequence ORDER BY scope_id").fetchall()
            if "epoch" in tables:
                epochs += connection.execute("SELECT * FROM epoch").fetchall()
    if len(sessions) != session_count or len(sequences) != 1 or sequences[0][1] != session_count:
        raise RuntimeError("agent catalog lost or duplicated a committed session/sequence")
    if [row[1] for row in sessions] != list(range(1, session_count + 1)):
        raise RuntimeError("agent session identities are not the admitted monotonic sequence")
    base = bytes.fromhex(root.joinpath("base-oid").read_text())
    head = bytes.fromhex(root.joinpath("head-oid").read_text())
    if any(row[6] != 1 or row[7] != base.ljust(32, b"\0")
           or row[8] != 1 or row[9] != head.ljust(32, b"\0")
           or row[10:] != (1, 1) for row in sessions):
        raise RuntimeError("persisted session does not retain the exact fixture binding")
    if len(epochs) != 1:
        raise RuntimeError("application lost or duplicated its epoch")
    repositories = list(root.glob("**/repositories/v1/*.git"))
    if len(repositories) != 1:
        raise RuntimeError("application did not retain one bare Git cache")
    environment = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
    environment.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull,
                       GIT_OPTIONAL_LOCKS="0", GIT_TERMINAL_PROMPT="0")
    def inspect(*arguments: str) -> str:
        return subprocess.run([str(git), f"--git-dir={repositories[0]}", *arguments],
                              env=environment, check=True, capture_output=True,
                              text=True, timeout=15).stdout
    inspect("fsck", "--full", "--strict", "--no-reflogs")
    refs = inspect("for-each-ref", "--format=%(refname) %(objectname)")
    expected = sorted((root.joinpath("base-oid").read_text(), root.joinpath("head-oid").read_text()))
    if sorted(row.split()[1] for row in refs.splitlines()) != expected:
        raise RuntimeError("Git cache did not retain the exact base/head ref pair")
    return sessions, sequences, epochs, refs


def workspace_snapshot(root: Path) -> tuple:
    result = []
    for path in [root, *sorted(root.rglob("*"))]:
        facts = path.lstat()
        content = (os.readlink(path) if stat.S_ISLNK(facts.st_mode)
                   else hashlib.sha256(path.read_bytes()).hexdigest() if stat.S_ISREG(facts.st_mode)
                   else None)
        result.append((str(path.relative_to(root)), facts.st_dev, facts.st_ino,
                       facts.st_mode, facts.st_size, content))
    return tuple(result)


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("executable", "canary", "git"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-agent-composition-kill-") as raw:
        parent = Path(raw).resolve()
        root = parent / "data"
        root.mkdir()
        pids = []
        stopped = False
        try:
            kill_at_checkpoint(args.executable, "crash-running", root,
                               arguments=(str(args.canary),), checkpoint_timeout=60)
            pids = [int(root.joinpath(name).read_text()) for name in ("agent.pid", "descendant.pid")]
            if len(set(pids)) != 2 or any(pid <= 1 for pid in pids):
                raise RuntimeError("invalid fixture process identities")
            heartbeat = Path(root.joinpath("heartbeat-path").read_text())
            if not heartbeat.is_relative_to(parent) or heartbeat.stat().st_size == 0:
                raise RuntimeError("descendant never worked in the contained workspace")
            deadline = time.monotonic() + 5
            while any(running(pid) for pid in pids):
                if time.monotonic() >= deadline:
                    raise RuntimeError("composed agent survived application SIGKILL")
                time.sleep(0.02)
            stopped = True
            baseline = inspect_state(args.git, root, 1)
            workspace = Path(root.joinpath("workspace-parent").read_text())
            if workspace.parent != parent:
                raise RuntimeError("workspace escaped the fixture-owned parent")
            original = workspace_snapshot(workspace)
            def invoke(course: str, marker: str) -> None:
                result = subprocess.run([str(args.executable), course, str(args.canary), str(root)],
                                        check=False, capture_output=True, text=True, timeout=90)
                if result.returncode != 0:
                    raise RuntimeError(f"{course} failed ({result.returncode}):\n{result.stdout[-4000:]}\n{result.stderr[-4000:]}")
                if marker not in result.stdout:
                    raise RuntimeError(f"{course} did not prove its public-path result")
            for index in range(2):
                invoke("positive-reopen", "agent-invocation-composition-reopen:pass")
                if inspect_state(args.git, root, 1) != baseline:
                    raise RuntimeError("ordinary reopen changed committed session/cache identity")
                if workspace_snapshot(workspace) != original:
                    raise RuntimeError("reopen changed the old workspace without recovery ownership")
                print(f"agent-composition-kill:reopen-{index + 1}:pass", flush=True)
            invoke("crash-reinvoke", "agent-invocation-crash-reinvoke:pass")
            current = inspect_state(args.git, root, 2)
            if current[0][0] != baseline[0][0] or current[2:] != baseline[2:]:
                raise RuntimeError("new explicit invocation changed prior session/epoch/cache identity")
            if current[0][1][2:] != current[0][0][2:]:
                raise RuntimeError("new session did not preserve the exact admitted binding")
            if workspace_snapshot(workspace) != original:
                raise RuntimeError("new invocation modified the interrupted workspace")
            print(f"agent-composition-kill:orphan-workspace-retained entries={len(original)}", flush=True)
            print("agent-composition-kill:pass", flush=True)
        finally:
            if not stopped:
                if not pids:
                    pids = [int(path.read_text()) for name in ("agent.pid", "descendant.pid")
                            if (path := root / name).exists()]
                for pid in pids:
                    if pid > 1:
                        try:
                            os.kill(pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                deadline = time.monotonic() + 5
                while any(pid > 1 and running(pid) for pid in pids):
                    if time.monotonic() >= deadline:
                        raise RuntimeError("fixture child cleanup failed")
                    time.sleep(0.02)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
