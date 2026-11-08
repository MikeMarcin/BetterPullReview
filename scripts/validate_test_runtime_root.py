#!/usr/bin/env python3
"""[[BPR-1942]]. Validate an explicitly designated test directory without changing it."""

import argparse
import os
from pathlib import Path
import stat
import sys


def validate_runtime_root(value: str, source: Path, build: Path, home: Path | None = None) -> Path | None:
    if value == "":
        return None
    if os.name != "posix":
        raise ValueError("the optional runtime root is supported only on POSIX hosts")
    if ";" in value or any(ord(character) < 32 or ord(character) == 127 for character in value):
        raise ValueError("list separators and control characters are not allowed")
    root = Path(value)
    if not root.is_absolute():
        raise ValueError("an absolute directory is required")
    try:
        information = root.lstat()
        canonical = root.resolve(strict=True)
    except (OSError, RuntimeError) as error:
        raise ValueError("an existing directory is required") from error
    if not stat.S_ISDIR(information.st_mode) or value != str(canonical):
        raise ValueError("the directory must be canonical and contain no symbolic links")
    if information.st_uid != os.geteuid() or stat.S_IMODE(information.st_mode) != 0o700:
        raise ValueError("the current user must own the directory with mode 0700")
    for protected in (source.resolve(), build.resolve()):
        if root == protected or root in protected.parents or protected in root.parents:
            raise ValueError("the runtime directory must be separate from source and build trees")
    home = (home if home is not None else Path.home()).resolve()
    if root == home or root in home.parents:
        raise ValueError("the home directory or its ancestors cannot be test runtime roots")
    return root


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--build-root", required=True, type=Path)
    arguments = parser.parse_args()
    try:
        validate_runtime_root(arguments.root, arguments.source_root, arguments.build_root)
    except ValueError as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
