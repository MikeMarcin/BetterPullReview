#!/usr/bin/env python3
"""Launch this checkout as one named Claude and Codex remote station.

usage: launch_remote_station.py [workstream] [--fresh [codex|claude|both]]

Start or resume automatically, or use --fresh to clear context in enabled
runtimes. Use --no-claude or --no-codex when one service is unavailable.
Claude uses the compact Remote Control interface. --help-all shows setup and
troubleshooting options. Workstreams and station names come from project.json.

The default path holds one writer lease while Codex Managed Remote provisions
and materializes a persistent task named for the station, then resumes the exact
recorded Claude Remote Control session when possible (otherwise creating it).
Unix hosts use the installer-managed Codex daemon. Native Windows leaves Remote
ownership with Codex Desktop and uses a short-lived standalone App Server only
to provision the task. Neither path exposes an App Server network listener.

Run it with `python3` on macOS, Linux and WSL2, and `py -3` in a native Windows
terminal. The station contract lives in
.agents/skills/implement-rfc/references/remote-routing.md.
"""

from __future__ import annotations

import argparse
import contextlib
import json
import pathlib
import platform
import re
import secrets
import subprocess
import sys
import tempfile
import threading
import time
from collections.abc import Callable, Iterator
from dataclasses import dataclass
from typing import Any

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import project_config  # noqa: E402
from codex_station import (  # noqa: E402
    AppServerClient,
    CodexStationError,
    ProxyAppServerClient,
    provision_named_thread,
    release_named_threads,
)

PROJECT = project_config.load()
WORKSTREAMS = PROJECT["workstreams"]


STATION_SCHEMA = "implement-rfc.remote-station.v2"
MINIMUM_CODEX_VERSION = (0, 151, 0)
CLAUDE_RESUME_FAILURE_WINDOW_SECONDS = 30.0
CLAUDE_BRIDGE_ISOLATION_GRACE_SECONDS = 30.0
CLAUDE_SESSION_RECORD_POLL_SECONDS = 0.1
DEFAULT_CODEX_PAIRING_WAIT_SECONDS = 300.0
MAXIMUM_CODEX_PAIRING_WAIT_SECONDS = 600.0
CODEX_REMOTE_CONNECT_WAIT_SECONDS = 10.0
CODEX_REMOTE_POLL_SECONDS = 1.0
CODEX_DESKTOP_PROXY_ATTEMPT_SECONDS = 2.0
MACHINE_TOKEN = re.compile(r"^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$")
CODEX_REMOTE_STATUSES = {"disabled", "connecting", "connected", "errored"}
CLAUDE_BRIDGE_BACKUP_NAME = "bridge-pointer.implement-rfc-remote-station-backup.json"


class LaunchRefusal(Exception):
    """A named reason this checkout may not launch as the requested station."""


@dataclass(frozen=True)
class StationContext:
    root: pathlib.Path
    workstream: str
    branch: str
    name: str
    environment: str
    authority: str
    lease_role: str
    station_file: pathlib.Path


def run_git(root: pathlib.Path | None, *arguments: str) -> str:
    command = ["git"]
    if root is not None:
        command += ["-C", str(root)]
    command += list(arguments)
    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    if completed.returncode != 0:
        raise LaunchRefusal(
            f"git {' '.join(arguments)} failed: {completed.stderr.strip()}"
        )
    return completed.stdout.strip()


def host_platform() -> tuple[str, str]:
    """Return the station platform token and environment for this host."""
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


def is_project_origin(origin: str) -> bool:
    """Match the repository token in SSH and URL-style Git origins."""
    trimmed = origin.rstrip("/")
    if trimmed.endswith(".git"):
        trimmed = trimmed[: -len(".git")]
    tail = trimmed.rsplit("/", maxsplit=1)[-1].rsplit(":", maxsplit=1)[-1]
    return tail == PROJECT["origin_repo_name"]


def load_station_file(station_file: pathlib.Path) -> dict[str, Any]:
    if not station_file.is_file():
        return {}
    try:
        value = json.loads(station_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def durable_station_id(existing: dict[str, Any]) -> str:
    """Preserve station identity across schema migration and relaunch."""
    identity = existing.get("station_id")
    if isinstance(identity, str) and identity:
        return identity
    return f"stn-{secrets.token_hex(4)}"


def expected_codex_thread_id(existing: dict[str, Any]) -> str | None:
    codex = existing.get("codex")
    if not isinstance(codex, dict):
        return None
    thread_id = codex.get("thread_id")
    return thread_id if isinstance(thread_id, str) and thread_id else None


def expected_claude_session_id(existing: dict[str, Any]) -> str | None:
    claude = existing.get("claude")
    if not isinstance(claude, dict):
        return None
    session_id = claude.get("session_id")
    return session_id if isinstance(session_id, str) and session_id else None


def atomic_write_json(path: pathlib.Path, value: dict[str, Any]) -> None:
    """Replace the ignored station registry without leaving a partial file."""
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        dir=path.parent,
        prefix=f".{path.name}.",
        suffix=".tmp",
        delete=False,
    ) as temporary:
        json.dump(value, temporary, indent=2)
        temporary.write("\n")
        temporary_path = pathlib.Path(temporary.name)
    temporary_path.replace(path)


