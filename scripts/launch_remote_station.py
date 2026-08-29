#!/usr/bin/env python3
"""Launch this checkout as a named remote station (or provision it for Codex).

usage: launch_remote_station.py <role> [options]
  role: win | linux | mac | main
  --machine <suffix>   append a machine token to the session name (mandatory
                       when the same role exists on more than one machine)
  --read-only          provision with authority read-only-proof
  --continue           reattach to the last remote-control session here
  --no-server          write .remote-station.json only (Codex-orchestrated
                       use); prints the lease-wrapped launch to run instead

One implementation for every host: run it with `python3` on macOS, Linux, and
WSL2, and `py -3` on native Windows. The station contract this implements
lives in .agents/skills/bpr-implement-rfc/references/remote-routing.md.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import platform
import secrets
import subprocess
import sys

ROLE_BRANCHES = {
    "win": "windows-lane",
    "linux": "linux-lane",
    "mac": "mac-lane",
    "main": "main",
}

STATION_SCHEMA = "betterpullreview.implement-rfc.remote-station.v1"


class LaunchRefusal(Exception):
    """A named reason this checkout may not launch as the requested station."""


def run_git(root: pathlib.Path | None, *arguments: str) -> str:
    command = ["git"]
    if root is not None:
        command += ["-C", str(root)]
    command += list(arguments)
    completed = subprocess.run(
        command, capture_output=True, text=True, check=False
    )
    if completed.returncode != 0:
        raise LaunchRefusal(
            f"git {' '.join(arguments)} failed: {completed.stderr.strip()}"
        )
    return completed.stdout.strip()


def host_platform() -> tuple[str, str]:
    """(role-platform, station environment) for this host, or a refusal."""
    system = platform.system()
    if system == "Darwin":
        return "mac", "macos"
    if system == "Linux":
        try:
            version = pathlib.Path("/proc/version").read_text(
                encoding="utf-8", errors="replace"
            )
        except OSError:
            version = ""
        if "microsoft" in version.lower():
            return "linux", "wsl2"
        return "linux", "linux-native"
    if system == "Windows":
        return "win", "windows-native"
    raise LaunchRefusal(f"unsupported platform: {system}")


def durable_station_id(station_file: pathlib.Path) -> str:
    """Station ids are durable: keep the existing one across relaunches."""
    if station_file.is_file():
        try:
            existing = json.loads(station_file.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            existing = {}
        identity = existing.get("station_id")
        if isinstance(identity, str) and identity:
            return identity
    return f"stn-{secrets.token_hex(4)}"


def interpreter_spelling() -> str:
    """How the operator invokes Python on this host, per repo convention."""
    return "py -3" if platform.system() == "Windows" else "python3"


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__.splitlines()[0],
    )
    parser.add_argument("role", choices=sorted(ROLE_BRANCHES))
    parser.add_argument("--machine", default="")
    parser.add_argument(
        "--read-only",
        action="store_const",
        dest="authority",
        const="read-only-proof",
        default="lane-commit",
    )
    parser.add_argument(
        "--continue",
        "-c",
        action="store_true",
        dest="reattach",
    )
    parser.add_argument("--no-server", action="store_true")
    arguments = parser.parse_args()

    branch = ROLE_BRANCHES[arguments.role]

    try:
        root = pathlib.Path(run_git(None, "rev-parse", "--show-toplevel"))
    except LaunchRefusal:
        print("not inside a git checkout", file=sys.stderr)
        return 2

    try:
        origin = run_git(root, "remote", "get-url", "origin")
    except LaunchRefusal:
        origin = ""
    if "BetterPullReview" not in origin:
        print(
            f"this checkout's origin ({origin}) is not the BetterPullReview "
            "repository",
            file=sys.stderr,
        )
        return 2

    current = run_git(root, "branch", "--show-current")
    if current != branch:
        print(
            f"checkout is on '{current}' but role '{arguments.role}' "
            f"requires '{branch}'\n"
            "check out the right station worktree instead of switching "
            "branches here",
            file=sys.stderr,
        )
        return 2

    try:
        role_platform, environment = host_platform()
    except LaunchRefusal as refusal:
        print(refusal, file=sys.stderr)
        return 2

    if arguments.role not in ("main", role_platform):
        print(
            f"role '{arguments.role}' cannot run on a {environment} host "
            "(WSL2 is a Linux station, native Windows a Windows station)",
            file=sys.stderr,
        )
        return 2

    suffix = f"-{arguments.machine}" if arguments.machine else ""
    if arguments.role == "main":
        name = f"bpr-main-{role_platform}{suffix}"
    else:
        name = f"bpr-{arguments.role}{suffix}"

    station_file = root / ".remote-station.json"
    station = {
        "schema": STATION_SCHEMA,
        "station_id": durable_station_id(station_file),
        "station": name,
        "project": "betterpullreview",
        "role": arguments.role,
        "environment": environment,
        "branch": branch,
        "authority": arguments.authority,
    }
    station_file.write_text(
        json.dumps(station, indent=2) + "\n", encoding="utf-8"
    )

    lease = (
        root
        / ".agents"
        / "skills"
        / "bpr-implement-rfc"
        / "scripts"
        / "station_lease.py"
    )
    lease_role = (
        "project-integrator" if arguments.role == "main" else "station-writer"
    )

    if arguments.no_server:
        print(
            f"station file written: {station_file} "
            f"({name}, {environment}, {arguments.authority})"
        )
        print(f"launch the Codex session under the writer lease from {root}:")
        print(
            f"  {interpreter_spelling()} {lease} run --role {lease_role} "
            f"--station {name} -- codex"
        )
        return 0

    command = [
        sys.executable,
        str(lease),
        "run",
        "--role",
        lease_role,
        "--station",
        name,
        "--",
        "claude",
        "remote-control",
        "--name",
        name,
        "--spawn",
        "session",
    ]
    if arguments.reattach:
        command.append("--continue")
    # The lease lives exactly as long as the wrapped process; waiting here
    # instead of exec-ing keeps one behavior on every platform, Windows
    # included, where exec semantics differ.
    return subprocess.run(command, cwd=root, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
