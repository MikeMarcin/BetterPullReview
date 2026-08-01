#!/usr/bin/env python3
"""Run BetterPullReview RFC adapter tests and verify generated files."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def run(command: list[str]) -> int:
    print("+", " ".join(command))
    return subprocess.call(command)


def main() -> int:
    skill_dir = Path(__file__).resolve().parents[1]
    renderer = skill_dir / "scripts" / "render_runtime_adapters.py"
    tests_dir = skill_dir / "tests"

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path)
    parser.add_argument("--skip-tests", action="store_true")
    args = parser.parse_args()

    if not args.skip_tests:
        code = run(
            [
                sys.executable,
                "-m",
                "unittest",
                "discover",
                "-s",
                str(tests_dir),
                "-p",
                "test_*.py",
            ]
        )
        if code != 0:
            return code

    command = [sys.executable, str(renderer), "--check"]
    if args.repo_root:
        command.extend(["--repo-root", str(args.repo_root)])
    return run(command)


if __name__ == "__main__":
    raise SystemExit(main())
