#!/usr/bin/env python3
"""Launch a routed Codex delegated lane through the codex-companion runtime.

The primary Claude integrator uses this launcher to run an RFC role on Codex:
it resolves the installed openai-codex plugin's companion script, maps the role
to its checked-in Codex model/effort route, prepends the rendered role prompt
(including the startup identity line) to the lane assignment, and records the
launch in the checkout's Codex lane log (`<project>-codex-lanes.jsonl` under the
git common directory, named from config/project.json).

Write-capable lanes must run in their own worktree: the launcher refuses
--write when the target working directory is the shared integration checkout
unless --allow-shared-write explicitly claims the single shared-workspace
writer slot.

status/result/cancel forward to the companion; pass the same --cwd the lane
was launched with, because companion job state is per-workspace.
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
SKILL_DIR = SCRIPT_DIR.parent
COMPANION_ENV = "IMPLEMENT_RFC_CODEX_COMPANION"

# The companion runtime currently caps reasoning effort at xhigh; the native
# Codex CLI route may name max. Downgrade per the skill's fallback rule and
# report it rather than failing the lane.
COMPANION_EFFORTS = {"none", "minimal", "low", "medium", "high", "xhigh"}
EFFORT_DOWNGRADES = {"max": "xhigh"}

sys.path.insert(0, str(SCRIPT_DIR))
import project_config  # noqa: E402
import render_runtime_adapters as renderer  # noqa: E402

PROJECT = project_config.load()
LANE_LOG_NAME = project_config.lane_log_name(PROJECT)


class LaunchError(Exception):
    pass


def find_companion() -> Path:
    override = os.environ.get(COMPANION_ENV)
    if override:
        candidate = Path(override).expanduser()
        if not candidate.is_file():
            raise LaunchError(f"{COMPANION_ENV} does not exist: {candidate}")
        return candidate
    cache_root = Path.home() / ".claude" / "plugins" / "cache" / "openai-codex" / "codex"
    candidates = sorted(
        cache_root.glob("*/scripts/codex-companion.mjs"),
        key=lambda path: [
            int(part) if part.isdigit() else part
            for part in path.parents[1].name.split(".")
        ],
    )
    if not candidates:
        raise LaunchError(
            "no installed codex-companion runtime found; install the openai-codex "
            f"Claude Code plugin or set {COMPANION_ENV}"
        )
    return candidates[-1]


def load_role_route(role_name: str) -> tuple[dict, dict]:
    roles = renderer.validate_roles(renderer.load_json(SKILL_DIR / "config" / "roles.json"))
    routing = renderer.load_json(SKILL_DIR / "config" / "routing.json")
    assignments, codex_classes, _ = renderer.validate_routing(routing, roles)
    if role_name not in roles:
        raise LaunchError(f"unknown role {role_name!r}; known roles: {', '.join(sorted(roles))}")
    return roles[role_name], codex_classes[assignments[role_name]]


def effective_effort(route: dict) -> tuple[str | None, str | None]:
    requested = route.get("effort")
    if requested is None or requested in COMPANION_EFFORTS:
        return requested, requested
    downgraded = EFFORT_DOWNGRADES.get(requested)
    if downgraded is None:
        raise LaunchError(f"route effort {requested!r} is not supported by the companion runtime")
    return requested, downgraded


def git_toplevel(directory: Path) -> Path | None:
    result = subprocess.run(
        ["git", "-C", str(directory), "rev-parse", "--show-toplevel"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None
    return Path(result.stdout.strip()).resolve()


def lane_log_path(repo_root: Path) -> Path | None:
    result = subprocess.run(
        ["git", "-C", str(repo_root), "rev-parse", "--git-common-dir"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None
    common = Path(result.stdout.strip())
    if not common.is_absolute():
        common = (repo_root / common).resolve()
    return common / LANE_LOG_NAME


def record_launch(repo_root: Path, record: dict) -> None:
    log_path = lane_log_path(repo_root)
    if log_path is None:
        print("warning: not a git checkout; codex lane launch not logged", file=sys.stderr)
        return
    with log_path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(record, sort_keys=True) + "\n")


def compose_prompt(role_name: str, role: dict, route: dict, lane_name: str, task_text: str) -> str:
    role_prompt = renderer.role_prompt(role_name, role, route, PROJECT)
    # The companion names a job by shortening the first line of the prompt, so
    # whatever leads the text becomes the label in the operator's task list.
    # Leading with the role prompt would make every lane read identically.
    banner = f"{PROJECT['display_name']} lane {lane_name} ({role_name})\n\n"
    header = (
        f"Session name: {lane_name}\n"
        f"Lane assignment from the primary {PROJECT['display_name']} RFC integrator follows. "
        "Honor every role instruction above; the assignment can narrow, never widen, your authority. "
        "This runtime returns only your final message, so begin that final message with your startup "
        "identity line, then the role's compact sections.\n"
    )
    return f"{banner}{role_prompt}\n---\n{header}\n{task_text.strip()}\n"


def cmd_launch(args: argparse.Namespace) -> int:
    role, route = load_role_route(args.role)
    if args.task_file:
        task_text = Path(args.task_file).read_text(encoding="utf-8")
    elif args.task:
        task_text = args.task
    else:
        raise LaunchError("provide --task or --task-file")

    lane_name = args.lane_name or args.role.replace("-", "_")
    if not lane_name.replace("_", "-").startswith(args.role):
        raise LaunchError(f"lane name {lane_name!r} must keep the role prefix {args.role!r}")

    write = bool(args.write)
    if write and role["mutation"] == "read-only":
        raise LaunchError(f"role {args.role} is read-only; do not launch it with --write")

    cwd = Path(args.cwd).resolve() if args.cwd else Path.cwd().resolve()
    repo_root = args.repo_root.resolve()
    if write and not args.allow_shared_write:
        target_top = git_toplevel(cwd)
        if target_top is not None and target_top == git_toplevel(repo_root):
            raise LaunchError(
                f"--write targets the shared checkout {target_top}; run write lanes in their "
                "own worktree (--cwd <worktree>), or pass --allow-shared-write to claim the "
                "single shared-workspace writer slot"
            )

    requested_effort, effort = effective_effort(route)
    if requested_effort != effort:
        print(
            f"note: companion runtime caps effort at xhigh; running {args.role} at "
            f"{effort} instead of {requested_effort} (report this downgrade in the closeout card)",
            file=sys.stderr,
        )

    route_for_prompt = {**route, "effort": effort}
    prompt = compose_prompt(args.role, role, route_for_prompt, lane_name, task_text)
    prompt_file = tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", suffix=".md", prefix=f"{lane_name}-", delete=False
    )
    with prompt_file as handle:
        handle.write(prompt)

    companion = find_companion()
    command = ["node", str(companion), "task", "--json", "--prompt-file", prompt_file.name]
    command.extend(["--model", route["model"]])
    if effort is not None:
        command.extend(["--effort", effort])
    if args.cwd:
        command.extend(["--cwd", str(cwd)])
    if write:
        command.append("--write")
    if args.background:
        command.append("--background")

    record_launch(
        repo_root,
        {
            "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "runtime": "codex",
            "role": args.role,
            "lane_name": lane_name,
            "model": route["model"],
            "effort_requested": requested_effort,
            "effort": effort,
            "write": write,
            "background": bool(args.background),
            "cwd": str(cwd),
        },
    )
    try:
        # The companion reads the prompt file before returning, even for
        # --background launches, so it is safe to remove afterwards.
        completed = subprocess.run(command)
    finally:
        Path(prompt_file.name).unlink(missing_ok=True)
    return completed.returncode


def cmd_forward(args: argparse.Namespace) -> int:
    companion = find_companion()
    rest = [*args.unknown, *args.rest]
    rest = [token for token in rest if token != "--"]
    completed = subprocess.run(["node", str(companion), args.command, *rest])
    return completed.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=SKILL_DIR.parents[2])
    sub = parser.add_subparsers(dest="command", required=True)

    launch = sub.add_parser("launch", help="Launch one routed Codex lane through the companion runtime.")
    launch.add_argument("--role", required=True)
    launch.add_argument("--lane-name", help="Role-prefixed session name, e.g. rfc_reviewer_bpr_0042.")
    launch.add_argument("--task", help="Lane assignment text.")
    launch.add_argument("--task-file", help="File containing the lane assignment text.")
    launch.add_argument("--cwd", help="Working directory for the lane; a worktree for write lanes.")
    launch.add_argument("--write", action="store_true")
    launch.add_argument("--background", action="store_true")
    launch.add_argument(
        "--allow-shared-write",
        action="store_true",
        help="Claim the single shared-workspace writer slot instead of a worktree.",
    )
    launch.set_defaults(func=cmd_launch)

    for name, help_text in (
        ("status", "Forward to companion status; pass the lane's --cwd."),
        ("result", "Forward to companion result; pass the lane's --cwd."),
        ("cancel", "Forward to companion cancel; pass the lane's --cwd."),
    ):
        forward = sub.add_parser(name, help=help_text)
        forward.add_argument("rest", nargs=argparse.REMAINDER)
        forward.set_defaults(func=cmd_forward)

    args, unknown = parser.parse_known_args()
    if args.command == "launch" and unknown:
        parser.error(f"unrecognized arguments: {' '.join(unknown)}")
    args.unknown = unknown
    return args.func(args)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except LaunchError as exc:
        print(f"codex lane error: {exc}", file=sys.stderr)
        raise SystemExit(2)
