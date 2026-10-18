#!/usr/bin/env python3
from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List, Optional, Sequence, Set

from _coding_standard import (
    PINNED_CLANG_FORMAT_DIRECTORIES,
    TIDY_EXTENSIONS,
    display_path,
    iter_source_files,
    normalize_input_paths,
    repo_root,
    resolve_tool,
)


def resolve_tidy_tool(requested: Optional[str]) -> str:
    """Prefer the documented LLVM 22 install without changing the host's PATH."""
    if requested is not None:
        return resolve_tool(requested)
    candidates = [shutil.which("clang-tidy-22")]
    suffix = ".exe" if os.name == "nt" else ""
    for directory in PINNED_CLANG_FORMAT_DIRECTORIES:
        candidate = Path(directory) / f"clang-tidy{suffix}"
        if candidate.is_file() and os.access(candidate, os.X_OK):
            candidates.append(str(candidate))
    candidates.append(shutil.which("clang-tidy"))
    for candidate in dict.fromkeys(item for item in candidates if item is not None):
        version = subprocess.run([candidate, "--version"], stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True, check=False)
        if version.returncode == 0 and re.search(r"\bversion\s+22(?:\.|\b)", version.stdout):
            return candidate
    raise FileNotFoundError("clang-tidy 22 is required for the default gate; install llvm@22 or clang-tidy-22. "
                            "Use --clang-tidy to select an explicit tool for a separate analysis.")


