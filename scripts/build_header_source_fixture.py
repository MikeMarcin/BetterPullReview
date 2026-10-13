#!/usr/bin/env python3
"""Write RFC-BPR-0074's synthetic changed-file catalogue (no remote data).

Usage: build_header_source_fixture.py <construction.json>
Regenerate the contract and pack with the existing recorded-pull-request builders.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import build_nested_folders_fixture as nested

BASE_OID = "71dc98a8939742f51fa98dd44936ba727c3e5796"
HEAD_OID = "24537f5b9f51511d4ee94c420192f04a972e7548"
PATHS = (
    "src/widget.cpp", "inc/widget.h",
    "src/a/b/c/closest.cpp", "inc/project/a/b/c/closest.h", "inc/a/b/d/closest.h",
    "src/local/pair.cc", "src/local/pair.HPP", "inc/local/pair.h",
    "src/tie/ambiguous.cpp", "inc/one/ambiguous.h", "inc/two/ambiguous.hpp",
    "src/lonely.cpp", "inc/alone.h", "README.md",
    "src/missing.cpp", "inc/missing.h",
)


def construction() -> dict:
    nested.FILES = tuple((path, None if path == "inc/missing.h" else f"// {path}\nint value = 1;\n", f"// {path}\nint value = 2;\n") for path in PATHS)
    nested.BASE_OID = BASE_OID
    nested.HEAD_OID = HEAD_OID
    result = nested.construction()
    result["constructed"] = "Synthetic BPR-1416 header/source course; no recording or remote data"
    result["repository"] = {"name": "header-source", "owner": "bpr-fixtures"}
    result["commits"][0]["message"] = "Update header and source counterparts"
    request = result["pull_request"]
    request["number"] = 74
    request["head"]["ref"] = "header-source"
    request["title"] = "Update header and source counterparts"
    request["body"] = "Constructed cross-folder, closest-directory and ambiguous pairs.\n"
    return result


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(construction(), indent="\t", sort_keys=True) + "\n", encoding="utf-8")
    print(f"CONSTRUCTION_OK:{len(PATHS)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
