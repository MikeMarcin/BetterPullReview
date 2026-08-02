#!/usr/bin/env python3
"""Render Claude Code and Codex custom agents from shared BetterPullReview role/routing data."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from pathlib import Path
from typing import Any

ROLE_NAME_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
MODEL_CLASSES = {"mechanical", "balanced", "deep", "deep-max"}
MUTATIONS = {"read-only", "workspace-write"}
PLACEHOLDER_RE = re.compile(r"^<[^>]+>$|\$\{")


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ValueError(f"missing JSON file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON in {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ValueError(f"expected JSON object in {path}")
    return value


def deep_merge(base: Any, override: Any) -> Any:
    if isinstance(base, dict) and isinstance(override, dict):
        merged = dict(base)
        for key, value in override.items():
            merged[key] = deep_merge(merged[key], value) if key in merged else value
        return merged
    return override


def validate_roles(data: dict[str, Any]) -> dict[str, dict[str, Any]]:
    if data.get("schema") != "betterpullreview.implement-rfc.roles.v1":
        raise ValueError("roles.json has an unsupported schema")
    roles = data.get("roles")
    if not isinstance(roles, dict) or not roles:
        raise ValueError("roles.json must contain a non-empty roles object")
    for name, role in roles.items():
        if not isinstance(name, str) or not ROLE_NAME_RE.fullmatch(name):
            raise ValueError(f"invalid role name: {name!r}")
        if not isinstance(role, dict):
            raise ValueError(f"role {name} must be an object")
        if not isinstance(role.get("description"), str) or not role["description"].strip():
            raise ValueError(f"role {name} needs a description")
        if role.get("mutation") not in MUTATIONS:
            raise ValueError(f"role {name} has invalid mutation")
        if not isinstance(role.get("max_turns"), int) or role["max_turns"] < 1:
            raise ValueError(f"role {name} has invalid max_turns")
        if not isinstance(role.get("nested_utility"), bool):
            raise ValueError(f"role {name} has invalid nested_utility")
        instructions = role.get("instructions")
        if not isinstance(instructions, list) or not instructions or not all(
            isinstance(item, str) and item.strip() for item in instructions
        ):
            raise ValueError(f"role {name} needs non-empty instruction strings")
    return roles


def validate_routing(
    data: dict[str, Any], roles: dict[str, dict[str, Any]]
) -> tuple[
    dict[str, str],
    dict[str, dict[str, Any]],
    dict[str, dict[str, Any]],
]:
    if data.get("schema") != "betterpullreview.implement-rfc.routing.v1":
        raise ValueError("routing.json has an unsupported schema")
    assignments = data.get("assignments")
    providers = data.get("providers")
    if not isinstance(assignments, dict):
        raise ValueError("routing assignments must be an object")
    if set(assignments) != set(roles):
        missing = sorted(set(roles) - set(assignments))
        extra = sorted(set(assignments) - set(roles))
        raise ValueError(f"routing role mismatch; missing={missing}, extra={extra}")
    for role_name, model_class in assignments.items():
        if model_class not in MODEL_CLASSES:
            raise ValueError(f"routing assigns invalid class {model_class} to {role_name}")
    if not isinstance(providers, dict) or "codex" not in providers or "claude" not in providers:
        raise ValueError("routing providers must contain codex and claude")

    codex_classes = providers["codex"].get("classes")
    claude_classes = providers["claude"].get("classes")
    if not isinstance(codex_classes, dict) or not isinstance(claude_classes, dict):
        raise ValueError("provider classes must be objects")
    for provider_name, classes in (("codex", codex_classes), ("claude", claude_classes)):
        for model_class in set(assignments.values()):
            route = classes.get(model_class)
            if not isinstance(route, dict) or not isinstance(route.get("model"), str):
                raise ValueError(f"{provider_name} lacks model route for {model_class}")
            model = route["model"].strip()
            if not model or PLACEHOLDER_RE.search(model):
                raise ValueError(f"{provider_name} route {model_class} contains a placeholder")
            effort = route.get("effort")
            if effort is not None and not isinstance(effort, str):
                raise ValueError(f"{provider_name} route {model_class} has invalid effort")
    return assignments, codex_classes, claude_classes


def q(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def role_prompt(role: dict[str, Any]) -> str:
    return "\n\n".join(role["instructions"]) + "\n"


def render_codex_agent(name: str, role: dict[str, Any], route: dict[str, Any], digest: str) -> str:
    sandbox = "read-only" if role["mutation"] == "read-only" else "workspace-write"
    lines = [
        f"# Generated by bpr-implement-rfc/scripts/render_runtime_adapters.py ({digest}).",
        "# Do not edit directly; update config/roles.json or config/routing.json.",
        f"name = {q(name)}",
        f"description = {q(role['description'])}",
        f"model = {q(route['model'])}",
    ]
    if route.get("effort") is not None:
        lines.append(f"model_reasoning_effort = {q(route['effort'])}")
    lines.extend(
        [
            f"sandbox_mode = {q(sandbox)}",
            f"developer_instructions = {q(role_prompt(role))}",
            "",
            "[agents]",
            f"enabled = {'true' if role['nested_utility'] else 'false'}",
            "",
        ]
    )
    if role["nested_utility"]:
        lines.insert(-1, "max_concurrent_threads_per_session = 2")
    return "\n".join(lines)


def render_claude_agent(name: str, role: dict[str, Any], route: dict[str, Any], digest: str) -> str:
    read_only = role["mutation"] == "read-only"
    tools = "Read, Grep, Glob, Bash" if read_only else "Read, Grep, Glob, Bash, Edit, Write, Skill"
    if role["nested_utility"]:
        tools += ", Agent"
    permission_mode = "plan" if read_only else "acceptEdits"
    frontmatter = [
        "---",
        f"name: {name}",
        f"description: {q(role['description'])}",
        f"tools: {tools}",
        f"model: {q(route['model'])}",
        f"permissionMode: {permission_mode}",
        f"maxTurns: {role['max_turns']}",
    ]
    if route.get("effort") is not None:
        frontmatter.append(f"effort: {route['effort']}")
    frontmatter.extend(["---", ""])
    body = [
        f"<!-- Generated by bpr-implement-rfc/scripts/render_runtime_adapters.py ({digest}). -->",
        "<!-- Do not edit directly; update config/roles.json or config/routing.json. -->",
        "",
        f"# {name}",
        "",
    ]
    for instruction in role["instructions"]:
        body.append(f"- {instruction}")
    body.append("")
    return "\n".join(frontmatter + body)


def render_claude_skill_shim(
    skill_name: str, skill_description: str, digest: str
) -> str:
    return f'''---
name: {skill_name}
description: {q(skill_description)}
---

# BetterPullReview RFC portable entrypoint

This is a generated Claude Code entrypoint ({digest}). Read and follow
`${{CLAUDE_PROJECT_DIR}}/.agents/skills/bpr-implement-rfc/SKILL.md` as the
authoritative skill before doing any RFC work. Resolve every relative path in
that skill from `${{CLAUDE_PROJECT_DIR}}/.agents/skills/bpr-implement-rfc/`.
'''


def source_digest(*paths: Path) -> str:
    hasher = hashlib.sha256()
    for path in sorted(paths, key=lambda candidate: str(candidate)):
        hasher.update(path.name.encode("utf-8"))
        hasher.update(b"\0")
        hasher.update(path.read_bytes())
        hasher.update(b"\0")
    return hasher.hexdigest()[:16]


def parse_skill_frontmatter(skill_md: Path) -> tuple[str, str]:
    text = skill_md.read_text(encoding="utf-8")
    match = re.search(r"\A---\s*\n(.*?)\n---\s*\n", text, re.DOTALL)
    if not match:
        raise ValueError(f"missing YAML frontmatter in {skill_md}")
    values: dict[str, str] = {}
    for line in match.group(1).splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        values[key.strip()] = value.strip()
    name = values.get("name", "")
    description = values.get("description", "")
    if not name or not ROLE_NAME_RE.fullmatch(name):
        raise ValueError(f"missing or invalid skill name in {skill_md}")
    if not description:
        raise ValueError(f"missing description in {skill_md}")
    return name, description


def write_or_check(path: Path, content: str, check: bool) -> bool:
    normalized = content.replace("\r\n", "\n")
    if check:
        if not path.exists():
            print(f"missing generated file: {path}", file=sys.stderr)
            return False
        actual = path.read_text(encoding="utf-8").replace("\r\n", "\n")
        if actual != normalized:
            print(f"stale generated file: {path}", file=sys.stderr)
            return False
        return True
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(normalized, encoding="utf-8", newline="\n")
    os.replace(temporary, path)
    return True


def main() -> int:
    script_path = Path(__file__).resolve()
    skill_dir = script_path.parents[1]
    default_repo_root = skill_dir.parents[2]

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=default_repo_root)
    parser.add_argument(
        "--routing-override",
        type=Path,
        help="Optional candidate routing overlay; use with a temporary --repo-root for evaluation.",
    )
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    roles_path = skill_dir / "config" / "roles.json"
    routing_path = skill_dir / "config" / "routing.json"
    skill_path = skill_dir / "SKILL.md"
    roles_data = load_json(roles_path)
    routing_data = load_json(routing_path)
    digest_paths = [script_path, skill_path, roles_path, routing_path]
    if args.routing_override:
        override_path = args.routing_override.resolve()
        routing_data = deep_merge(routing_data, load_json(override_path))
        digest_paths.append(override_path)

    roles = validate_roles(roles_data)
    assignments, codex_classes, claude_classes = validate_routing(routing_data, roles)

    digest = source_digest(*digest_paths)
    repo_root = args.repo_root.resolve()
    generated: dict[Path, str] = {}
    for name, role in sorted(roles.items()):
        model_class = assignments[name]
        generated[repo_root / ".codex" / "agents" / f"{name}.toml"] = render_codex_agent(
            name, role, codex_classes[model_class], digest
        )
        generated[repo_root / ".claude" / "agents" / f"{name}.md"] = render_claude_agent(
            name, role, claude_classes[model_class], digest
        )

    skill_name, skill_description = parse_skill_frontmatter(skill_path)
    generated[
        repo_root / ".claude" / "skills" / "bpr-implement-rfc" / "SKILL.md"
    ] = render_claude_skill_shim(skill_name, skill_description, digest)

    manifest_entries: list[dict[str, str]] = []
    for path, content in sorted(generated.items(), key=lambda item: str(item[0])):
        relative = path.relative_to(repo_root).as_posix()
        manifest_entries.append(
            {
                "path": relative,
                "sha256": hashlib.sha256(content.encode("utf-8")).hexdigest(),
            }
        )
    manifest = {
        "schema": "betterpullreview.implement-rfc.runtime-adapters.v1",
        "source_digest": digest,
        "assignments": assignments,
        "verified_as_of": routing_data.get("verified_as_of"),
        "files": manifest_entries,
    }
    manifest_path = (
        repo_root / ".agents" / "skills" / "bpr-implement-rfc" / "generated" / "runtime-adapters.json"
    )
    generated[manifest_path] = json.dumps(manifest, indent=2, sort_keys=True) + "\n"

    ok = True
    for path, content in sorted(generated.items(), key=lambda item: str(item[0])):
        ok = write_or_check(path, content, args.check) and ok
    if not ok:
        return 1
    action = "validated" if args.check else "rendered"
    print(f"{action} {len(generated)} files for source={digest}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
