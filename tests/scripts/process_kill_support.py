"""Bounded child execution and forced death at a fixture-owned checkpoint."""

from __future__ import annotations

import os
from pathlib import Path
import signal
import subprocess
import time


def run(executable: Path, phase: str, root: Path, *, timeout: float = 15) -> str:
    completed = subprocess.run(
        [str(executable), phase, str(root)],
        check=False,
        capture_output=True,
        text=True,
        timeout=timeout,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{phase} failed ({completed.returncode}):\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed.stdout


def kill_at_checkpoint(
    executable: Path, phase: str, root: Path, *, kill_process_group: bool = False
) -> None:
    with subprocess.Popen(
        [str(executable), phase, str(root)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=kill_process_group,
    ) as process:
        try:
            deadline = time.monotonic() + 15
            marker = root / "kill-ready"
            while not marker.exists() or marker.read_text() != phase:
                if process.poll() is not None:
                    stdout, stderr = process.communicate(timeout=5)
                    raise RuntimeError(
                        f"{phase} exited before checkpoint ({process.returncode}):\n"
                        f"{stdout}\n{stderr}"
                    )
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"{phase} did not reach checkpoint")
                time.sleep(0.01)
            if process.poll() is not None:
                raise RuntimeError(f"{phase} exited instead of waiting to be killed")
            # kill() uses SIGKILL on POSIX and TerminateProcess on Windows;
            # stdin remains open until after death, so the C++ scope cannot exit.
            if kill_process_group:
                if os.getpgid(process.pid) != process.pid:
                    raise RuntimeError("fixture lost its isolated process group")
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
            process.wait(timeout=5)
            stdout, stderr = process.communicate(timeout=5)
            expected = -signal.SIGKILL if os.name == "posix" else 1
            if process.returncode != expected:
                raise RuntimeError(
                    f"{phase} was not forcibly killed ({process.returncode}):\n"
                    f"{stdout}\n{stderr}"
                )
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


