#!/usr/bin/env python3
from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterable, Iterator, List, Optional, Sequence, Set, TextIO, Tuple


# The root `.clang-format` is rendered by clang-format 22 and later. Release 21
# and earlier strip the space out of an empty braced initializer -- they emit
# `{}` where this style file's own sources hold `{ }` -- and no setting restores
# it, because `SpaceInEmptyBraces` is not a key those releases accept. The
# difference is not cosmetic to the gate: on this tree it makes an older tool
# report several hundred files dirty that a supported tool reports clean, and it
# would make `format.py` rewrite every one of them.
MINIMUM_CLANG_FORMAT_VERSION: Tuple[int, ...] = (22,)

CLANG_FORMAT_VERSION_PATTERN = re.compile(r"(\d+)(?:\.(\d+))?(?:\.(\d+))?")

UNSUPPORTED_CLANG_FORMAT_OVERRIDE = "--allow-unsupported-clang-format"


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


def version_text(version: Sequence[int]) -> str:
    return ".".join(str(part) for part in version)


def clang_format_version(tool: str) -> Optional[Tuple[int, ...]]:
    """Return the release version clang-format reports, or None when it cannot be read.

    Every distribution prefixes the banner differently -- `Homebrew clang-format
    version 22.1.8`, `Ubuntu clang-format version 21.1.8`, `Apple clang-format
    version 16.0.0 (clang-1600.0.26.6)` -- so the first dotted number after the
    banner is what is parsed rather than a fixed field position.
    """
    try:
        completed = subprocess.run(
            [tool, "--version"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
    except OSError:
        return None
    if completed.returncode != 0:
        return None

    match = CLANG_FORMAT_VERSION_PATTERN.search(completed.stdout)
    if match is None:
        return None
    return tuple(int(part) for part in match.groups() if part is not None)


def check_clang_format_version(
    tool: str,
    *,
    allow_unsupported: bool = False,
    mutating: bool = False,
    stream: Optional[TextIO] = None,
) -> Tuple[bool, Optional[Tuple[int, ...]]]:
    """Report whether `tool` is a clang-format this project's style file supports.

    Returns `(usable, version)`. `usable` is False only when the caller must
    stop: an unsupported or unreadable version without `allow_unsupported`. With
    `allow_unsupported` the diagnostic is still written, because a verdict from
    an unsupported tool is advisory whether or not the caller wanted it.
    """
    out = stream if stream is not None else sys.stderr
    version = clang_format_version(tool)
    minimum = version_text(MINIMUM_CLANG_FORMAT_VERSION)

    if version is None:
        reason = f"cannot read a version from {tool}"
    elif version >= MINIMUM_CLANG_FORMAT_VERSION:
        return True, version
    else:
        reason = f"{tool} reports version {version_text(version)}"

    print(f"error: unsupported clang-format: {reason}", file=out)
    print(
        f"       This project's .clang-format requires clang-format {minimum} or newer."
        " Releases before"
        f" {minimum} render an empty braced initializer as `{{}}` where this tree holds `{{ }}`,",
        file=out,
    )
    if mutating:
        print(
            "       so an older tool would rewrite hundreds of files that a supported tool"
            " reports clean.",
            file=out,
        )
    else:
        print(
            "       so an older tool reports hundreds of files dirty that a supported tool"
            " reports clean.",
            file=out,
        )
    print(
        f"       Install clang-format {minimum} or newer, or pass"
        f" {UNSUPPORTED_CLANG_FORMAT_OVERRIDE} to run anyway and treat the result as advisory.",
        file=out,
    )

    if allow_unsupported:
        print(
            f"warning: continuing because {UNSUPPORTED_CLANG_FORMAT_OVERRIDE} was passed;"
            " this result is not a gate verdict.",
            file=out,
        )
        return True, version
    return False, version


def add_clang_format_version_argument(parser: "object") -> None:
    """Register the shared override flag on an `argparse.ArgumentParser`."""
    parser.add_argument(  # type: ignore[attr-defined]
        UNSUPPORTED_CLANG_FORMAT_OVERRIDE,
        action="store_true",
        help=(
            "Run even when clang-format is older than the version this project's style file"
            " requires. The result is advisory rather than a gate verdict."
        ),
    )


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
