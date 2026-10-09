#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Sequence

from _coding_standard import (
    SOURCE_EXTENSIONS,
    add_clang_format_version_argument,
    check_clang_format_version,
    display_path,
    iter_source_files,
    normalize_input_paths,
    read_nul_paths_from_stdin,
    repo_root,
    resolve_clang_format,
    supports_option,
    version_text,
)


def check_with_dry_run(clang_format: str, files: Sequence[Path], root: Path) -> int:
    dirty: List[Path] = []
    tool_failed = False

    for path in files:
        completed = subprocess.run(
            [clang_format, "--dry-run", "--Werror", "--style=file", str(path)],
            cwd=str(root),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if completed.returncode != 0:
            dirty.append(path)
            if completed.stdout:
                print(completed.stdout, end="")
            if completed.stderr:
                print(completed.stderr, end="", file=sys.stderr)
            if "would be reformatted" not in completed.stderr and "code should be clang-formatted" not in completed.stderr:
                tool_failed = True

    if dirty:
        print("Files needing formatting:")
        for path in dirty:
            print(f"  {display_path(path, root)}")
        return 2 if tool_failed else 1

    return 0


def check_with_compare(clang_format: str, files: Sequence[Path], root: Path) -> int:
    dirty: List[Path] = []

    for path in files:
        original = path.read_bytes()
        completed = subprocess.run(
            [clang_format, "--style=file", str(path)],
            cwd=str(root),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if completed.returncode != 0:
            if completed.stdout:
                sys.stdout.buffer.write(completed.stdout)
            if completed.stderr:
                sys.stderr.buffer.write(completed.stderr)
            return completed.returncode
        if completed.stdout != original:
            dirty.append(path)

    if dirty:
        print("Files needing formatting:")
        for path in dirty:
            print(f"  {display_path(path, root)}")
        return 1

    return 0


def parse_args(argv: Optional[Sequence[str]]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check selected C/C++ files with clang-format without modifying them.")
    parser.add_argument("paths", nargs="*", help="Files or directories to check. Defaults to normal project source roots.")
    parser.add_argument("--root", type=Path, default=None, help="Project root to scan. Defaults to the current working directory.")
    parser.add_argument(
        "--clang-format",
        default=None,
        help=(
            "clang-format executable path or PATH name. Defaults to the first clang-format 22 found:"
            " clang-format-22 on PATH, then Homebrew's llvm@22 and Debian's llvm-22, then clang-format"
            " on PATH."
        ),
    )
    add_clang_format_version_argument(parser)
    parser.add_argument("--from-stdin", action="store_true", help=argparse.SUPPRESS)
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    root = repo_root(args.root)

    try:
        clang_format = resolve_clang_format(args.clang_format)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    usable, version = check_clang_format_version(
        clang_format,
        allow_unsupported=args.allow_unsupported_clang_format,
    )
    if not usable:
        return 2

    raw_paths = list(args.paths)
    if args.from_stdin:
        raw_paths.extend(read_nul_paths_from_stdin())

    if args.from_stdin and not raw_paths:
        files: List[Path] = []
    else:
        files = sorted(iter_source_files(normalize_input_paths(raw_paths, root), root, SOURCE_EXTENSIONS))
    if not files:
        print("No C/C++ source files found.")
        return 0

    reported = version_text(version) if version is not None else "unknown version"
    print(f"Checking {len(files)} file(s) with {clang_format} ({reported})")
    if supports_option(clang_format, "--dry-run"):
        rc = check_with_dry_run(clang_format, files, root)
    else:
        rc = check_with_compare(clang_format, files, root)

    if rc == 0:
        print("Formatting check passed.")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