def station_document(
    context: StationContext,
    *,
    station_id: str,
    codex: dict[str, str] | None,
    claude: dict[str, str] | None = None,
) -> dict[str, Any]:
    return {
        "schema": STATION_SCHEMA,
        "station_id": station_id,
        "station": context.name,
        "project": PROJECT["project"],
        "workstream": context.workstream,
        "environment": context.environment,
        "branch": context.branch,
        "authority": context.authority,
        "codex": codex,
        "claude": claude,
    }


def interpreter_spelling() -> str:
    """Return the repository's operator-facing Python spelling for this host."""
    return "py -3" if platform.system() == "Windows" else "python3"


def station_lease_path(context: StationContext) -> pathlib.Path:
    return (
        context.root
        / ".agents"
        / "skills"
        / "implement-rfc"
        / "scripts"
        / "station_lease.py"
    )


def resolve_context(arguments: argparse.Namespace) -> StationContext:
    try:
        root = pathlib.Path(run_git(None, "rev-parse", "--show-toplevel")).resolve()
    except LaunchRefusal as error:
        raise LaunchRefusal("not inside a git checkout") from error

    try:
        origin = run_git(root, "remote", "get-url", "origin")
    except LaunchRefusal:
        origin = ""
    if not is_project_origin(origin):
        raise LaunchRefusal(
            f"this checkout's origin ({origin}) is not the "
            f"{PROJECT['display_name']} repository"
        )

    current = run_git(root, "branch", "--show-current")
    if arguments.workstream is None:
        arguments.workstream = infer_workstream(
            root / ".remote-station.json", current
        )
    workstream = WORKSTREAMS[arguments.workstream]
    branch = workstream["branch"]
    if current != branch:
        raise LaunchRefusal(
            f"checkout is on '{current}' but workstream "
            f"'{arguments.workstream}' requires '{branch}'\n"
            "check out the right worktree for this workstream instead of "
            "switching branches here"
        )

    if arguments.machine and not MACHINE_TOKEN.fullmatch(arguments.machine):
        raise LaunchRefusal(
            "--machine must be a lowercase alphanumeric token with optional hyphens"
        )
    platform_token, environment = host_platform()
    bound_platform = workstream.get("platform")
    if bound_platform is not None and bound_platform != platform_token:
        raise LaunchRefusal(
            f"workstream '{arguments.workstream}' runs only on a {bound_platform} "
            f"host; this is a {environment} host (WSL2 is a Linux station, native "
            "Windows a Windows station)"
        )
    station_file = root / ".remote-station.json"
    canonical = project_config.station_name(
        PROJECT, arguments.workstream, platform_token
    )
    recorded_machine = (
        "" if arguments.fresh_station else infer_machine(station_file, canonical)
    )
    if arguments.machine and recorded_machine and arguments.machine != recorded_machine:
        raise LaunchRefusal(
            f"--machine {arguments.machine} disagrees with this checkout's "
            f"recorded station '{canonical}-{recorded_machine}'; relaunch without "
            "--machine to keep that name, or with --fresh-station to start over "
            "under a different one"
        )
    arguments.machine = arguments.machine or recorded_machine
    name = project_config.station_name(
        PROJECT, arguments.workstream, platform_token, arguments.machine
    )
    lease_role = (
        "project-integrator" if arguments.workstream == "main" else "station-writer"
    )
    return StationContext(
        root=root,
        workstream=arguments.workstream,
        branch=branch,
        name=name,
        environment=environment,
        authority=arguments.authority,
        lease_role=lease_role,
        station_file=station_file,
    )


def infer_workstream(station_file: pathlib.Path, current_branch: str) -> str:
    """The workstream this checkout is, without being told.

    A checkout launched before says so in its station file. A fresh one says
    so by its branch: every workstream in `config/project.json` names the one
    branch it lives on, so the branch identifies the workstream exactly or not
    at all. Nothing is inferred from the host platform — a Windows host can
    hold the integrator checkout, and the branch is what makes that unambiguous.
    """
    try:
        recorded = load_station_file(station_file)
    except LaunchRefusal:
        recorded = {}
    recorded_workstream = recorded.get("workstream")
    if isinstance(recorded_workstream, str) and recorded_workstream in WORKSTREAMS:
        return recorded_workstream
    matches = [
        name
        for name, workstream in WORKSTREAMS.items()
        if workstream["branch"] == current_branch
    ]
    if len(matches) == 1:
        return matches[0]
    branches = ", ".join(
        f"{name} ({workstream['branch']})"
        for name, workstream in sorted(WORKSTREAMS.items())
    )
    raise LaunchRefusal(
        f"cannot infer a workstream: this checkout is on '{current_branch}', "
        f"which no workstream names, and it has no station file; the "
        f"workstreams are {branches}"
    )


