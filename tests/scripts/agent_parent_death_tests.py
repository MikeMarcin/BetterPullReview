#!/usr/bin/env python3
"""Force-kill the production launcher's owner while a local agent tree runs."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402
from process_kill_support import kill_at_checkpoint, run  # noqa: E402


def running(pid: int) -> bool:
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


def run_course(executable: Path, root: Path, *, kill_process_group: bool) -> None:
    agent = None
    descendant = None
    terminated = False
    try:
        kill_at_checkpoint(executable, "--kill-agent-parent", root,
                           kill_process_group=kill_process_group)
        agent = int((root / "agent.pid").read_text())
        descendant = int((root / "descendant.pid").read_text())
        if agent <= 1 or descendant <= 1 or agent == descendant:
            raise RuntimeError("fixture did not report distinct agent and descendant PIDs")
        marker = root / "heartbeat"
        if not marker.is_file() or marker.stat().st_size == 0:
            raise RuntimeError("descendant never performed work before application death")
        deadline = time.monotonic() + 5
        while running(agent) or running(descendant):
            if time.monotonic() >= deadline:
                raise RuntimeError("agent or descendant survived forced application death")
            time.sleep(0.02)
        terminated = True
        size = marker.stat().st_size
        time.sleep(0.15)
        if marker.stat().st_size != size:
            raise RuntimeError("descendant continued work after reported termination")
        print("agent-parent-death:terminated", flush=True)
        if "agent-parent-recovery:pass" not in run(executable, "--recover-agent-parent", root, timeout=25):
            raise RuntimeError("fresh launcher did not recover")
        print(f"agent-parent-death:{root.name}:pass", flush=True)
    finally:
        if not terminated:
            # These are fixture-reported direct descendants of this run.
            # Cleanup follows the assertion; it cannot make a failed claim pass.
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--supervisor", type=Path, required=True)
    parser.add_argument("--canary", type=Path, required=True)
    args = parser.parse_args()
    with contained_temporary_directory(prefix="bpr-agent-parent-death-") as raw:
        root = Path(raw)
        unauthorized = root / "missing-channel-side-effect"
        refused = subprocess.run(
            [str(args.supervisor), str(args.canary), "--pre-assignment-side-effect", str(unauthorized)],
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            close_fds=True, timeout=5, check=False,
        )
        if refused.returncode != 125 or unauthorized.exists():
            raise RuntimeError("supervisor launched an agent without its owner channel")
        for name, kill_group in (("owner-pid", False), ("owner-group", True)):
            course = root / name
            course.mkdir()
            run_course(args.executable, course, kill_process_group=kill_group)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
