#!/usr/bin/env python3
"""Codex hook guard for a checkout bound to a remote station.

Project facts come from the guarded checkout's own
.agents/skills/implement-rfc/config/project.json.
"""

from __future__ import annotations

import datetime as dt
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
from typing import Any

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import project_config  # noqa: E402

STATION_SCHEMA = "implement-rfc.remote-station.v2"
TASK_ENVELOPE = "implement-rfc.remote-task.v1"
SESSION_NAME = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)+$")
SKILL_RELATIVE = pathlib.Path(".agents/skills/implement-rfc")
TASK_ID = re.compile(r"^task-[0-9a-z-]{4,64}$")
SHA40 = re.compile(r"^[0-9a-f]{40}$")
FENCED_BLOCK = re.compile(r"```(?:yaml|yml|json)?\s*\n(.*?)\n```", re.DOTALL)
YAML_KEY = re.compile(r"^([a-z_]+):(?:\s*(.*))?$")
REQUIRED_TASK_FIELDS = {
    "envelope",
    "task_id",
    "project",
    "station",
    "workstream",
    "branch",
    "rfcs",
    "cards",
    "rfc_base_sha",
    "base_commit",
    "authority",
    "id_reservations",
    "reply_to",
    "done_when",
}
READ_ONLY_DENIED_COMMANDS = (
    re.compile(
        r"(^|[;&|]\s*)git\s+(?:add|am|apply|cherry-pick|clean|commit|merge|mv|"
        r"push|rebase|reset|restore|revert|rm|switch|tag)(?:\s|$)"
    ),
    re.compile(r"(^|\s)(?:sed\s+-i|perl\s+-pi|tee)(?:\s|$)"),
    re.compile(r"(?:^|[^<>])(?:>>?|<<?)\s*[^&]"),
)


class GuardRefusal(Exception):
    """A station guard refusal with a stable protocol reason."""

    def __init__(self, reason: str, detail: str, correct_station: str | None = None):
        super().__init__(detail)
        self.reason = reason
        self.detail = detail
        self.correct_station = correct_station


def _git(root: pathlib.Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(root), *arguments],
        capture_output=True,
        text=True,
        check=False,
    )


def _repo_root(cwd: str) -> pathlib.Path | None:
    completed = _git(pathlib.Path(cwd), "rev-parse", "--show-toplevel")
    if completed.returncode != 0:
        return None
    return pathlib.Path(completed.stdout.strip()).resolve()


def _load_station(root: pathlib.Path) -> dict[str, Any] | None:
    path = root / ".remote-station.json"
    if not path.is_file():
        return None
    try:
        station = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise GuardRefusal("no-station", f"station registry is unreadable: {error}") from error
    if not isinstance(station, dict):
        raise GuardRefusal("no-station", "station registry is not a JSON object")
    if station.get("schema") != STATION_SCHEMA:
        # A station file written by an older launcher carries no Codex task
        # binding, so there is nothing to enforce against. Blocking here would
        # refuse every Codex turn in the checkout, including delegated lanes,
        # until an operator relaunches; stay inactive and say so instead.
        print(
            f"remote-station guard inactive: {path} is {station.get('schema')!r}, "
            f"not {STATION_SCHEMA}; relaunch the station to enforce turns",
            file=sys.stderr,
        )
        return None
    return station


def _codex_thread_id(station: dict[str, Any]) -> str:
    codex = station.get("codex")
    thread_id = codex.get("thread_id") if isinstance(codex, dict) else None
    if not isinstance(thread_id, str) or not thread_id:
        raise GuardRefusal("no-station", "station has no provisioned Codex task")
    return thread_id


def _is_project_origin(origin: str, expected: str) -> bool:
    trimmed = origin.rstrip("/")
    if trimmed.endswith(".git"):
        trimmed = trimmed[:-4]
    return trimmed.rsplit("/", 1)[-1].rsplit(":", 1)[-1] == expected


def _load_project(root: pathlib.Path) -> dict[str, Any]:
    try:
        return project_config.load(root / SKILL_RELATIVE / "config" / "project.json")
    except project_config.ProjectConfigError as error:
        raise GuardRefusal("no-station", f"project configuration is unusable: {error}") from error


