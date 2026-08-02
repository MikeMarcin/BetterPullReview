#!/usr/bin/env python3
from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterable, Iterator, List, Optional, Sequence, Set


SOURCE_EXTENSIONS: Set[str] = {
    ".h",
    ".hpp",
    ".hh",
    ".hxx",
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".inl",
    ".ipp",
}

TIDY_EXTENSIONS: Set[str] = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
}

DEFAULT_SOURCE_ROOTS: Sequence[str] = (
    "include",
    "src",
    "modules",
    "examples",
    "benchmarks",
    "tests",
)

EXCLUDED_DIR_NAMES: Set[str] = {
    ".git",
    ".hg",
    ".svn",
    ".vs",
    "__pycache__",
    "build",
    "cmake-build-debug",
    "cmake-build-release",
    "debug",
    "release",
    "out",
    "third_party",
    "third-party",
    "extern",
    "external",
    "vendor",
    "generated",
    "_deps",
}


def repo_root(override: Optional[Path] = None) -> Path:
    if override is not None:
        return override.resolve()
    env_root = os.environ.get("CODING_STANDARD_REPO_ROOT")
    if env_root:
        return Path(env_root).resolve()
    return Path.cwd().resolve()


def display_path(path: Path, root: Optional[Path] = None) -> str:
    base = root if root is not None else repo_root()
    try:
        return path.resolve().relative_to(base.resolve()).as_posix()
    except ValueError:
        return str(path)


def resolve_tool(name_or_path: str) -> str:
    path = Path(name_or_path)
    if path.parent != Path(".") or path.is_absolute():
        if path.exists():
            return str(path)
        raise FileNotFoundError(f"Tool not found: {name_or_path}")

    found = shutil.which(name_or_path)
    if found is None:
        raise FileNotFoundError(f"Tool not found on PATH: {name_or_path}")
    return found


def default_roots(root: Path) -> List[Path]:
    return [root / rel for rel in DEFAULT_SOURCE_ROOTS if (root / rel).exists()]


def normalize_input_paths(paths: Sequence[str], root: Path) -> List[Path]:
    if not paths:
        return default_roots(root)

    out: List[Path] = []
    for raw in paths:
        path = Path(raw)
        if not path.is_absolute():
            path = root / path
        out.append(path)
    return out


def path_has_excluded_part(path: Path, root: Path) -> bool:
    try:
        rel_parts = path.resolve().relative_to(root.resolve()).parts
    except ValueError:
        rel_parts = path.parts

    for part in rel_parts:
        lower = part.lower()
        if lower in EXCLUDED_DIR_NAMES:
            return True
    return False


def looks_generated(path: Path) -> bool:
    lower_name = path.name.lower()
    if ".generated." in lower_name:
        return True
    if lower_name.endswith(("_generated.cpp", "_generated.hpp", "_generated.h")):
        return True
    return False


def is_source_file(path: Path, extensions: Set[str]) -> bool:
    return path.is_file() and path.suffix.lower() in extensions and not looks_generated(path)


def iter_source_files(paths: Sequence[Path], root: Path, extensions: Set[str]) -> Iterator[Path]:
    seen: Set[Path] = set()

    for path in paths:
        if path_has_excluded_part(path, root):
            continue

        if path.is_file():
            candidate = path.resolve()
            if is_source_file(candidate, extensions) and candidate not in seen:
                seen.add(candidate)
                yield candidate
            continue

        if not path.exists():
            print(f"warning: path does not exist: {display_path(path, root)}", file=sys.stderr)
            continue

        if not path.is_dir():
            continue

        for dirpath, dirnames, filenames in os.walk(path):
            current = Path(dirpath)
            dirnames[:] = [
                name
                for name in dirnames
                if name.lower() not in EXCLUDED_DIR_NAMES
                and not path_has_excluded_part(current / name, root)
            ]

            for filename in filenames:
                candidate = (current / filename).resolve()
                if path_has_excluded_part(candidate, root):
                    continue
                if is_source_file(candidate, extensions) and candidate not in seen:
                    seen.add(candidate)
                    yield candidate


def chunked(items: Sequence[Path], max_items: int = 100) -> Iterator[Sequence[Path]]:
    for index in range(0, len(items), max_items):
        yield items[index : index + max_items]


def supports_option(tool: str, option: str) -> bool:
    try:
        completed = subprocess.run(
            [tool, "--help"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
    except OSError:
        return False
    return option in completed.stdout


def read_nul_paths_from_stdin() -> List[str]:
    raw = sys.stdin.buffer.read()
    if not raw:
        return []
    return [os.fsdecode(part) for part in raw.split(b"\0") if part]
