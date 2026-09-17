#!/usr/bin/env python3
"""Kill the composed application while its selected Git materializer is stopped."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import signal
import subprocess
import threading
import time

from agent_composition_kill_recovery_tests import inspect_state, read_journal, workspace_snapshot
import agent_parent_death_tests as processes
from agent_parent_death_tests import running
from agent_workspace_ownership_tests import make_private
from process_kill_support import kill_at_checkpoint
from scripts.contained_temporary_support import contained_temporary_directory

WINDOWS = os.name != "posix"
SUPERVISOR_IMAGE = "better_pull_review_agent_supervisor.exe"


class WindowsWriters:
    """[[BPR-0622]]. Handles to the paused Git writer and its supervisor.

    The wrapper reports both process IDs and then waits for `handles-captured`
    before it publishes the workspace the application's checkpoint waits for.
    Handles are therefore opened while both processes are alive, their images
    are checked, and every later liveness question reads the handle.
    """

    def __init__(self, root: Path) -> None:
        self.root = root
        self.handles: dict[str, int] = {}
        self.error: BaseException | None = None
        self.thread = threading.Thread(target=self._capture, daemon=True)
        self.thread.start()

    def _capture(self) -> None:
        try:
            deadline = time.monotonic() + 60
            paths = (self.root / "git.pid", self.root / "supervisor.pid")
            while not all(path.exists() and path.read_text() for path in paths):
                if time.monotonic() >= deadline:
                    raise RuntimeError("materializer never reported its process identities")
                time.sleep(0.01)
            git_pid, supervisor_pid = (int(path.read_text()) for path in paths)
            if min(git_pid, supervisor_pid) <= 1 or git_pid == supervisor_pid:
                raise RuntimeError("invalid materializer process identities")
            self.handles["git"] = processes.open_fixture_process(git_pid, "git.exe")
            self.handles["supervisor"] = processes.open_fixture_process(supervisor_pid, SUPERVISOR_IMAGE)
            (self.root / "handles-captured").write_text("captured")
        except BaseException as error:  # reported by wait()
            self.error = error

    def wait(self) -> None:
        self.thread.join(timeout=65)
        if self.thread.is_alive() or self.error is not None or len(self.handles) != 2:
            raise RuntimeError(f"materializer handle capture failed: {self.error}")

    def running(self, name: str) -> bool:
        return not processes.exited(self.handles[name])

    def close(self) -> None:
        for handle in self.handles.values():
            processes.kernel32.CloseHandle(handle)
        self.handles.clear()


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("executable", "canary", "git", "wrapper"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    for stage in ("init", "checkout", "cancel"):
        with contained_temporary_directory(prefix="bpr-materialization-kill-") as raw:
            parent = Path(raw).resolve()
            # The workspace neighbourhood is created only under a parent no other
            # principal can write; a contained root inherits %TEMP%'s entries.
            make_private(parent)
            root = parent / "data"
            root.mkdir()
            git_pid = supervisor_pid = None
            baseline = None
            workspace = None
            paused = False
            dead = False
            writers = WindowsWriters(root) if WINDOWS else None

            def alive(name: str) -> bool:
                if WINDOWS:
                    return writers.running(name)
                return running(git_pid if name == "git" else supervisor_pid)

            def recover() -> None:
                result = subprocess.run(
                    [str(args.executable), "startup-recover", str(args.canary), str(root)],
                    capture_output=True, text=True, timeout=45, check=False)
                if result.returncode != 0 or "agent-invocation-startup-recover:pass" not in result.stdout:
                    raise RuntimeError(f"ordinary startup recovery failed:\n{result.stdout[-4000:]}\n{result.stderr[-4000:]}")

            def pause_supervisor() -> None:
                nonlocal paused
                if WINDOWS:
                    processes.suspend_process(writers.handles["supervisor"])
                else:
                    os.kill(supervisor_pid, signal.SIGSTOP)
                paused = True

            def resume_supervisor() -> None:
                nonlocal paused
                if WINDOWS:
                    processes.resume_process(writers.handles["supervisor"])
                else:
                    os.kill(supervisor_pid, signal.SIGCONT)
                paused = False

            def before_kill() -> None:
                nonlocal git_pid, supervisor_pid, baseline, workspace
                if WINDOWS:
                    writers.wait()
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
                if not alive("git") or not alive("supervisor"):
                    raise RuntimeError(f"materializer disappeared before crash observation: git={alive('git')} supervisor={alive('supervisor')}")
                baseline = inspect_state(args.git, root, 1)
                if not alive("git") or not alive("supervisor"):
                    raise RuntimeError(f"materializer disappeared during independent state inspection: git={alive('git')} supervisor={alive('supervisor')}")
                pause_supervisor()

            def after_kill() -> None:
                if not alive("git") or not alive("supervisor"):
                    raise RuntimeError("materialization course lost its paused writer or supervisor")
                records = list((root / "agent-workspace-ownership-v1").glob("*.jsonl"))
                if len(records) != 1:
                    raise RuntimeError("materialization lacks its exact journal")
                journal = read_journal(records[0])
                original = workspace_snapshot(workspace)
                for _ in range(2):
                    recover()
                    if (not workspace.exists() or workspace_snapshot(workspace) != original
                            or read_journal(records[0]) != journal
                            or inspect_state(args.git, root, 1) != baseline):
                        raise RuntimeError("recovery changed ownership while Git could still write")

            try:
                if stage == "cancel":
                    result = subprocess.run(
                        [str(args.executable), "cancel-materializing-checkout", str(args.canary), str(args.wrapper), str(root)],
                        capture_output=True, text=True, check=False, timeout=60)
                    if result.returncode != 0 or "agent-materialization-cancel:pass" not in result.stdout:
                        raise RuntimeError(f"materialization cancellation failed:\n{result.stdout[-4000:]}\n{result.stderr[-4000:]}")
                    if WINDOWS:
                        writers.wait()
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
                    resume_supervisor()
                deadline = time.monotonic() + 5
                while alive("git") or alive("supervisor"):
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
            except BaseException:
                # A refused capture leaves the wrapper waiting, so the course
                # times out at its checkpoint; name the capture failure instead.
                if WINDOWS and writers.error is not None:
                    raise RuntimeError(f"materializer handle capture failed: {writers.error}")
                raise
            finally:
                if WINDOWS:
                    if paused and "supervisor" in writers.handles:
                        processes.resume_process(writers.handles["supervisor"])
                    if not dead:
                        # Kill the exact fixture writer first; retain the helper
                        # until it has had an opportunity to perform normal reap.
                        if "git" in writers.handles and writers.running("git"):
                            processes.kernel32.TerminateProcess(writers.handles["git"], 1)
                        deadline = time.monotonic() + 5
                        while any(writers.running(name) for name in writers.handles):
                            if time.monotonic() >= deadline:
                                raise RuntimeError("materialization fixture process cleanup failed")
                            time.sleep(0.02)
                    writers.close()
                else:
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
