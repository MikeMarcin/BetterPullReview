#!/usr/bin/env python3
"""Say whether one clang-format can return a verdict for this tree.

Exits 0 and prints the version when the tool resolves and reports major release
REQUIRED_CLANG_FORMAT_MAJOR. Exits 1 with the gate's own diagnostic otherwise.
CMake's discovery asks this of every candidate it finds, so configure and the
gate share one pinned release and one banner parser rather than two.
"""
from __future__ import annotations

import argparse
import sys
from typing import Optional, Sequence

from _coding_standard import check_clang_format_version, resolve_tool, version_text


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("clang_format", help="clang-format executable path or PATH name.")
    args = parser.parse_args(argv)

    try:
        tool = resolve_tool(args.clang_format)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    usable, version = check_clang_format_version(tool)
    if not usable or version is None:
        return 1
    print(version_text(version))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