def _validate_live_checkout(
    root: pathlib.Path, station: dict[str, Any], project: dict[str, Any]
) -> None:
    if station.get("project") != project["project"]:
        raise GuardRefusal(
            "project-mismatch",
            f"station file names project {station.get('project')!r}, checkout is {project['project']!r}",
        )
    origin = _git(root, "remote", "get-url", "origin")
    if origin.returncode != 0 or not _is_project_origin(
        origin.stdout.strip(), project["origin_repo_name"]
    ):
        raise GuardRefusal(
            "project-mismatch",
            f"live checkout is not the {project['display_name']} repository",
        )
    branch = _git(root, "branch", "--show-current")
    live_branch = branch.stdout.strip() if branch.returncode == 0 else ""
    if live_branch != station.get("branch"):
        raise GuardRefusal(
            "branch-mismatch",
            f"live branch {live_branch!r} does not match {station.get('branch')!r}",
        )


def _lease_record(root: pathlib.Path, station: dict[str, Any]) -> dict[str, Any]:
    role = "project-integrator" if station.get("workstream") == "main" else "station-writer"
    lease = root / SKILL_RELATIVE / "scripts" / "station_lease.py"
    completed = subprocess.run(
        [
            sys.executable,
            str(lease),
            "status",
            "--role",
            role,
            "--repo",
            str(root),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if completed.returncode != 10:
        raise GuardRefusal("authority-exceeded", f"{role} supervisor lease is not held")
    _, separator, record_text = completed.stdout.partition("\n")
    if not separator:
        raise GuardRefusal("authority-exceeded", f"{role} lease has no holder record")
    try:
        record = json.loads(record_text)
    except json.JSONDecodeError as error:
        raise GuardRefusal("authority-exceeded", f"{role} holder record is invalid") from error
    if record.get("station") != station.get("station"):
        raise GuardRefusal(
            "authority-exceeded",
            f"{role} lease belongs to {record.get('station')!r}",
        )
    return record


def _yaml_scalars(block: str) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for line in block.splitlines():
        match = YAML_KEY.fullmatch(line)
        if match is None:
            continue
        key, raw_value = match.groups()
        if key in result:
            raise GuardRefusal("project-mismatch", f"remote-task repeats field {key}")
        value = (raw_value or "").strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        result[key] = value
    return result


def _extract_envelope(prompt: str) -> dict[str, Any]:
    candidates: list[dict[str, Any]] = []
    for block in FENCED_BLOCK.findall(prompt):
        stripped = block.lstrip()
        if stripped.startswith("{"):
            try:
                parsed = json.loads(block)
            except json.JSONDecodeError:
                continue
            if isinstance(parsed, dict) and parsed.get("envelope") == TASK_ENVELOPE:
                candidates.append(parsed)
        else:
            parsed = _yaml_scalars(block)
            if parsed.get("envelope") == TASK_ENVELOPE:
                candidates.append(parsed)
    if len(candidates) != 1:
        raise GuardRefusal(
            "project-mismatch",
            "prompt must contain exactly one fenced remote-task v1 envelope",
        )
    envelope = candidates[0]
    missing = sorted(REQUIRED_TASK_FIELDS - envelope.keys())
    if missing:
        raise GuardRefusal(
            "project-mismatch",
            f"remote-task is missing required fields: {', '.join(missing)}",
        )
    if not TASK_ID.fullmatch(str(envelope.get("task_id", ""))):
        raise GuardRefusal("project-mismatch", "remote-task task_id is invalid")
    if not SHA40.fullmatch(str(envelope.get("rfc_base_sha", ""))):
        raise GuardRefusal("project-mismatch", "remote-task rfc_base_sha is invalid")
    if not SHA40.fullmatch(str(envelope.get("base_commit", ""))):
        raise GuardRefusal("base-not-ancestor", "remote-task base_commit is invalid")
    if not SESSION_NAME.fullmatch(str(envelope.get("reply_to", ""))):
        raise GuardRefusal("project-mismatch", "remote-task reply_to is invalid")
    return envelope


def _parse_expiry(value: object) -> dt.datetime | None:
    if value in (None, ""):
        return None
    if not isinstance(value, str):
        raise GuardRefusal("expired", "remote-task expires_at is invalid")
    try:
        parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise GuardRefusal("expired", "remote-task expires_at is invalid") from error
    if parsed.tzinfo is None:
        raise GuardRefusal("expired", "remote-task expires_at must include a timezone")
    return parsed


def _validate_envelope(
    root: pathlib.Path,
    station: dict[str, Any],
    envelope: dict[str, Any],
) -> None:
    comparisons = (
        ("project", "project-mismatch"),
        ("station", "station-mismatch"),
        ("workstream", "workstream-mismatch"),
        ("branch", "branch-mismatch"),
    )
    for field, reason in comparisons:
        if envelope.get(field) != station.get(field):
            correct = str(station.get("station")) if field == "station" else None
            raise GuardRefusal(
                reason,
                f"remote-task {field} {envelope.get(field)!r} does not match "
                f"{station.get(field)!r}",
                correct,
            )
    authority = envelope.get("authority")
    if authority not in ("lane-commit", "read-only-proof"):
        raise GuardRefusal("authority-exceeded", "remote-task authority is invalid")
    if station.get("authority") == "read-only-proof" and authority == "lane-commit":
        raise GuardRefusal(
            "authority-exceeded",
            "remote-task lane-commit authority exceeds read-only station authority",
        )
    ancestry = _git(
        root,
        "merge-base",
        "--is-ancestor",
        str(envelope["base_commit"]),
        "HEAD",
    )
    if ancestry.returncode != 0:
        raise GuardRefusal(
            "base-not-ancestor",
            f"remote-task base {envelope['base_commit']} is not an ancestor of HEAD",
        )
    expiry = _parse_expiry(envelope.get("expires_at"))
    if expiry is not None and expiry <= dt.datetime.now(dt.timezone.utc):
        raise GuardRefusal("expired", f"remote-task expired at {envelope['expires_at']}")


def _state_path(root: pathlib.Path, session_id: str, turn_id: str) -> pathlib.Path:
    git_dir = _git(root, "rev-parse", "--git-dir")
    if git_dir.returncode != 0:
        raise GuardRefusal("no-station", "cannot resolve station git directory")
    path = pathlib.Path(git_dir.stdout.strip())
    if not path.is_absolute():
        path = (root / path).resolve()
    key = hashlib.sha256(f"{session_id}\0{turn_id}".encode()).hexdigest()
    return path / "implement-rfc-remote-station-turns" / f"{key}.json"


def _write_turn_state(
    root: pathlib.Path,
    event: dict[str, Any],
    envelope: dict[str, Any],
) -> None:
    session_id = str(event.get("session_id", ""))
    turn_id = str(event.get("turn_id", ""))
    if not session_id or not turn_id:
        raise GuardRefusal("authority-exceeded", "hook input has no turn identity")
    path = _state_path(root, session_id, turn_id)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(f".{os.getpid()}.tmp")
    temporary.write_text(
        json.dumps(
            {
                "schema": "implement-rfc.remote-turn.v1",
                "session_id": session_id,
                "turn_id": turn_id,
                "task_id": envelope["task_id"],
                "authority": envelope["authority"],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def _read_turn_state(root: pathlib.Path, event: dict[str, Any]) -> dict[str, Any]:
    path = _state_path(
        root,
        str(event.get("session_id", "")),
        str(event.get("turn_id", "")),
    )
    try:
        state = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise GuardRefusal(
            "authority-exceeded",
            "this tool call has no accepted remote-task turn",
        ) from error
    if (
        state.get("session_id") != event.get("session_id")
        or state.get("turn_id") != event.get("turn_id")
    ):
        raise GuardRefusal("authority-exceeded", "remote-task turn identity changed")
    return state


def _clear_turn_state(root: pathlib.Path, event: dict[str, Any]) -> None:
    try:
        _state_path(
            root,
            str(event.get("session_id", "")),
            str(event.get("turn_id", "")),
        ).unlink(missing_ok=True)
    except (GuardRefusal, OSError):
        pass


def _hook_context(event_name: str, text: str) -> None:
    print(
        json.dumps(
            {
                "hookSpecificOutput": {
                    "hookEventName": event_name,
                    "additionalContext": text,
                }
            }
        )
    )


def _block(reason: str) -> None:
    print(json.dumps({"decision": "block", "reason": reason}))


def _deny_tool(reason: str) -> None:
    print(
        json.dumps(
            {
                "hookSpecificOutput": {
                    "hookEventName": "PreToolUse",
                    "permissionDecision": "deny",
                    "permissionDecisionReason": reason,
                }
            }
        )
    )


def _refusal_text(
    station: dict[str, Any],
    envelope: dict[str, Any] | None,
    refusal: GuardRefusal,
) -> str:
    task_id = "task-invalid"
    if envelope is not None and isinstance(envelope.get("task_id"), str):
        task_id = str(envelope["task_id"])
    lines = [
        "The station guard refused this task. Do not call tools. "
        "Respond with exactly this envelope:",
        "```yaml",
        "envelope: implement-rfc.remote-refusal.v1",
        f"task_id: {task_id}",
        f"station: {station.get('station', 'unknown-station')}",
        f"reason: {refusal.reason}",
        f"detail: {refusal.detail}",
    ]
    if refusal.correct_station:
        lines.append(f"correct_station: {refusal.correct_station}")
    lines.append("```")
    return "\n".join(lines)


def _handle_prompt(
    root: pathlib.Path,
    station: dict[str, Any],
    project: dict[str, Any],
    event: dict[str, Any],
) -> None:
    expected_thread = _codex_thread_id(station)
    if event.get("session_id") != expected_thread:
        _block(
            f"checkout is bound to Codex task {expected_thread}; "
            f"current task is {event.get('session_id')}"
        )
        return

    envelope: dict[str, Any] | None = None
    try:
        _validate_live_checkout(root, station, project)
        _lease_record(root, station)
        envelope = _extract_envelope(str(event.get("prompt", "")))
        _validate_envelope(root, station, envelope)
        _write_turn_state(root, event, envelope)
    except GuardRefusal as refusal:
        _clear_turn_state(root, event)
        _hook_context("UserPromptSubmit", _refusal_text(station, envelope, refusal))
        return

    _hook_context(
        "UserPromptSubmit",
        "Remote-task envelope accepted for this station and turn. "
        "Obey its narrowed authority. Finish with one fenced "
        "implement-rfc.remote-reply.v1 envelope, or the documented "
        "remote-refusal envelope if later identity validation fails.",
    )


def _read_only_command_denied(command: str) -> bool:
    return any(pattern.search(command) for pattern in READ_ONLY_DENIED_COMMANDS)


def _handle_tool(
    root: pathlib.Path,
    station: dict[str, Any],
    project: dict[str, Any],
    event: dict[str, Any],
) -> None:
    try:
        if event.get("session_id") != _codex_thread_id(station):
            raise GuardRefusal(
                "authority-exceeded",
                "tool call came from the wrong Codex task",
            )
        _validate_live_checkout(root, station, project)
        _lease_record(root, station)
        state = _read_turn_state(root, event)
        if state.get("authority") == "lane-commit":
            return

        tool_name = str(event.get("tool_name", ""))
        if tool_name in {"apply_patch", "Edit", "Write"}:
            raise GuardRefusal(
                "authority-exceeded",
                "read-only-proof authority cannot edit repository files",
            )
        if tool_name == "Bash":
            tool_input = event.get("tool_input")
            command = ""
            if isinstance(tool_input, dict):
                command = str(tool_input.get("command", tool_input.get("cmd", "")))
            if _read_only_command_denied(command):
                raise GuardRefusal(
                    "authority-exceeded",
                    "read-only-proof authority denied a mutating shell command",
                )
    except GuardRefusal as refusal:
        _deny_tool(refusal.detail)


def main() -> int:
    try:
        event = json.load(sys.stdin)
    except (json.JSONDecodeError, OSError):
        _block("station guard received invalid hook input")
        return 0
    if not isinstance(event, dict):
        _block("station guard received non-object hook input")
        return 0
    root = _repo_root(str(event.get("cwd", pathlib.Path.cwd())))
    if root is None:
        return 0
    try:
        station = _load_station(root)
    except GuardRefusal as refusal:
        _block(refusal.detail)
        return 0
    if station is None:
        return 0
    if station.get("codex") is None:
        # Deprecated direct-Codex mode has no thread binding. Its lease wrapper
        # and the portable skill guard remain authoritative.
        return 0

    try:
        project = _load_project(root)
    except GuardRefusal as refusal:
        _block(refusal.detail)
        return 0

    event_name = event.get("hook_event_name")
    if event_name == "UserPromptSubmit":
        _handle_prompt(root, station, project, event)
    elif event_name == "PreToolUse":
        _handle_tool(root, station, project, event)
    elif event_name == "Stop":
        _clear_turn_state(root, event)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
