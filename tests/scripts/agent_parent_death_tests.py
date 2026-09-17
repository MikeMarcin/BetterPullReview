#!/usr/bin/env python3
"""Force-kill the production launcher's owner while a local agent tree runs."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import time

if os.name == "posix":
    import fcntl
    import signal
else:
    import ctypes
    from ctypes import wintypes

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from process_kill_support import kill_at_checkpoint, run  # noqa: E402


def running(pid: int) -> bool:
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


if os.name != "posix":
    # [[BPR-0622]]. Windows equivalents, test code only: an exclusive LockFileEx
    # on the fixed guard range replaces flock, NtSuspendProcess/NtResumeProcess
    # replace SIGSTOP/SIGCONT, and liveness is read from process handles opened
    # while the processes were alive, never from PID existence.
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    ntdll = ctypes.WinDLL("ntdll")
    GUARD_LOCK_OFFSET = 0x4000_0000_0000_0000
    GENERIC_READ = 0x80000000
    FILE_SHARE_READ = 0x1
    FILE_SHARE_WRITE = 0x2
    OPEN_EXISTING = 3
    FILE_FLAG_OPEN_REPARSE_POINT = 0x00200000
    LOCKFILE_FAIL_IMMEDIATELY = 0x1
    LOCKFILE_EXCLUSIVE_LOCK = 0x2
    SYNCHRONIZE = 0x00100000
    PROCESS_SUSPEND_RESUME = 0x0800
    PROCESS_TERMINATE = 0x0001
    WAIT_OBJECT_0 = 0
    INVALID_HANDLE_VALUE = wintypes.HANDLE(-1).value

    class OVERLAPPED(ctypes.Structure):
        _fields_ = [("Internal", ctypes.c_void_p), ("InternalHigh", ctypes.c_void_p),
                    ("Offset", wintypes.DWORD), ("OffsetHigh", wintypes.DWORD), ("hEvent", wintypes.HANDLE)]

    kernel32.CreateFileW.restype = wintypes.HANDLE
    kernel32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                     wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel32.LockFileEx.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
                                    wintypes.DWORD, ctypes.POINTER(OVERLAPPED)]
    kernel32.UnlockFileEx.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
                                      ctypes.POINTER(OVERLAPPED)]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    ntdll.NtSuspendProcess.argtypes = [wintypes.HANDLE]
    ntdll.NtResumeProcess.argtypes = [wintypes.HANDLE]

    def windows_guard_available(path: Path) -> bool:
        handle = kernel32.CreateFileW(str(path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, None,
                                      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, None)
        if handle in (None, INVALID_HANDLE_VALUE):
            raise RuntimeError(f"cannot open workspace guard ({ctypes.get_last_error()})")
        try:
            overlapped = OVERLAPPED()
            overlapped.Offset = GUARD_LOCK_OFFSET & 0xFFFFFFFF
            overlapped.OffsetHigh = GUARD_LOCK_OFFSET >> 32
            if not kernel32.LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0,
                                       ctypes.byref(overlapped)):
                return False
            kernel32.UnlockFileEx(handle, 0, 1, 0, ctypes.byref(overlapped))
            return True
        finally:
            kernel32.CloseHandle(handle)

    def open_process(pid: int, access: int) -> int:
        handle = kernel32.OpenProcess(access, False, pid)
        if not handle:
            raise RuntimeError(f"cannot open fixture process {pid} ({ctypes.get_last_error()})")
        return handle

    def exited(handle: int) -> bool:
        return kernel32.WaitForSingleObject(handle, 0) == WAIT_OBJECT_0


def run_course(executable: Path, root: Path, *, kill_process_group: bool, pause: bool = True) -> None:
    agent = None
    descendant = None
    terminated = False
    paused_supervisor = None
    windows_handles: dict[str, int] = {}

    def guard_available() -> bool:
        if os.name != "posix":
            return windows_guard_available(root / "workspace.guard")
        with (root / "workspace.guard").open("rb") as guard:
            try:
                fcntl.flock(guard, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return True
            except BlockingIOError:
                return False

    def capture_windows_handles() -> None:
        windows_handles["agent"] = open_process(int((root / "agent.pid").read_text()), SYNCHRONIZE | PROCESS_TERMINATE)
        windows_handles["descendant"] = open_process(int((root / "descendant.pid").read_text()), SYNCHRONIZE | PROCESS_TERMINATE)

    def pause_supervisor() -> None:
        nonlocal paused_supervisor
        if os.name != "posix":
            capture_windows_handles()
        if not pause:
            if guard_available():
                raise RuntimeError("workspace guard was not held during agent execution")
            return
        paused_supervisor = int((root / "supervisor.pid").read_text())
        if paused_supervisor <= 1:
            raise RuntimeError("invalid fixture supervisor PID")
        if os.name == "posix":
            os.kill(paused_supervisor, signal.SIGSTOP)
        else:
            windows_handles["supervisor"] = open_process(paused_supervisor, PROCESS_SUSPEND_RESUME | SYNCHRONIZE)
            if ntdll.NtSuspendProcess(windows_handles["supervisor"]) != 0:
                raise RuntimeError("could not suspend the fixture supervisor")
        if guard_available():
            raise RuntimeError("workspace guard was not held during agent execution")

    def resume_supervisor() -> None:
        nonlocal paused_supervisor
        if paused_supervisor is not None:
            if os.name == "posix":
                try:
                    os.kill(paused_supervisor, signal.SIGCONT)
                except ProcessLookupError:
                    pass
            else:
                ntdll.NtResumeProcess(windows_handles["supervisor"])
            paused_supervisor = None

    def observe_dead_owner() -> None:
        nonlocal paused_supervisor
        try:
            if paused_supervisor is not None and guard_available():
                raise RuntimeError("application death released the live supervisor's workspace guard")
            if os.name != "posix" and paused_supervisor is not None and exited(windows_handles["agent"]):
                raise RuntimeError("the paused supervisor's job ended before the supervisor could act")
        finally:
            resume_supervisor()

    def agent_or_descendant_running() -> bool:
        if os.name == "posix":
            return running(agent) or running(descendant)
        return not exited(windows_handles["agent"]) or not exited(windows_handles["descendant"])

    try:
        kill_at_checkpoint(executable, "--kill-agent-parent", root,
                           kill_process_group=kill_process_group, before_kill=pause_supervisor,
                           after_kill=observe_dead_owner)
        agent = int((root / "agent.pid").read_text())
        descendant = int((root / "descendant.pid").read_text())
        if agent <= 1 or descendant <= 1 or agent == descendant:
            raise RuntimeError("fixture did not report distinct agent and descendant PIDs")
        marker = root / "heartbeat"
        if not marker.is_file() or marker.stat().st_size == 0:
            raise RuntimeError("descendant never performed work before application death")
        deadline = time.monotonic() + 5
        while agent_or_descendant_running():
            if time.monotonic() >= deadline:
                raise RuntimeError("agent or descendant survived forced application death")
            time.sleep(0.02)
        terminated = True
        # The agent's PID disappears when the supervisor reaps it; the guard is
        # released afterwards, when the supervisor's own exit closes the last
        # descriptor holding the shared lock. On macOS that lands about a
        # millisecond later (BPR-0666), so the release gets a bounded wait too.
        deadline = time.monotonic() + 5
        while not guard_available():
            if time.monotonic() >= deadline:
                raise RuntimeError("finished supervisor retained its workspace guard")
            time.sleep(0.02)
        size = marker.stat().st_size
        time.sleep(0.15)
        if marker.stat().st_size != size:
            raise RuntimeError("descendant continued work after reported termination")
        print("agent-parent-death:terminated", flush=True)
        if "agent-parent-recovery:pass" not in run(executable, "--recover-agent-parent", root, timeout=25):
            raise RuntimeError("fresh launcher did not recover")
        print(f"agent-parent-death:{root.name}:pass", flush=True)
    finally:
        resume_supervisor()
        if not terminated:
            # These are fixture-reported direct descendants of this run.
            # Cleanup follows the assertion; it cannot make a failed claim pass.
            if os.name != "posix":
                for name in ("agent", "descendant"):
                    if name in windows_handles and not exited(windows_handles[name]):
                        kernel32.TerminateProcess(windows_handles[name], 1)
            else:
                if agent is None and (root / "agent.pid").exists():
                    agent = int((root / "agent.pid").read_text())
                if descendant is None and (root / "descendant.pid").exists():
                    descendant = int((root / "descendant.pid").read_text())
                for pid in (agent, descendant):
                    if pid is not None and pid > 1:
                        try:
                            os.kill(pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                deadline = time.monotonic() + 5
                while any(pid is not None and pid > 1 and running(pid) for pid in (agent, descendant)):
                    if time.monotonic() >= deadline:
                        raise RuntimeError("fixture process cleanup did not finish")
                    time.sleep(0.02)
        if os.name != "posix":
            for handle in windows_handles.values():
                kernel32.CloseHandle(handle)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--supervisor", type=Path, required=True)
    parser.add_argument("--canary", type=Path, required=True)
    args = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-agent-parent-death-") as raw:
        root = Path(raw)
        if "agent-workspace-guard:pass" not in run(args.executable, "--workspace-guard", root, timeout=60):
            raise RuntimeError("workspace guard refusal/descriptor course failed")
        unauthorized = root / "missing-channel-side-effect"
        refused = subprocess.run(
            [str(args.supervisor), str(args.canary), "--pre-assignment-side-effect", str(unauthorized)],
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            close_fds=True, timeout=5, check=False,
        )
        if refused.returncode != 125 or unauthorized.exists():
            raise RuntimeError("supervisor launched an agent without its owner channel")
        if os.name == "posix":
            courses = (("owner-pid", False, True), ("owner-group", True, True))
        else:
            # Windows has no process groups. The second course kills the owner
            # while its supervisor keeps running, so teardown starts at once.
            courses = (("owner-pid", False, True), ("owner-live-supervisor", False, False))
        for name, kill_group, pause in courses:
            course = root / name
            course.mkdir()
            run_course(args.executable, course, kill_process_group=kill_group, pause=pause)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