def infer_machine(station_file: pathlib.Path, canonical_name: str) -> str:
    """The machine suffix this checkout already launched under, without being told.

    The station file records the exact name the Codex task, the station
    registry, and any id reservation carry. Recomputing a shorter name on
    relaunch does not make the checkout a different station — it keeps the
    recorded task id and then refuses to rename that task — so the suffix is
    read back the same way the workstream is. `--fresh-station` is how an
    operator drops it on purpose.
    """
    recorded = load_station_file(station_file).get("station")
    prefix = f"{canonical_name}-"
    if not isinstance(recorded, str) or not recorded.startswith(prefix):
        return ""
    suffix = recorded[len(prefix):]
    return suffix if MACHINE_TOKEN.fullmatch(suffix) else ""


def codex_cli_version(context: StationContext) -> str:
    try:
        completed = subprocess.run(
            ["codex", "--version"],
            cwd=context.root,
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as error:
        raise LaunchRefusal(f"Codex CLI is unavailable: {error}") from error
    if completed.returncode != 0:
        raise LaunchRefusal(
            f"Codex CLI version probe failed: {completed.stderr.strip()}"
        )
    match = re.search(r"(\d+)\.(\d+)\.(\d+)", completed.stdout)
    if match is None:
        raise LaunchRefusal(
            f"unrecognized Codex CLI version output: {completed.stdout.strip()!r}"
        )
    version_tuple = tuple(int(part) for part in match.groups())
    if version_tuple < MINIMUM_CODEX_VERSION:
        required = ".".join(str(part) for part in MINIMUM_CODEX_VERSION)
        raise LaunchRefusal(
            f"Codex CLI {match.group(0)} is too old; remote stations require {required}+"
        )
    return match.group(0)


def uses_managed_codex_daemon(context: StationContext) -> bool:
    """Native Windows lacks the Unix app-server daemon lifecycle."""
    return context.environment != "windows-native"


def codex_transport_label(context: StationContext) -> str:
    if uses_managed_codex_daemon(context):
        return "managed daemon proxy"
    return "Desktop-owned Remote; stdio task provisioning"


def start_codex_remote_daemon(context: StationContext) -> None:
    """Idempotently start the installer-managed Unix daemon."""
    try:
        completed = subprocess.run(
            ["codex", "remote-control", "start", "--json"],
            cwd=context.root,
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as error:
        raise LaunchRefusal(f"could not start Codex Remote daemon: {error}") from error
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise LaunchRefusal(
            "Codex Remote daemon could not start. Install the current standalone "
            f"Codex CLI for this Unix host and retry.\n{detail}"
        )
    try:
        result = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise LaunchRefusal(
            "Codex Remote daemon returned malformed JSON; upgrade the Codex CLI"
        ) from error
    if not isinstance(result, dict):
        raise LaunchRefusal(
            "Codex Remote daemon returned malformed JSON; upgrade the Codex CLI"
        )
    status = result.get("status")
    if status == "errored":
        raise LaunchRefusal(
            "Codex Remote daemon entered an errored state. Stop the prior host "
            "connection, allow it to drain, and rerun the launcher."
        )
    if status not in {"connecting", "connected"}:
        raise LaunchRefusal(
            "Codex Remote daemon returned an unsupported status; upgrade the Codex CLI"
        )


def start_codex_desktop_app(context: StationContext) -> None:
    """Ask Codex Desktop to open or focus this checkout without owning the app."""
    try:
        completed = subprocess.run(
            ["codex", "app", str(context.root)],
            cwd=context.root,
            capture_output=True,
            text=True,
            check=False,
            timeout=30,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise LaunchRefusal(f"could not launch Codex Desktop: {error}") from error
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise LaunchRefusal(f"Codex Desktop could not launch: {detail}")


def initialize_codex_proxy(context: StationContext) -> ProxyAppServerClient:
    client = ProxyAppServerClient(
        cwd=context.root,
        connect_timeout=CODEX_DESKTOP_PROXY_ATTEMPT_SECONDS,
    )
    try:
        client.__enter__()
    except BaseException:
        client.close()
        raise
    return client


def connect_codex_app_server(context: StationContext) -> ProxyAppServerClient:
    """Connect to the installer-managed App Server on a Unix host."""
    if not uses_managed_codex_daemon(context):
        raise LaunchRefusal(
            "native Windows Remote is owned by Codex Desktop and has no supported "
            "App Server proxy attachment"
        )
    start_codex_remote_daemon(context)
    return initialize_codex_proxy(context)


def codex_pairing_details(message: dict[str, Any]) -> tuple[str, float]:
    code = message.get("manualPairingCode")
    if not isinstance(code, str) or not code.strip():
        raise LaunchRefusal("Codex App Server returned no manual pairing code")

    wait_seconds = DEFAULT_CODEX_PAIRING_WAIT_SECONDS
    expires_at = message.get("expiresAt")
    if isinstance(expires_at, (int, float)) and not isinstance(expires_at, bool):
        wait_seconds = expires_at - time.time()
        if wait_seconds <= 0:
            raise LaunchRefusal("Codex App Server returned an expired pairing code")
    return code.strip(), min(wait_seconds, MAXIMUM_CODEX_PAIRING_WAIT_SECONDS)


def codex_remote_status(result: dict[str, Any], operation: str) -> str:
    status = result.get("status")
    if not isinstance(status, str) or status not in CODEX_REMOTE_STATUSES:
        raise LaunchRefusal(
            f"Codex App Server returned a malformed {operation} response"
        )
    return status


def codex_remote_error(
    client: ProxyAppServerClient,
) -> LaunchRefusal:
    diagnostic = client.diagnostic()
    detail = f"\n{diagnostic}" if diagnostic else ""
    return LaunchRefusal(
        "Codex Remote entered an errored state. Repair this machine's connection "
        "in Codex Desktop or restart the installer-managed daemon, then rerun "
        "the launcher."
        f"{detail}"
    )


def wait_for_codex_remote(
    client: ProxyAppServerClient,
    *,
    timeout: float,
) -> str:
    deadline = time.monotonic() + timeout
    while True:
        result = client.request("remoteControl/status/read", {})
        status = codex_remote_status(result, "remoteControl/status/read")
        if status in {"connected", "errored"} or time.monotonic() >= deadline:
            return status
        time.sleep(
            min(CODEX_REMOTE_POLL_SECONDS, max(0.0, deadline - time.monotonic()))
        )


def ensure_codex_remote(
    client: ProxyAppServerClient,
    *,
    pair: bool,
) -> None:
    operation = "remoteControl/status/read"
    result = client.request(operation, {})
    status = codex_remote_status(result, operation)
    if status == "connected":
        return
    if status == "errored":
        raise codex_remote_error(client)

    status = wait_for_codex_remote(
        client,
        timeout=CODEX_REMOTE_CONNECT_WAIT_SECONDS,
    )
    if status == "connected":
        return
    if status == "errored":
        raise codex_remote_error(client)
    if not pair:
        raise LaunchRefusal(
            "Codex Remote is enabled but not connected. Rerun with --pair-codex "
            "to create a manual pairing code."
        )

    pairing = client.request("remoteControl/pairing/start", {"manualCode": True})
    pairing_code, wait_seconds = codex_pairing_details(pairing)
    wait_minutes = max(1, int((wait_seconds + 59) // 60))
    wait_unit = "minute" if wait_minutes == 1 else "minutes"
    print("\nCodex Remote pairing\n", flush=True)
    print("Copy this code:\n", flush=True)
    print(f"    {pairing_code}\n", flush=True)
    print(
        "On a different controller device, open:\n\n"
        "    Settings > Connections > Control Other devices > Add\n\n"
        "Do not enter this code back into the station host itself.",
        flush=True,
    )
    print(
        f"The code expires in about {wait_minutes} {wait_unit}. Waiting for pairing...",
        flush=True,
    )
    deadline = time.monotonic() + wait_seconds
    while True:
        pairing_status = client.request(
            "remoteControl/pairing/status",
            {
                "pairingCode": pairing.get("pairingCode"),
                "manualPairingCode": pairing.get("manualPairingCode"),
            },
        )
        claimed = pairing_status.get("claimed")
        if not isinstance(claimed, bool):
            raise LaunchRefusal(
                "Codex App Server returned a malformed remoteControl/pairing/status response"
            )
        if claimed:
            break
        if time.monotonic() >= deadline:
            raise LaunchRefusal(
                "Codex Remote pairing was not completed before the code expired; "
                "rerun the station launcher to create a new code"
            )
        time.sleep(CODEX_REMOTE_POLL_SECONDS)

    status = wait_for_codex_remote(
        client,
        timeout=CODEX_REMOTE_CONNECT_WAIT_SECONDS,
    )
    if status == "connected":
        return
    if status == "errored":
        raise codex_remote_error(client)
    raise LaunchRefusal(
        "Codex Remote pairing was claimed, but the host did not become connected"
    )


@contextlib.contextmanager
def provision_codex(
    context: StationContext,
    *,
    existing: dict[str, Any],
    pair: bool,
    fresh: bool = False,
    new_session: bool = False,
) -> Iterator[dict[str, str]]:
    version = codex_cli_version(context)
    expected_thread_id = expected_codex_thread_id(existing)

    def forget_recorded_tasks(client: Any) -> None:
        """Archive the tasks a fresh start must not adopt, and say which."""
        nonlocal expected_thread_id
        if not fresh:
            return
        for outcome in release_named_threads(
            client,
            cwd=context.root,
            name=context.name,
            recorded_thread_id=expected_thread_id,
        ):
            print(f"fresh station: Codex task {outcome}", flush=True)
        expected_thread_id = None

    if not uses_managed_codex_daemon(context):
        if pair:
            raise LaunchRefusal(
                "--pair-codex cannot pair native Windows. In Codex Desktop on "
                "this Windows machine, use Settings > Connections > Control "
                "this PC; then rerun the launcher without --pair-codex."
            )
        start_codex_desktop_app(context)
        try:
            with AppServerClient(cwd=context.root) as client:
                forget_recorded_tasks(client)
                thread_id = provision_named_thread(
                    client,
                    cwd=context.root,
                    name=context.name,
                    expected_thread_id=expected_thread_id,
                    new_session=new_session,
                )
        except CodexStationError as error:
            raise LaunchRefusal(str(error)) from error
        yield {
            "thread_id": thread_id,
            "thread_name": context.name,
            "cli_version": version,
        }
        return

    client: ProxyAppServerClient | None = None
    try:
        client = connect_codex_app_server(context)
        ensure_codex_remote(client, pair=pair)
        forget_recorded_tasks(client)
        thread_id = provision_named_thread(
            client,
            cwd=context.root,
            name=context.name,
            expected_thread_id=expected_thread_id,
            new_session=new_session,
        )
        yield {
            "thread_id": thread_id,
            "thread_name": context.name,
            "cli_version": version,
        }
    except CodexStationError as error:
        raise LaunchRefusal(str(error)) from error
    finally:
        if client is not None:
            client.close()


def claude_command(
    arguments: argparse.Namespace,
    context: StationContext,
    *,
    reattach: bool,
    session_id: str | None = None,
) -> list[str]:
    command = [
        "claude",
        "remote-control",
        "--permission-mode",
        arguments.permission_mode,
    ]
    if reattach:
        if session_id is None:
            raise LaunchRefusal(
                "this station has no recorded Claude session; run normally to create one"
            )
        command += ["--session-id", session_id]
    else:
        command += ["--name", context.name, "--spawn", "session"]
    return command


def claude_bridge_pointer(root: pathlib.Path) -> pathlib.Path:
    encoded_root = re.sub(r"[^A-Za-z0-9-]", "-", str(root.resolve()))
    return pathlib.Path.home() / ".claude" / "projects" / encoded_root / "bridge-pointer.json"


def claude_worktree_bridge_pointers(root: pathlib.Path) -> tuple[pathlib.Path, ...]:
    """Return every Claude bridge pointer that can influence this worktree."""
    if not (root / ".git").exists():
        return (claude_bridge_pointer(root),)
    output = run_git(root, "worktree", "list", "--porcelain")
    roots = [
        pathlib.Path(line.removeprefix("worktree "))
        for line in output.splitlines()
        if line.startswith("worktree ")
    ]
    if root.resolve() not in {candidate.resolve() for candidate in roots}:
        roots.append(root)
    return tuple(dict.fromkeys(claude_bridge_pointer(candidate) for candidate in roots))


@dataclass
class ClaudeBridgeIsolation:
    """Temporarily hide cross-worktree pointers while Claude creates a session."""

    target: pathlib.Path
    backups: tuple[tuple[pathlib.Path, pathlib.Path], ...]

    @classmethod
    def begin(cls, root: pathlib.Path) -> "ClaudeBridgeIsolation":
        target = claude_bridge_pointer(root)
        backups: list[tuple[pathlib.Path, pathlib.Path]] = []
        try:
            for pointer in claude_worktree_bridge_pointers(root):
                backup = pointer.with_name(CLAUDE_BRIDGE_BACKUP_NAME)
                if backup.exists():
                    if pointer.exists():
                        backup.unlink()
                    else:
                        backup.replace(pointer)
                if pointer.exists():
                    pointer.replace(backup)
                    backups.append((pointer, backup))
        except OSError as error:
            isolation = cls(target=target, backups=tuple(backups))
            isolation.close()
            raise LaunchRefusal(
                f"could not isolate Claude bridge pointers for a fresh session: {error}"
            ) from error
        return cls(target=target, backups=tuple(backups))

    def session_started(self) -> None:
        """Keep the new target pointer and make sibling pointers visible again."""
        self._restore(preserve_target=True)

    def release_siblings(self) -> None:
        """Make sibling pointers visible again while the target stays hidden.

        Claude's bridge pointer for a fresh session can appear long after
        launch. Sibling worktrees should not lose their pointers for that
        whole wait, but the old target pointer must stay aside so a stale
        session ID cannot be read as the new one.
        """
        remaining: list[tuple[pathlib.Path, pathlib.Path]] = []
        for pointer, backup in self.backups:
            if pointer == self.target:
                remaining.append((pointer, backup))
                continue
            if not backup.exists():
                continue
            try:
                if pointer.exists():
                    backup.unlink()
                else:
                    backup.replace(pointer)
            except OSError:
                remaining.append((pointer, backup))
        self.backups = tuple(remaining)

    def close(self) -> None:
        self._restore(preserve_target=self.target.exists())

    def _restore(self, *, preserve_target: bool) -> None:
        remaining: list[tuple[pathlib.Path, pathlib.Path]] = []
        for pointer, backup in self.backups:
            if not backup.exists():
                continue
            keep_current = pointer.exists() and (
                pointer != self.target or preserve_target
            )
            try:
                if keep_current:
                    backup.unlink()
                else:
                    backup.replace(pointer)
            except OSError:
                remaining.append((pointer, backup))
        self.backups = tuple(remaining)


def read_claude_bridge_session_id(root: pathlib.Path) -> str | None:
    pointer = claude_bridge_pointer(root)
    try:
        value = json.loads(pointer.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    session_id = value.get("sessionId") if isinstance(value, dict) else None
    if not isinstance(session_id, str) or not re.fullmatch(
        r"session_[A-Za-z0-9]+", session_id
    ):
        return None
    return session_id


def watch_for_claude_session(
    context: StationContext,
    *,
    previous_session_id: str | None,
    record_session: Callable[[str], None],
    stop: threading.Event,
    session_started: Callable[[], None] | None = None,
    grace_elapsed: Callable[[], None] | None = None,
) -> None:
    """Record the station's Claude session whenever it is created.

    Claude writes the bridge pointer on its own schedule, not necessarily at
    startup, so the watch runs until `stop` — the Claude process exiting —
    rather than for a fixed startup window. After
    `CLAUDE_BRIDGE_ISOLATION_GRACE_SECONDS` without a session, `grace_elapsed`
    runs once so bridge isolation can release sibling pointers while the wait
    continues.
    """
    grace_deadline = time.monotonic() + CLAUDE_BRIDGE_ISOLATION_GRACE_SECONDS
    grace_pending = grace_elapsed is not None
    while not stop.is_set():
        session_id = read_claude_bridge_session_id(context.root)
        if session_id is not None and session_id != previous_session_id:
            try:
                record_session(session_id)
            finally:
                if session_started is not None:
                    session_started()
            return
        if grace_pending and time.monotonic() >= grace_deadline:
            grace_pending = False
            grace_elapsed()
        stop.wait(CLAUDE_SESSION_RECORD_POLL_SECONDS)


def run_fresh_claude(
    arguments: argparse.Namespace,
    context: StationContext,
    *,
    record_session: Callable[[str], None] | None,
) -> int:
    isolation = ClaudeBridgeIsolation.begin(context.root)
    stop = threading.Event()
    watcher = threading.Thread(
        target=watch_for_claude_session,
        kwargs={
            "context": context,
            "previous_session_id": None,
            "record_session": record_session or (lambda _: None),
            "session_started": isolation.session_started,
            "grace_elapsed": isolation.release_siblings,
            "stop": stop,
        },
        daemon=True,
    )
    watcher.start()
    try:
        return subprocess.run(
            claude_command(arguments, context, reattach=False),
            cwd=context.root,
            check=False,
        ).returncode
    finally:
        stop.set()
        watcher.join(timeout=1.0)
        isolation.close()


def run_claude(
    arguments: argparse.Namespace,
    context: StationContext,
    *,
    session_id: str | None = None,
    record_session: Callable[[str], None] | None = None,
) -> int:
    """Run Claude, resuming only the session bound to this exact station."""
    if arguments.reattach:
        return subprocess.run(
            claude_command(
                arguments,
                context,
                reattach=True,
                session_id=session_id,
            ),
            cwd=context.root,
            check=False,
        ).returncode
    if arguments.new_claude_session:
        return run_fresh_claude(
            arguments,
            context,
            record_session=record_session,
        )
    if session_id is None:
        return run_fresh_claude(
            arguments,
            context,
            record_session=record_session,
        )

    started_at = time.monotonic()
    resumed = subprocess.run(
        claude_command(
            arguments,
            context,
            reattach=True,
            session_id=session_id,
        ),
        cwd=context.root,
        check=False,
    )
    startup_elapsed = time.monotonic() - started_at
    if resumed.returncode == 0:
        return 0
    if (
        resumed.returncode not in (1, 2)
        or startup_elapsed > CLAUDE_RESUME_FAILURE_WINDOW_SECONDS
    ):
        return resumed.returncode

    print(
        "Claude reattachment was unavailable during startup; starting a fresh session.",
        flush=True,
    )
    return run_fresh_claude(
        arguments,
        context,
        record_session=record_session,
    )


def wait_for_station_stop() -> int:
    """Keep the supervisor's writer lease alive for a Codex-only station."""
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        return 0


def supervise(arguments: argparse.Namespace, context: StationContext) -> int:
    existing = load_station_file(context.station_file)
    remembered = {} if arguments.fresh_station else existing
    station_id = durable_station_id(remembered)
    claude_session_id = expected_claude_session_id(remembered)
    try:
        codex_session = (
            contextlib.nullcontext(existing.get("codex"))
            if arguments.no_codex
            else provision_codex(
                context,
                existing=existing,
                pair=arguments.pair_codex,
                new_session=arguments.new_codex_session,
                fresh=arguments.fresh_station,
            )
        )
        with codex_session as codex:
            atomic_write_json(
                context.station_file,
                station_document(
                    context,
                    station_id=station_id,
                    codex=codex,
                    claude=(
                        {"session_id": claude_session_id}
                        if claude_session_id is not None
                        else None
                    ),
                ),
            )
            if arguments.no_codex:
                print("Codex skipped; saved task binding retained.", flush=True)
            else:
                codex_state = (
                    "station ready"
                    if uses_managed_codex_daemon(context)
                    else "task provisioned"
                )
                print(
                    f"Codex {codex_state}: {context.name} task {codex['thread_id']} "
                    f"(CLI {codex['cli_version']}, {codex_transport_label(context)})",
                    flush=True,
                )
                if context.environment == "windows-native":
                    print(
                        "Codex Desktop sidebar: if this task is under Recents, drag "
                        f"{context.name} onto the {PROJECT['display_name']} project; Desktop exposes no "
                        "supported project-placement RPC.",
                        flush=True,
                    )
            if arguments.no_claude:
                print("Claude skipped. Codex station is running; press Ctrl+C to stop.", flush=True)
                return wait_for_station_stop()
            if arguments.reattach:
                claude_action = "requiring reattachment"
            elif arguments.new_claude_session:
                claude_action = "starting fresh"
            else:
                claude_action = "resuming if available, otherwise starting fresh"
            print(
                f"Claude station {claude_action}: authority {context.authority}, "
                f"lease {context.lease_role}, permissions {arguments.permission_mode}, "
                f"file {context.station_file.name}",
                flush=True,
            )
            def record_claude_session(session_id: str) -> None:
                atomic_write_json(
                    context.station_file,
                    station_document(
                        context,
                        station_id=station_id,
                        codex=codex,
                        claude={"session_id": session_id},
                    ),
                )

            return run_claude(
                arguments,
                context,
                session_id=claude_session_id,
                record_session=record_claude_session,
            )
    except LaunchRefusal as refusal:
        print(f"station refused: {refusal}", file=sys.stderr)
        return 2
    except OSError as error:
        print(
            f"station refused: could not start Claude Remote Control: {error}",
            file=sys.stderr,
        )
        return 2


def internal_supervisor_arguments(arguments: argparse.Namespace) -> list[str]:
    command = [
        sys.executable,
        str(pathlib.Path(__file__).resolve()),
        arguments.workstream,
        "--_supervise",
        "--permission-mode",
        arguments.permission_mode,
    ]
    if arguments.machine:
        command += ["--machine", arguments.machine]
    if arguments.authority == "read-only-proof":
        command.append("--read-only")
    if arguments.reattach:
        command.append("--continue")
    if arguments.new_claude_session:
        command.append("--new-claude-session")
    if arguments.new_codex_session:
        command.append("--new-codex-session")
    if arguments.fresh_station:
        command.append("--fresh-station")
    if arguments.no_claude:
        command.append("--no-claude")
    if arguments.no_codex:
        command.append("--no-codex")
    if arguments.pair_codex:
        command.append("--pair-codex")
    return command


def run_under_lease(arguments: argparse.Namespace, context: StationContext) -> int:
    command = [
        sys.executable,
        str(station_lease_path(context)),
        "run",
        "--role",
        context.lease_role,
        "--station",
        context.name,
        "--",
        *internal_supervisor_arguments(arguments),
    ]
    if arguments.fresh_station:
        verb = "starting over"
    elif arguments.reattach:
        verb = "reattaching"
    elif arguments.new_claude_session:
        verb = "starting fresh"
    else:
        verb = "starting or resuming"
    print(
        f"{verb} unified station {context.name} under {context.lease_role} lease",
        flush=True,
    )
    completed = subprocess.run(command, cwd=context.root, check=False)
    print(
        f"\nstation {context.name} stopped; writer lease released.",
        flush=True,
    )
    return completed.returncode


def run_legacy_no_server(context: StationContext) -> int:
    existing = load_station_file(context.station_file)
    atomic_write_json(
        context.station_file,
        station_document(
            context,
            station_id=durable_station_id(existing),
            codex=None,
        ),
    )
    lease = station_lease_path(context)
    print("warning: --no-server is deprecated; the default launcher enables both runtimes")
    print(
        f"station file written: {context.station_file} "
        f"({context.name}, {context.environment}, {context.authority})"
    )
    print(f"launch the direct Codex session under the writer lease from {context.root}:")
    print(
        f"  {interpreter_spelling()} {lease} run --role {context.lease_role} "
        f"--station {context.name} -- codex"
    )
    return 0


def parse_arguments(argv: list[str] | None) -> argparse.Namespace:
    argv = list(sys.argv[1:] if argv is None else argv)
    show_advanced = "--help-all" in argv
    parser = argparse.ArgumentParser(
        description="Start or resume this checkout's Claude and Codex station.",
        epilog=(
            "No options needed for normal use: the workstream is inferred and both "
            "sessions resume automatically. Stop with Ctrl+C before restarting. "
            "Use --help-all for setup and troubleshooting options."
        ),
    )
    parser.add_argument(
        "workstream", nargs="?", choices=sorted(WORKSTREAMS),
        help="optional override; defaults to the station file or checkout branch",
    )
    parser.add_argument(
        "--fresh", nargs="?", const="both", choices=("codex", "claude", "both"),
        help="clear both contexts, or choose codex/claude; old Codex history is archived",
    )
    parser.add_argument("--help-all", action="help", help="show all setup and troubleshooting options")

    services = parser.add_mutually_exclusive_group()
    services.add_argument("--no-claude", action="store_true", help="run only Codex; keep this terminal open")
    services.add_argument("--no-codex", action="store_true", help="run only Claude; skip all Codex setup")

    advanced = parser.add_argument_group("setup and troubleshooting")

    def advanced_help(text: str) -> str:
        return text if show_advanced else argparse.SUPPRESS

    advanced.add_argument(
        "--machine", default="",
        help=advanced_help("station-name suffix when multiple machines use the same platform"),
    )
    advanced.add_argument(
        "--read-only", action="store_const", dest="authority",
        const="read-only-proof", default="lane-commit",
        help=advanced_help("launch with read-only station authority"),
    )
    claude_session = advanced.add_mutually_exclusive_group()
    claude_session.add_argument(
        "--require-claude-resume", "--continue", "-c",
        action="store_true", dest="reattach",
        help=advanced_help("require the recorded Claude session; fail instead of starting a replacement"),
    )
    claude_session.add_argument(
        "--new-claude-session", action="store_true", help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--new-codex-session", action="store_true", help=argparse.SUPPRESS,
    )
    advanced.add_argument(
        "--pair-codex", action="store_true",
        help=advanced_help("pair Codex Remote on Unix; Windows pairing is in Desktop settings"),
    )
    advanced.add_argument(
        "--no-server", action="store_true",
        help=advanced_help("deprecated: print a direct Codex command without provisioning a station"),
    )
    parser.add_argument("--_supervise", action="store_true", help=argparse.SUPPRESS)
    advanced.add_argument(
        "--permission-mode", default="bypassPermissions",
        choices=("acceptEdits", "auto", "bypassPermissions", "default", "dontAsk", "plan"),
        help=advanced_help("Claude permission mode; default bypassPermissions avoids unattended prompts"),
    )
    advanced.add_argument(
        "--fresh-station", action="store_true",
        help=advanced_help("legacy identity reset: discard bindings and inferred machine suffix"),
    )
    parsed = parser.parse_args(argv)
    if parsed.fresh_station and (parsed.no_codex or parsed.no_claude):
        parser.error("--fresh-station resets both identities; use --fresh with service selection")
    if parsed.no_codex and (parsed.new_codex_session or parsed.fresh == "codex" or parsed.pair_codex):
        parser.error("Codex is disabled; omit Codex reset and pairing options")
    if parsed.no_claude and (parsed.new_claude_session or parsed.fresh == "claude" or parsed.reattach):
        parser.error("Claude is disabled; omit Claude reset and resume options")
    if parsed.no_server and (parsed.no_codex or parsed.no_claude):
        parser.error("service selection requires normal station mode; omit --no-server")
    parsed.new_codex_session |= not parsed.no_codex and parsed.fresh in ("codex", "both")
    parsed.new_claude_session |= not parsed.no_claude and parsed.fresh in ("claude", "both")
    if parsed.reattach and parsed.new_claude_session:
        parser.error("cannot request fresh Claude context and require Claude resumption together")
    if parsed.no_server and (parsed.new_codex_session or parsed.new_claude_session):
        parser.error("fresh sessions require station provisioning; omit --no-server")
    return parsed


def main(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    if arguments.fresh_station and arguments.reattach:
        print(
            "--fresh-station discards the recorded Claude session, which "
            "--continue requires; pass one or the other",
            file=sys.stderr,
        )
        return 2
    # Forgetting the station forgets its session too, so the Claude side takes
    # the fresh path rather than continuing this directory's conversation.
    if arguments.fresh_station:
        arguments.new_claude_session = True
    try:
        context = resolve_context(arguments)
    except LaunchRefusal as refusal:
        print(refusal, file=sys.stderr)
        return 2

    if arguments.no_server:
        if arguments._supervise or arguments.pair_codex:
            print(
                "--no-server cannot be combined with internal supervision or pairing",
                file=sys.stderr,
            )
            return 2
        if arguments.fresh_station:
            # Without an App Server the recorded Codex task cannot be archived,
            # so this path could only pretend to have started the station over.
            print(
                "--no-server cannot start a station over: it reaches no App "
                "Server, so the recorded Codex task would survive the reset",
                file=sys.stderr,
            )
            return 2
        return run_legacy_no_server(context)
    if arguments._supervise:
        return supervise(arguments, context)
    return run_under_lease(arguments, context)


if __name__ == "__main__":
    raise SystemExit(main())
