#!/usr/bin/env python3
from __future__ import annotations

import os
import platform
import re
import subprocess
import sys
import time
from pathlib import Path


CANARY = "BPR_CANARY_SECRET_7B1E6D9A"
SOCKET_PATTERN = re.compile(r"socket:\[(\d+)\]")


def child_processes(parent_pid: int) -> list[int]:
    completed = subprocess.run(
        ["ps", "-eo", "pid=,ppid="],
        check=True,
        capture_output=True,
        text=True,
        timeout=5,
    )
    children: list[int] = []
    for line in completed.stdout.splitlines():
        fields = line.split()
        if len(fields) == 2 and int(fields[1]) == parent_pid:
            children.append(int(fields[0]))
    return children


def linux_network_sockets(pid: int) -> list[str]:
    socket_inodes: set[str] = set()
    for descriptor in Path(f"/proc/{pid}/fd").iterdir():
        try:
            target = os.readlink(descriptor)
        except FileNotFoundError:
            continue
        match = SOCKET_PATTERN.fullmatch(target)
        if match:
            socket_inodes.add(match.group(1))

    matches: list[str] = []
    for table in ("tcp", "tcp6", "udp", "udp6"):
        table_path = Path(f"/proc/{pid}/net/{table}")
        if not table_path.exists():
            continue
        for line in table_path.read_text(encoding="utf-8").splitlines()[1:]:
            fields = line.split()
            if len(fields) > 9 and fields[9] in socket_inodes:
                matches.append(f"{table}:{fields[1]}->{fields[2]}:{fields[3]}")
    return matches


def macos_network_sockets(pid: int) -> list[str]:
    completed = subprocess.run(
        ["lsof", "-Pan", "-a", "-p", str(pid), "-i"],
        check=False,
        capture_output=True,
        text=True,
        timeout=5,
    )
    if completed.returncode not in (0, 1):
        raise RuntimeError(f"lsof failed with status {completed.returncode}")
    return completed.stdout.splitlines()[1:]


def network_sockets(pid: int) -> list[str]:
    system = platform.system()
    if system == "Darwin":
        return macos_network_sockets(pid)
    if system == "Linux":
        return linux_network_sockets(pid)
    raise RuntimeError(f"unsupported runtime-audit platform: {system}")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_runtime_surface.py <executable>", file=sys.stderr)
        return 2

    executable = Path(sys.argv[1]).resolve()
    if not executable.is_file():
        print(f"missing executable: {executable}", file=sys.stderr)
        return 2

    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "offscreen"
    environment["BPR_TEST_CANARY"] = CANARY

    process = subprocess.Popen(
        [str(executable)],
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        time.sleep(0.5)
        if process.poll() is not None:
            stdout, stderr = process.communicate(timeout=2)
            print(stdout + stderr, file=sys.stderr)
            print("application exited before runtime inspection", file=sys.stderr)
            return 1

        sockets = network_sockets(process.pid)
        children = child_processes(process.pid)
        if sockets:
            print("unexpected application network sockets:", file=sys.stderr)
            print("\n".join(sockets), file=sys.stderr)
            return 1
        if children:
            print(
                "unexpected application child processes: "
                + ", ".join(str(child) for child in children),
                file=sys.stderr,
            )
            return 1
    finally:
        process.terminate()
        try:
            stdout, stderr = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            stdout, stderr = process.communicate(timeout=5)

    combined_output = stdout + stderr
    if CANARY in combined_output:
        print("canary secret appeared in application output", file=sys.stderr)
        return 1

    print("RUNTIME_SURFACE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
