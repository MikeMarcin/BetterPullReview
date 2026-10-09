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
    chunked,
    display_path,
    iter_source_files,
    normalize_input_paths,
    repo_root,
    resolve_clang_format,
    supports_option,
    version_text,
)


def run_clang_format(clang_format: str, files: Sequence[Path], *, dry_run: bool) -> int:
    root = repo_root()
    if dry_run and supports_option(clang_format, "--dry-run"):
        base_cmd = [clang_format, "--dry-run", "--Werror", "--style=file"]
    elif dry_run:
        print("clang-format does not support --dry-run; use format_check.py for a non-mutating check", file=sys.stderr)
        return 2
    else:
        base_cmd = [clang_format, "-i", "--style=file"]

    rc = 0
    for group in chunked(list(files)):
        cmd = [*base_cmd, *[str(path) for path in group]]
        completed = subprocess.run(cmd, cwd=str(root))
        if completed.returncode != 0:
            rc = completed.returncode
    return rc


def parse_args(argv: Optional[Sequence[str]]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Format selected C/C++ files with clang-format.")
    parser.add_argument("paths", nargs="*", help="Files or directories to format. Defaults to normal project source roots.")
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
    parser.add_argument("--dry-run", action="store_true", help="Ask clang-format to check formatting without editing when supported.")
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
        mutating=not args.dry_run,
    )
    if not usable:
        return 2

    files: List[Path] = sorted(iter_source_files(normalize_input_paths(args.paths, root), root, SOURCE_EXTENSIONS))
    if not files:
        print("No C/C++ source files found.")
        return 0

    action = "Checking" if args.dry_run else "Formatting"
    reported = version_text(version) if version is not None else "unknown version"
    print(f"{action} {len(files)} file(s) with {clang_format} ({reported})")
    rc = run_clang_format(clang_format, files, dry_run=args.dry_run)
    if rc == 0:
        for path in files:
            print(display_path(path, root))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
