#!/usr/bin/env python3
from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterable, Iterator, List, Optional, Sequence, Set, TextIO, Tuple


# The root `.clang-format` is rendered by clang-format 22, and only by 22
# ([[BPR-1352]]). The style file does not pin a renderer; the tree does. Release
# 21 and earlier strip the space out of an empty braced initializer -- they emit
# `{}` where this tree holds `{ }` -- and no setting restores it, because
# `SpaceInEmptyBraces` is not a key those releases accept. Release 23 reads the
# same file and renders it differently again: it sorts some include blocks in
# another order, aligns runs of consecutive assignments differently and indents
# a designated initializer's continuation lines differently. A file formatted by
# one major release fails the other's check, so a gate that accepts a range of
# releases gives two stations two verdicts on one tree. Homebrew's default
# `llvm` moved to 23 while Linux stayed on 22, and main went red on Linux.
REQUIRED_CLANG_FORMAT_MAJOR: int = 22

# Where a clang-format 22 is installed when the station's default is another
# release. The discovery in `find_clang_format` and the CMake lookup in
# `cmake/CodingStandardClangFormat.cmake` search these in this order before
# PATH's `clang-format`; `coding_standard_format_gate_tests.py` holds the two
# lists together.
PINNED_CLANG_FORMAT_NAMES: Sequence[str] = (
    f"clang-format-{REQUIRED_CLANG_FORMAT_MAJOR}",
    "clang-format",
)
PINNED_CLANG_FORMAT_DIRECTORIES: Sequence[str] = (
    # Homebrew on Apple silicon and on Intel: `brew install llvm@22`. The
    # formula is keg-only, so its tools are never on PATH by default.
    f"/opt/homebrew/opt/llvm@{REQUIRED_CLANG_FORMAT_MAJOR}/bin",
    f"/usr/local/opt/llvm@{REQUIRED_CLANG_FORMAT_MAJOR}/bin",
    # Debian and Ubuntu: `apt install clang-format-22` installs here and links
    # /usr/bin/clang-format-22.
    f"/usr/lib/llvm-{REQUIRED_CLANG_FORMAT_MAJOR}/bin",
)

# How to get the required release, printed by every refusal.
CLANG_FORMAT_INSTALL_HINTS: Sequence[str] = (
    f"macOS: brew install llvm@{REQUIRED_CLANG_FORMAT_MAJOR}"
    f"  (found at /opt/homebrew/opt/llvm@{REQUIRED_CLANG_FORMAT_MAJOR}/bin/clang-format)",
    f"Debian/Ubuntu: sudo apt install clang-format-{REQUIRED_CLANG_FORMAT_MAJOR}"
    f"  (found as clang-format-{REQUIRED_CLANG_FORMAT_MAJOR})",
    f"Windows: install LLVM {REQUIRED_CLANG_FORMAT_MAJOR}.x, or use a Visual Studio whose"
    " bundled LLVM is 22.x, and name it with --clang-format or"
    " -DBPR_CLANG_FORMAT_EXECUTABLE",
)

# Anchored on the banner text rather than "the first number in the output". Every
# distribution's banner contains this phrase, and some put digits before it: the
# Android NDK prints `Android (10552028, +pgo, …) clang-format version 17.0.2`,
# which an unanchored search reads as version 10552028 -- comfortably above any
# minimum, and a release that cannot parse this project's style file at all. A
# banner without the phrase is an unreadable version, which fails closed.
CLANG_FORMAT_VERSION_PATTERN = re.compile(
    r"clang-format version\s+(\d+)(?:\.(\d+))?(?:\.(\d+))?"
)

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


def is_required_clang_format(version: Optional[Sequence[int]]) -> bool:
    """True for a version whose major release is the one this tree is rendered by."""
    return version is not None and len(version) > 0 and version[0] == REQUIRED_CLANG_FORMAT_MAJOR


def unsupported_clang_format_reason(tool: str, version: Optional[Sequence[int]]) -> str:
    """Name the tool and what it reported, for a refusal or a skip."""
    if version is None:
        return f"cannot read a version from {tool}"
    return f"{tool} reports version {version_text(version)}"


