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
        paused_supervisor = None
        try:
            def invoke(course: str, marker: str) -> None:
                result = subprocess.run([str(args.executable), course, str(args.canary), str(root)],
                                        check=False, capture_output=True, text=True, timeout=90)
                if result.returncode != 0:
                    raise RuntimeError(f"{course} failed ({result.returncode}):\n{result.stdout[-4000:]}\n{result.stderr[-4000:]}")
                if marker not in result.stdout:
                    raise RuntimeError(f"{course} did not prove its public-path result")
            def live_recovery(course: str) -> None:
                live_workspace = Path(root.joinpath("workspace-parent").read_text())
                records = list((root / "agent-workspace-ownership-v1").glob("*.jsonl"))
                if len(records) != 1:
                    raise RuntimeError("running application lacks exact workspace journal")
                identity = live_workspace.stat().st_ino
                receipt = records[0].read_bytes()
                invoke(course, f"agent-invocation-{course}:pass")
                if not live_workspace.exists() or live_workspace.stat().st_ino != identity or records[0].read_bytes() != receipt:
                    raise RuntimeError("another ordinary application reclaimed live workspace ownership")
            def resume_supervisor() -> None:
                nonlocal paused_supervisor
                if paused_supervisor is not None:
                    try:
                        os.kill(paused_supervisor, signal.SIGCONT)
                    except ProcessLookupError:
                        pass
                    paused_supervisor = None
            def pause_supervisor() -> None:
                nonlocal paused_supervisor
                # The live application holds the profile's exclusive runtime
                # lease, so a second ordinary launch on its root is refused
                # before it composes anything (BPR-0808).
                live_recovery("startup-owner-busy")
                paused_supervisor = int(root.joinpath("supervisor.pid").read_text())
                if paused_supervisor <= 1:
                    raise RuntimeError("invalid fixture supervisor identity")
                os.kill(paused_supervisor, signal.SIGSTOP)
            def recover_before_supervisor_teardown() -> None:
                try:
                    if not all(running(int(root.joinpath(name).read_text())) for name in ("supervisor.pid", "agent.pid", "descendant.pid")):
                        raise RuntimeError("supervisor lifetime course lost its live fixture processes")
                    # Owner death released the lease; the paused supervisor
                    # still holds the workspace, so startup must not reclaim it.
                    live_recovery("startup-recover")
                finally:
                    resume_supervisor()
            kill_at_checkpoint(args.executable, "crash-running", root,
                               arguments=(str(args.canary),), checkpoint_timeout=60, before_kill=pause_supervisor, after_kill=recover_before_supervisor_teardown)
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
            if workspace.parent != parent / "data-agent-workspaces-v1":
                raise RuntimeError("workspace escaped the fixture-owned parent")
            original = workspace_snapshot(workspace)
            displaced = parent / "preserved-original"
            substitute = parent / "preserved-substitute"
            workspace.rename(displaced)
            workspace.mkdir(mode=0o700)
            workspace.joinpath("user-canary").write_text("preserve this substitute")
            substitute_identity = workspace.stat().st_ino
            invoke("startup-repair", "agent-invocation-startup-repair:pass")
            if workspace.stat().st_ino != substitute_identity or workspace.joinpath("user-canary").read_text() != "preserve this substitute":
                raise RuntimeError("startup recovery altered the substitute")
            if workspace_snapshot(displaced) != original or inspect_state(args.git, root, 1) != baseline:
                raise RuntimeError("repair refusal altered original workspace or retained state")
            workspace.rename(substitute)
            displaced.rename(workspace)
            invoke("startup-recover", "agent-invocation-startup-recover:pass")
            if workspace.exists() or inspect_state(args.git, root, 1) != baseline:
                raise RuntimeError("startup without opening a PR failed exact workspace reclamation")
            if substitute.stat().st_ino != substitute_identity or substitute.joinpath("user-canary").read_text() != "preserve this substitute":
                raise RuntimeError("recovery changed the preserved substitute")
            for index in range(2):
                invoke("positive-reopen", "agent-invocation-composition-reopen:pass")
                if inspect_state(args.git, root, 1) != baseline:
                    raise RuntimeError("ordinary reopen changed committed session/cache identity")
                if workspace.exists():
                    raise RuntimeError("ordinary reopen retained a durably owned abandoned workspace")
                if list((root / "agent-workspace-ownership-v1").glob("*.jsonl")):
                    raise RuntimeError("ordinary reopen retained abandoned ownership journals")
                print(f"agent-composition-kill:reopen-{index + 1}:pass", flush=True)
            invoke("crash-reinvoke", "agent-invocation-crash-reinvoke:pass")
            current = inspect_state(args.git, root, 2)
            if current[0][0] != baseline[0][0] or current[2:] != baseline[2:]:
                raise RuntimeError("new explicit invocation changed prior session/epoch/cache identity")
            if current[0][1][2:] != current[0][0][2:]:
                raise RuntimeError("new session did not preserve the exact admitted binding")
            if workspace.exists() or list((root / "agent-workspace-ownership-v1").glob("*.jsonl")):
                raise RuntimeError("new invocation retained old or new disposable workspace ownership")
            print(f"agent-composition-kill:owned-workspace-reclaimed entries={len(original)}", flush=True)
            print("agent-composition-kill:pass", flush=True)
        finally:
            if paused_supervisor is not None:
                resume_supervisor()
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