def find_compile_commands(root: Path, build_dir: Optional[Path], compile_commands: Optional[Path]) -> Optional[Path]:
    if compile_commands is not None:
        candidate = compile_commands if compile_commands.is_absolute() else root / compile_commands
        return candidate if candidate.is_file() else None

    if build_dir is not None:
        candidate_dir = build_dir if build_dir.is_absolute() else root / build_dir
        candidate = candidate_dir / "compile_commands.json"
        return candidate if candidate.is_file() else None

    code_root = next(
        (candidate for candidate in (root, *root.parents) if (candidate / "build-projects.json").is_file()),
        None,
    )
    candidates = []
    if code_root is not None:
        candidates.extend(
            sorted(
                (code_root / ".build/projects").glob("**/compile_commands.json"),
                key=lambda path: path.stat().st_mtime,
                reverse=True,
            )
        )

    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def changed_paths(root: Path) -> List[str]:
    completed = subprocess.run(
        ["git", "diff", "--relative", "--name-only", "--diff-filter=ACMR", "HEAD", "--"],
        cwd=str(root),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        if completed.stderr:
            print(completed.stderr, end="", file=sys.stderr)
        return []
    return [line for line in completed.stdout.splitlines() if line.strip()]


def compile_command_files(compile_commands: Path) -> Set[Path]:
    data = json.loads(compile_commands.read_text())
    files: Set[Path] = set()
    for entry in data:
        raw_file = entry.get("file")
        if not raw_file:
            continue
        path = Path(raw_file)
        if not path.is_absolute():
            directory = Path(entry.get("directory", compile_commands.parent))
            path = directory / path
        files.add(path.resolve())
    return files


# Flags GCC writes into the compilation database that clang-tidy's Clang driver
# rejects outright as unknown arguments, which ends every translation unit in
# clang-diagnostic-error before a single check runs. CMake emits these for its
# C++20 module scanning; this tree declares no module, so dropping them for the
# analyser changes nothing it would have seen.
UNSUPPORTED_DRIVER_FLAGS = ("-fmodules-ts", "-fmodule-mapper=", "-fdeps-format=")


def strip_unsupported_driver_flags(argument: str) -> bool:
    return any(
        argument == flag or argument.startswith(flag)
        for flag in UNSUPPORTED_DRIVER_FLAGS
    )


def filtered_compilation_database(compile_commands: Path) -> Optional[Path]:
    """Rewrite the database without flags the analyser cannot accept.

    Returns a directory holding the rewritten database, or None when nothing
    needed removing and the original directory can be used as-is.
    """
    entries = json.loads(compile_commands.read_text(encoding="utf-8"))
    removed: Set[str] = set()
    for entry in entries:
        if "arguments" in entry:
            kept = [a for a in entry["arguments"] if not strip_unsupported_driver_flags(a)]
            removed.update(a for a in entry["arguments"] if strip_unsupported_driver_flags(a))
            entry["arguments"] = kept
        elif "command" in entry:
            parts = shlex.split(entry["command"])
            kept = [a for a in parts if not strip_unsupported_driver_flags(a)]
            removed.update(a for a in parts if strip_unsupported_driver_flags(a))
            entry["command"] = shlex.join(kept)
    if not removed:
        return None

    print(f"Removed {len(removed)} compiler flag(s) the analyser cannot accept:")
    for flag in sorted(removed):
        print(f"  {flag}")
    directory = Path(tempfile.mkdtemp(prefix="bpr-tidy-db-"))
    (directory / "compile_commands.json").write_text(
        json.dumps(entries, indent=2), encoding="utf-8"
    )
    return directory


def host_extra_args() -> List[str]:
    # The compilation database deliberately carries the product build's
    # -Werror policy. Keep compiler warnings as warnings here so
    # --warnings-as-errors can remain scoped to clang-tidy diagnostics; syntax
    # and semantic compiler errors still fail the invocation.
    extra_args = ["--extra-arg=-Wno-error", "--extra-arg=-Wno-missing-designated-field-initializers"]

    if sys.platform != "darwin":
        return extra_args

    completed = subprocess.run(
        ["xcrun", "--show-sdk-path"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        return extra_args

    sdk_path = completed.stdout.strip()
    if not sdk_path:
        return extra_args
    return [*extra_args, "--extra-arg=-isysroot", f"--extra-arg={sdk_path}"]


def print_compile_commands_help(root: Path, build_dir: Optional[Path], compile_commands: Optional[Path]) -> None:
    if compile_commands is not None:
        print(f"compile_commands.json not found: {compile_commands}", file=sys.stderr)
    elif build_dir is not None:
        print(f"compile_commands.json not found under build directory: {build_dir}", file=sys.stderr)
    else:
        print("compile_commands.json not found.", file=sys.stderr)
    print("", file=sys.stderr)
    print("Generate one with CMake, for example:", file=sys.stderr)
    print("  cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON", file=sys.stderr)
    print("", file=sys.stderr)
    print(f"Repository root: {root}", file=sys.stderr)


def parse_args(argv: Optional[Sequence[str]]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run clang-tidy on selected project translation units.")
    parser.add_argument("--root", type=Path, default=None, help="Project root to scan. Defaults to the current working directory.")
    parser.add_argument("--build-dir", type=Path, default=None, help="Build directory containing compile_commands.json.")
    parser.add_argument("--compile-commands", type=Path, default=None, help="Path to compile_commands.json.")
    parser.add_argument("--clang-tidy", default=None, help="Explicit analyser path or PATH name; default discovery selects LLVM 22.")
    parser.add_argument("--fix", action="store_true", help="Allow clang-tidy to apply fixes.")
    parser.add_argument("--paths", nargs="*", default=None, help="Files or directories to check. Defaults to normal project roots.")
    parser.add_argument("--changed", action="store_true", help="Check changed translation units according to Git.")
    parser.add_argument("--warnings-as-errors", action="store_true", help="Promote all clang-tidy warnings to errors.")
    parser.add_argument("--jobs", type=int, default=os.environ.get("CODING_STANDARD_TIDY_JOBS", "1"),
                        help="Concurrent analyser processes (default: CODING_STANDARD_TIDY_JOBS, or 1).")
    args = parser.parse_args(argv)
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    if args.fix and args.jobs != 1:
        parser.error("--fix requires --jobs=1 because translation units can share headers")
    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    root = repo_root(args.root)

    try:
        clang_tidy = resolve_tidy_tool(args.clang_tidy)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    compile_commands = find_compile_commands(root, args.build_dir, args.compile_commands)
    if compile_commands is None:
        print_compile_commands_help(root, args.build_dir, args.compile_commands)
        return 2

    if args.changed:
        raw_paths = changed_paths(root)
        if not raw_paths:
            print("No changed C/C++ translation units found for clang-tidy.")
            return 0
    else:
        raw_paths = args.paths if args.paths is not None else []

    files = sorted(iter_source_files(normalize_input_paths(raw_paths, root), root, TIDY_EXTENSIONS))
    if not files:
        print("No C/C++ translation units found for clang-tidy.")
        return 0

    compile_files = compile_command_files(compile_commands)
    files_in_database = [path for path in files if path.resolve() in compile_files]
    skipped = len(files) - len(files_in_database)
    if skipped:
        print(f"Skipping {skipped} translation unit(s) absent from {compile_commands}.")
    files = files_in_database
    if not files:
        print("No selected C/C++ translation units are present in the compilation database.")
        return 0

    print(f"Running clang-tidy on {len(files)} translation unit(s), {args.jobs} process(es)", flush=True)
    print(f"Compilation database: {compile_commands}", flush=True)
    rewritten = filtered_compilation_database(compile_commands)
    build_root = rewritten if rewritten is not None else compile_commands.parent

    extra_args = host_extra_args()
    failures: List[Path] = []

    def check_file(path: Path):
        cmd = [clang_tidy, str(path), "-p", str(build_root), *extra_args]
        if args.warnings_as_errors:
            cmd.append("--warnings-as-errors=*")
        if args.fix:
            cmd.append("--fix")
        completed = subprocess.run(cmd, cwd=str(root), stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True)
        return path, cmd, completed

    try:
        # Preserve file order and whole diagnostic blocks while read-only
        # analysis runs concurrently. Fixes stay serial: two units can both
        # propose edits to the same header.
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for path, cmd, completed in pool.map(check_file, files):
                print("+", " ".join(cmd), flush=True)
                print(completed.stdout, end="", flush=True)
                if completed.returncode != 0:
                    failures.append(path)
    finally:
        # BPR-1051. The rewritten database is this run's own directory, and
        # nothing else removes it: every run that stripped a flag left one
        # `bpr-tidy-db-*` in the temporary directory.
        if rewritten is not None:
            shutil.rmtree(rewritten)

    if failures:
        print("clang-tidy reported violations or tool failures:", file=sys.stderr)
        for path in failures:
            print(f"  {display_path(path, root)}", file=sys.stderr)
        return 1

    print("clang-tidy passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
