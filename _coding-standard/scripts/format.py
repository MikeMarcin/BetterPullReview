#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Sequence

from _coding_standard import (
    SOURCE_EXTENSIONS,
    chunked,
    display_path,
    iter_source_files,
    normalize_input_paths,
    repo_root,
    resolve_tool,
    supports_option,
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
    parser.add_argument("--clang-format", default="clang-format", help="clang-format executable path or PATH name.")
    parser.add_argument("--dry-run", action="store_true", help="Ask clang-format to check formatting without editing when supported.")
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    root = repo_root(args.root)

    try:
        clang_format = resolve_tool(args.clang_format)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    files: List[Path] = sorted(iter_source_files(normalize_input_paths(args.paths, root), root, SOURCE_EXTENSIONS))
    if not files:
        print("No C/C++ source files found.")
        return 0

    action = "Checking" if args.dry_run else "Formatting"
    print(f"{action} {len(files)} file(s) with {clang_format}")
    rc = run_clang_format(clang_format, files, dry_run=args.dry_run)
    if rc == 0:
        for path in files:
            print(display_path(path, root))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
