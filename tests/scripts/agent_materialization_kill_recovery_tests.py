#!/usr/bin/env python3
"""Kill the composed application while its selected Git materializer is stopped."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import signal
import subprocess
import time

from agent_composition_kill_recovery_tests import inspect_state, workspace_snapshot
from agent_parent_death_tests import running
from process_kill_support import kill_at_checkpoint
from scripts.contained_temporary_support import contained_temporary_directory


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("executable", "canary", "git", "wrapper"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    for stage in ("init", "checkout", "cancel"):
        with contained_temporary_directory(prefix="bpr-materialization-kill-") as raw:
            parent = Path(raw).resolve()
            root = parent / "data"
            root.mkdir()
            git_pid = supervisor_pid = None
            baseline = None
            workspace = None
            paused = False
            dead = False
            def recover() -> None:
                result = subprocess.run(
                    [str(args.executable), "startup-recover", str(args.canary), str(root)],
                    capture_output=True, text=True, timeout=45, check=False)
                if result.returncode != 0 or "agent-invocation-startup-recover:pass" not in result.stdout:
                    raise RuntimeError(f"ordinary startup recovery failed:\n{result.stdout[-4000:]}\n{result.stderr[-4000:]}")
            def before_kill() -> None:
                nonlocal git_pid, supervisor_pid, baseline, workspace, paused
                git_pid = int(root.joinpath("git.pid").read_text())
                supervisor_pid = int(root.joinpath("supervisor.pid").read_text())
                if min(git_pid, supervisor_pid) <= 1 or git_pid == supervisor_pid:
                    raise RuntimeError("invalid materializer process identities")
                workspace = Path(root.joinpath("workspace-parent").read_text())
                if workspace.parent != parent / "data-agent-workspaces-v1":
                    raise RuntimeError("materializer escaped the fixture-owned neighbourhood")
                # Give the live helper several poll intervals to observe the
                # stopped child. Darwin's waitid can return CLD_STOPPED for
                # WEXITED; treating that as exit killed the writer here.
                time.sleep(0.1)
                if not running(git_pid) or not running(supervisor_pid):
                    raise RuntimeError(f"materializer disappeared before crash observation: git={running(git_pid)} supervisor={running(supervisor_pid)}")
                baseline = inspect_state(args.git, root, 1)
                if not running(git_pid) or not running(supervisor_pid):
                    raise RuntimeError(f"materializer disappeared during independent state inspection: git={running(git_pid)} supervisor={running(supervisor_pid)}")
                os.kill(supervisor_pid, signal.SIGSTOP)
                paused = True
            def after_kill() -> None:
                if not running(git_pid) or not running(supervisor_pid):
                    raise RuntimeError("materialization course lost its paused writer or supervisor")
                records = list((root / "agent-workspace-ownership-v1").glob("*.jsonl"))
                if len(records) != 1:
                    raise RuntimeError("materialization lacks its exact journal")
                journal = records[0].read_bytes()
                original = workspace_snapshot(workspace)
                for _ in range(2):
                    recover()
                    if (not workspace.exists() or workspace_snapshot(workspace) != original
                            or records[0].read_bytes() != journal
                            or inspect_state(args.git, root, 1) != baseline):
                        raise RuntimeError("recovery changed ownership while Git could still write")
            try:
                if stage == "cancel":
                    result = subprocess.run(
                        [str(args.executable), "cancel-materializing-checkout", str(args.canary), str(args.wrapper), str(root)],
                        capture_output=True, text=True, check=False, timeout=60)
                    if result.returncode != 0 or "agent-materialization-cancel:pass" not in result.stdout:
                        raise RuntimeError(f"materialization cancellation failed:\n{result.stdout[-4000:]}\n{result.stderr[-4000:]}")
                    git_pid = int(root.joinpath("git.pid").read_text())
                    supervisor_pid = int(root.joinpath("supervisor.pid").read_text())
                    workspace = Path(root.joinpath("workspace-parent").read_text())
                    if min(git_pid, supervisor_pid) <= 1 or workspace.parent != parent / "data-agent-workspaces-v1":
                        raise RuntimeError("invalid cancellation fixture identity")
                    baseline = inspect_state(args.git, root, 1)
                else:
                    kill_at_checkpoint(args.executable, f"crash-materializing-{stage}", root,
                                       arguments=(str(args.canary), str(args.wrapper)),
                                       checkpoint_timeout=60, before_kill=before_kill,
                                       after_kill=after_kill)
                    # Keep Git itself stopped. The resumed supervisor must kill
                    # and reap it before releasing the journal lock.
                    os.kill(supervisor_pid, signal.SIGCONT)
                    paused = False
                deadline = time.monotonic() + 5
                while running(git_pid) or running(supervisor_pid):
                    if time.monotonic() >= deadline:
                        raise RuntimeError("materializing Git survived application death")
                    time.sleep(0.02)
                dead = True
                for _ in range(2):
                    recover()
                    if (workspace.exists()
                            or list((root / "agent-workspace-ownership-v1").glob("*.jsonl"))
                            or inspect_state(args.git, root, 1) != baseline):
                        raise RuntimeError("ordinary startup failed exact materialization recovery")
                print(f"agent-materialization-kill:{stage}:pass", flush=True)
            finally:
                if paused and supervisor_pid is not None:
                    try:
                        os.kill(supervisor_pid, signal.SIGCONT)
                    except ProcessLookupError:
                        pass
                if not dead:
                    # Kill the exact fixture writer first; retain the helper
                    # until it has had an opportunity to perform normal reap.
                    git_pid = git_pid or (int(path.read_text()) if (path := root / "git.pid").exists() else None)
                    if git_pid is not None and git_pid > 1:
                        try:
                            os.kill(git_pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                    deadline = time.monotonic() + 5
                    while any(pid is not None and pid > 1 and running(pid) for pid in (git_pid, supervisor_pid)):
                        if time.monotonic() >= deadline:
                            raise RuntimeError("materialization fixture process cleanup failed")
                        time.sleep(0.02)
    print("agent-materialization-kill:pass", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
