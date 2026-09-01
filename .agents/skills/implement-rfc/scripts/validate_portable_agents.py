#!/usr/bin/env python3
"""Run implement-rfc skill tests, verify checked-in generated files, and optionally
compare this skill's portable set against another checkout's copy."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

DISPLAY_SKILL_DIR = Path(".agents/skills/implement-rfc")


def run(command: list[str], *, display_command: list[str] | None = None) -> int:
    print("+", " ".join(display_command if display_command is not None else command))
    return subprocess.call(command)


def load_manifest(skill_dir: Path) -> dict[str, str]:
    path = skill_dir / "generated" / "portable-manifest.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    return {entry["path"]: entry["sha256"] for entry in data["files"]}


def compare_manifests(ours: dict[str, str], theirs: dict[str, str]) -> list[str]:
    """Human-readable differences between two portable manifests; empty when identical."""
    findings: list[str] = []
    for path in sorted(set(ours) | set(theirs)):
        if path not in theirs:
            findings.append(f"only here: {path}")
        elif path not in ours:
            findings.append(f"only there: {path}")
        elif ours[path] != theirs[path]:
            findings.append(f"differs: {path}")
    return findings


def main() -> int:
    skill_dir = Path(__file__).resolve().parents[1]
    renderer = skill_dir / "scripts" / "render_runtime_adapters.py"
    tests_dir = skill_dir / "tests"
    display_renderer = DISPLAY_SKILL_DIR / "scripts" / "render_runtime_adapters.py"
    display_tests_dir = DISPLAY_SKILL_DIR / "tests"

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path)
    parser.add_argument("--skip-tests", action="store_true")
    parser.add_argument(
        "--compare",
        type=Path,
        help="Another checkout's .agents/skills/implement-rfc directory; fail when the portable sets differ.",
    )
    args = parser.parse_args()

    if not args.skip_tests:
        command = [
            sys.executable,
            "-m",
            "unittest",
            "discover",
            "-s",
            str(tests_dir),
            "-p",
            "test_*.py",
        ]
        display_command = [
            sys.executable,
            "-m",
            "unittest",
            "discover",
            "-s",
            str(display_tests_dir),
            "-p",
            "test_*.py",
        ]
        code = run(command, display_command=display_command)
        if code != 0:
            return code

    command = [sys.executable, str(renderer), "--check"]
    display_command = [sys.executable, str(display_renderer), "--check"]
    if args.repo_root:
        command.extend(["--repo-root", str(args.repo_root)])
        display_command.extend(["--repo-root", str(args.repo_root)])
    code = run(command, display_command=display_command)
    if code != 0 or args.compare is None:
        return code

    other = args.compare.resolve()
    print(f"+ compare portable manifest with {other}")
    findings = compare_manifests(load_manifest(skill_dir), load_manifest(other))
    for finding in findings:
        print(f"  {finding}")
    if findings:
        print(f"portable set differs in {len(findings)} file(s)")
        return 1
    print("portable set identical")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
