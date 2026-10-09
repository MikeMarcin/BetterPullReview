#!/usr/bin/env python3
"""Run the format gate, or report a skip when there is no clang-format 22.

A test harness runs the repo-wide format check as one of its tests. That test
has two outcomes that must never be confused: the tree is unformatted, which
fails it, and there is no clang-format 22 to run, which says
nothing about the tree. The second exits with `--skip-exit-code` so the harness
reports the test as skipped rather than passed or failed.

`--clang-format` is the tool configure found, given as a path. A PATH name would
bring back the problem discovery exists to solve: a station can put another
release ahead of a 22 on PATH (Homebrew's default `llvm` is 23). Leaving it out
means configure found none.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Optional, Sequence

import format_check
from _coding_standard import (
    REQUIRED_CLANG_FORMAT_MAJOR,
    clang_format_version,
    is_required_clang_format,
    resolve_tool,
    unsupported_clang_format_reason,
)

# The gate's own exit codes: formatted, unformatted, and the tool failed.
GATE_EXIT_CODES = (0, 1, 2)


def parse_args(argv: Optional[Sequence[str]]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, required=True, help="Project root to check.")
    parser.add_argument(
        "--clang-format",
        default=None,
        help="Path of the clang-format configure found. Omit it when configure found none.",
    )
    parser.add_argument(
        "--skip-exit-code",
        type=int,
        required=True,
        help="Exit code the test harness reads as a skip.",
    )
    args = parser.parse_args(argv)
    if args.skip_exit_code in GATE_EXIT_CODES:
        parser.error(f"--skip-exit-code must differ from the gate's own exit codes {GATE_EXIT_CODES}")
    return args


def skip(reason: str, skip_exit_code: int) -> int:
    print(f"SKIPPED: {reason}.")
    print(
        "This is not a formatting verdict. A missing clang-format, or another release than"
        f" {REQUIRED_CLANG_FORMAT_MAJOR}, says nothing about the tree."
    )
    return skip_exit_code


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    required = REQUIRED_CLANG_FORMAT_MAJOR

    if args.clang_format is None:
        return skip(
            f"configure found no clang-format {required}; install one"
            f" (brew install llvm@{required}, apt install clang-format-{required}) and reconfigure",
            args.skip_exit_code,
        )

    try:
        tool = resolve_tool(args.clang_format)
    except FileNotFoundError as exc:
        return skip(f"{exc}; reconfigure to find a clang-format again", args.skip_exit_code)

    version = clang_format_version(tool)
    if not is_required_clang_format(version):
        reason = unsupported_clang_format_reason(tool, version)
        if version is not None:
            reason += (
                f"; this tree is rendered by clang-format {required} exactly."
                f" Reconfigure with -DBPR_CLANG_FORMAT_EXECUTABLE=<a clang-format {required}>"
            )
        return skip(reason, args.skip_exit_code)

    exit_code = format_check.main(["--root", str(args.root), "--clang-format", tool])
    if exit_code not in GATE_EXIT_CODES:
        # A verdict is never reported as a skip. Only format_check's fallback
        # path, for a clang-format without --dry-run, returns another code.
        return 2
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
