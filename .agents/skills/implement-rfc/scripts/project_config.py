#!/usr/bin/env python3
"""Load and validate `config/project.json`, the portable skill's only machine-read
project-specific file.

Every other script in this directory imports this module instead of naming a
project, a tracker path, a lane, or a session prefix. The checks here mirror
`schemas/project.schema.json` so the skill stays runnable on a bare Python
without a JSON Schema library.
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

SKILL_DIR = Path(__file__).resolve().parents[1]
PROJECT_PATH = SKILL_DIR / "config" / "project.json"
SCHEMA = "implement-rfc.project.v1"
TOKEN_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
PLATFORM_TOKENS = ("win", "linux", "mac")
REQUIRED_KEYS = (
    "schema", "project", "display_name", "session_prefix", "origin_repo_name",
    "rfc_root", "issue_root", "next_id", "reservations_dir", "allocate_id_command",
    "lanes", "lane_statuses", "rfc_link_field", "terminal_rfc_status", "closeout_card",
    "structure_gate", "attended_review_skill", "station_registry_doc", "workstreams",
    "role_addenda",
)
OPTIONAL_KEYS = ("bookkeeping",)
LANE_KEYS = ("backlog", "ready", "platform_ready", "in_progress", "parked", "blocked", "done")


class ProjectConfigError(ValueError):
    """config/project.json is missing, unreadable, or violates the contract."""


def _require_token(config: dict[str, Any], key: str) -> None:
    value = config.get(key)
    if not isinstance(value, str) or not TOKEN_RE.fullmatch(value):
        raise ProjectConfigError(f"{key} must be a lowercase hyphenated token, got {value!r}")


def _require_text(config: dict[str, Any], key: str) -> None:
    value = config.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ProjectConfigError(f"{key} must be a non-empty string")


def _require_relative_path(config: dict[str, Any], key: str) -> None:
    _require_text(config, key)
    value = config[key]
    if value.startswith("/") or value.endswith("/") or "\n" in value:
        raise ProjectConfigError(f"{key} must be a repository-relative path without a trailing slash")


def validate(config: Any) -> dict[str, Any]:
    if not isinstance(config, dict):
        raise ProjectConfigError("project.json must be a JSON object")
    missing = [key for key in REQUIRED_KEYS if key not in config]
    if missing:
        raise ProjectConfigError(f"project.json lacks required keys: {', '.join(missing)}")
    extra = sorted(set(config) - set(REQUIRED_KEYS) - set(OPTIONAL_KEYS))
    if extra:
        raise ProjectConfigError(f"project.json has unknown keys: {', '.join(extra)}")
    if config["schema"] != SCHEMA:
        raise ProjectConfigError(f"project.json schema must be {SCHEMA!r}")
    _require_token(config, "project")
    _require_token(config, "session_prefix")
    _require_text(config, "display_name")
    _require_text(config, "origin_repo_name")
    for key in ("rfc_root", "issue_root", "reservations_dir", "station_registry_doc"):
        _require_relative_path(config, key)
    _require_text(config, "allocate_id_command")
    _require_text(config, "rfc_link_field")
    if (not re.fullmatch(r"[a-z][a-z0-9_]*", config["rfc_link_field"])
            or config["rfc_link_field"] in {"id", "title", "status", "depends_on", "parked_on",
                                           "blocked_by", "milestone", "completed", "closeout"}):
        raise ProjectConfigError("rfc_link_field must be a distinct front-matter key")
    _require_text(config, "terminal_rfc_status")

    next_id = config["next_id"]
    if not isinstance(next_id, dict) or next_id.get("mode") != "single-file":
        raise ProjectConfigError("next_id.mode must be 'single-file'")
    _require_relative_path(next_id, "path")

    lanes = config["lanes"]
    if not isinstance(lanes, dict) or set(lanes) != set(LANE_KEYS):
        raise ProjectConfigError(f"lanes must define exactly {', '.join(LANE_KEYS)}")
    for key in LANE_KEYS:
        if key == "platform_ready":
            platform_lanes = lanes[key]
            if not isinstance(platform_lanes, dict):
                raise ProjectConfigError("lanes.platform_ready must be an object")
            for platform, lane in platform_lanes.items():
                if platform not in PLATFORM_TOKENS or not isinstance(lane, str) or not TOKEN_RE.fullmatch(lane):
                    raise ProjectConfigError(f"lanes.platform_ready has invalid entry {platform!r}: {lane!r}")
        else:
            _require_token(lanes, key)
    if len(set(lane_names(config))) != len(lane_names(config)):
        raise ProjectConfigError("lane directory names must be distinct")

    statuses = config["lane_statuses"]
    if not isinstance(statuses, dict) or set(statuses) != set(lane_names(config)):
        raise ProjectConfigError("lane_statuses must name every lane exactly once")
    for lane, status in statuses.items():
        if not isinstance(status, str) or not status.strip():
            raise ProjectConfigError(f"lane_statuses[{lane!r}] must be a non-empty string")

    closeout = config["closeout_card"]
    if not isinstance(closeout, dict) or len(closeout) != 1:
        raise ProjectConfigError("closeout_card must hold exactly one detection rule")
    rule, value = next(iter(closeout.items()))
    if rule == "filename_pattern":
        if not isinstance(value, str) or not value:
            raise ProjectConfigError("closeout_card.filename_pattern must be a regex string")
        re.compile(value)
    elif rule == "front_matter":
        if not isinstance(value, dict) or len(value) != 1 or not all(
            isinstance(k, str) and isinstance(v, str) for k, v in value.items()
        ):
            raise ProjectConfigError("closeout_card.front_matter must map one key to one value")
        if not re.fullmatch(r"[a-z][a-z0-9_]*", next(iter(value))):
            raise ProjectConfigError("closeout_card.front_matter must name a valid field")
    else:
        raise ProjectConfigError(f"closeout_card has unknown rule {rule!r}")

    gate = config["structure_gate"]
    if not isinstance(gate, list) or not all(isinstance(item, str) and item.strip() for item in gate):
        raise ProjectConfigError("structure_gate must be a list of command strings")

    attended = config["attended_review_skill"]
    if attended is not None and (not isinstance(attended, str) or not attended.strip()):
        raise ProjectConfigError("attended_review_skill must be a skill name or null")

    workstreams = config["workstreams"]
    if not isinstance(workstreams, dict) or "main" not in workstreams:
        raise ProjectConfigError("workstreams must be an object containing 'main'")
    for name, spec in workstreams.items():
        if not TOKEN_RE.fullmatch(name):
            raise ProjectConfigError(f"workstream name {name!r} is not a token")
        if not isinstance(spec, dict) or not isinstance(spec.get("branch"), str) or not spec["branch"]:
            raise ProjectConfigError(f"workstream {name!r} needs a branch")
        if set(spec) - {"branch", "platform"}:
            raise ProjectConfigError(f"workstream {name!r} has unknown keys")
        if "platform" in spec and spec["platform"] not in PLATFORM_TOKENS:
            raise ProjectConfigError(f"workstream {name!r} names an unknown platform")

    addenda = config["role_addenda"]
    if not isinstance(addenda, dict) or not all(
        isinstance(role, str)
        and isinstance(lines, list)
        and all(isinstance(line, str) and line.strip() for line in lines)
        for role, lines in addenda.items()
    ):
        raise ProjectConfigError("role_addenda must map role names to lists of instruction strings")
    if "bookkeeping" in config:
        rules = config["bookkeeping"]
        keys = {"deferring_lanes", "parked_on", "blocked_by_forbidden_prefixes",
                "link_roots", "protected_globs", "editable_globs"}
        if not isinstance(rules, dict) or set(rules) != keys:
            raise ProjectConfigError("bookkeeping must define exactly " + ", ".join(sorted(keys)))
        for key, values in rules.items():
            if not isinstance(values, list) or not all(isinstance(v, str) and v.strip() for v in values):
                raise ProjectConfigError(f"bookkeeping.{key} must be a list of nonempty strings")
            if len(values) != len(set(values)):
                raise ProjectConfigError(f"bookkeeping.{key} contains duplicates")
        if set(rules["deferring_lanes"]) - set(lane_names(config)):
            raise ProjectConfigError("bookkeeping.deferring_lanes names an unknown lane")
        for key in ("parked_on", "blocked_by_forbidden_prefixes"):
            if any(not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", v) for v in rules[key]):
                raise ProjectConfigError(f"bookkeeping.{key} contains an invalid token")
        for key in ("link_roots", "protected_globs", "editable_globs"):
            for value in rules[key]:
                if value.startswith("/") or ".." in value.split("/") or "\\" in value or "\n" in value:
                    raise ProjectConfigError(f"bookkeeping.{key} must use repository-relative paths")
    return config


def load(path: Path = PROJECT_PATH) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ProjectConfigError(f"missing project configuration: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ProjectConfigError(f"invalid JSON in {path}: {exc}") from exc
    return validate(data)


def lane_names(config: dict[str, Any]) -> list[str]:
    """Every board lane directory, in board order."""
    lanes = config["lanes"]
    platform = [lanes["platform_ready"][key] for key in sorted(lanes["platform_ready"])]
    return [
        lanes["backlog"],
        lanes["ready"],
        *platform,
        lanes["in_progress"],
        lanes["parked"],
        lanes["blocked"],
        lanes["done"],
    ]


def active_lanes(config: dict[str, Any]) -> list[str]:
    return [lane for lane in lane_names(config) if lane != config["lanes"]["done"]]


def repo_root() -> Path:
    return SKILL_DIR.parents[2]


def session_name_pattern(config: dict[str, Any]) -> re.Pattern[str]:
    return re.compile(rf"^{re.escape(config['session_prefix'])}(?:-[a-z0-9]+)+$")


def station_name(config: dict[str, Any], workstream: str, platform: str, machine: str = "") -> str:
    """`<prefix>-<workstream>-<platform>[-<machine>]`, omitting the platform token
    when the workstream is itself platform-bound (so `<prefix>-win-jaegaw`, not
    `<prefix>-win-win-jaegaw`, and `<prefix>-main-mac` for the integrator)."""
    spec = config["workstreams"][workstream]
    parts = [config["session_prefix"], workstream]
    if "platform" not in spec:
        parts.append(platform)
    if machine:
        parts.append(machine)
    return "-".join(parts)


def lane_log_name(config: dict[str, Any]) -> str:
    return f"{config['project']}-codex-lanes.jsonl"


def closeout_card_matcher(config: dict[str, Any]):
    """Return a predicate (path, front_matter) -> bool for closeout-card detection."""
    rule, value = next(iter(config["closeout_card"].items()))
    if rule == "filename_pattern":
        pattern = re.compile(value)
        return lambda path, _matter: bool(pattern.fullmatch(path.name))
    key, expected = next(iter(value.items()))
    return lambda _path, matter: matter.get(key, "").strip().strip("\"'").lower() == expected.lower()


if __name__ == "__main__":
    import sys

    try:
        loaded = load()
    except ProjectConfigError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(2)
    print(f"project.json valid: {loaded['project']} ({loaded['display_name']})")