def _executable(path: Path) -> bool:
    return path.is_file() and os.access(str(path), os.X_OK)


def clang_format_candidates() -> List[str]:
    """Every clang-format this station offers, in the order the gate prefers them.

    `clang-format-22` on PATH, then `clang-format` in each pinned directory, then
    `clang-format` on PATH. Duplicates reached through another name or a symlink
    appear once.
    """
    found: List[str] = []
    seen: Set[str] = set()

    def add(candidate: Optional[str]) -> None:
        if candidate is None:
            return
        key = os.path.realpath(candidate)
        if key in seen:
            return
        seen.add(key)
        found.append(candidate)

    pinned_name, plain_name = PINNED_CLANG_FORMAT_NAMES
    add(shutil.which(pinned_name))
    suffix = ".exe" if os.name == "nt" else ""
    for directory in PINNED_CLANG_FORMAT_DIRECTORIES:
        for name in PINNED_CLANG_FORMAT_NAMES:
            candidate = Path(directory) / f"{name}{suffix}"
            if _executable(candidate):
                add(str(candidate))
    add(shutil.which(plain_name))
    return found


def find_clang_format() -> str:
    """Return the clang-format the gate runs when the caller names none.

    The first candidate that reports major release 22. When none does, the one
    PATH gives (or, without one, the first found), so the version refusal names
    the tool a person would otherwise have run. Raises FileNotFoundError, naming
    how to install 22, when the station has no clang-format at all.
    """
    candidates = clang_format_candidates()
    for candidate in candidates:
        if is_required_clang_format(clang_format_version(candidate)):
            return candidate
    on_path = shutil.which(PINNED_CLANG_FORMAT_NAMES[1])
    if on_path is not None:
        return on_path
    if candidates:
        return candidates[0]
    hints = "; ".join(CLANG_FORMAT_INSTALL_HINTS)
    raise FileNotFoundError(
        f"no clang-format found; this project needs clang-format {REQUIRED_CLANG_FORMAT_MAJOR}."
        f" {hints}"
    )


def resolve_clang_format(name_or_path: Optional[str]) -> str:
    """The tool a format script runs: the one named, else `find_clang_format()`."""
    if name_or_path is None:
        return find_clang_format()
    return resolve_tool(name_or_path)


def check_clang_format_version(
    tool: str,
    *,
    allow_unsupported: bool = False,
    mutating: bool = False,
    stream: Optional[TextIO] = None,
) -> Tuple[bool, Optional[Tuple[int, ...]]]:
    """Report whether `tool` is the clang-format release this tree is rendered by.

    Returns `(usable, version)`. `usable` is False only when the caller must
    stop: another major release or an unreadable version without
    `allow_unsupported`. With `allow_unsupported` the diagnostic is still
    written, because a verdict from another release is advisory whether or not
    the caller wanted it.
    """
    out = stream if stream is not None else sys.stderr
    version = clang_format_version(tool)
    if is_required_clang_format(version):
        return True, version

    required = REQUIRED_CLANG_FORMAT_MAJOR
    print(
        f"error: unsupported clang-format: {unsupported_clang_format_reason(tool, version)}",
        file=out,
    )
    print(
        f"       This project's .clang-format is rendered by clang-format {required} exactly."
        f" Other major releases format the same file differently: {required - 1} and earlier"
        " write `{}` where this tree holds `{ }`, and 23 sorts includes, aligns consecutive"
        " assignments and indents designated-initializer continuations differently,",
        file=out,
    )
    if mutating:
        print(
            f"       so another release would rewrite files that clang-format {required}"
            " reports clean.",
            file=out,
        )
    else:
        print(
            f"       so another release reports files dirty that clang-format {required}"
            " reports clean.",
            file=out,
        )
    print(f"       Install clang-format {required}; the gate looks for it first:", file=out)
    for hint in CLANG_FORMAT_INSTALL_HINTS:
        print(f"         {hint}", file=out)
    print(
        f"       Or pass --clang-format <path to a clang-format {required}>, or"
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
            "Run even when clang-format is not the major release this project's tree is"
            f" rendered by ({REQUIRED_CLANG_FORMAT_MAJOR}). The result is advisory rather than"
            " a gate verdict."
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
