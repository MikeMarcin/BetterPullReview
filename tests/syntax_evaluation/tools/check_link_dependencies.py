#!/usr/bin/env python3
"""Checks where tree-sitter is acquired and linked in a configured build tree.

With BPR_BUILD_SYNTAX_EVALUATION off, the tree must hold no tree-sitter
target, archive, stage or link input at all. With it on, only targets under
tests/syntax_evaluation may link the tree-sitter archives, and the shipped
syntax adapter archive must define no tree-sitter symbol.

Usage: python3 check_link_dependencies.py --build BUILD
Prints a JSON verdict and exits nonzero when the rule is broken.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

TREE_SITTER = re.compile(r"tree[-_]sitter", re.IGNORECASE)


def option(build: Path) -> bool:
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("BPR_BUILD_SYNTAX_EVALUATION:"):
            return line.split("=", 1)[1].strip().upper() in {"ON", "1", "TRUE", "YES"}
    return False


def link_statements(build: Path) -> list[tuple[str, str]]:
    """(output, inputs) for every link edge in build.ninja."""
    edges = []
    current = None
    for line in (build / "build.ninja").read_text(encoding="utf-8").splitlines():
        if line.startswith("build ") and ("_LINKER__" in line or "STATIC_LIBRARY_LINKER" in line):
            output, _, rest = line[len("build "):].partition(":")
            current = (output.strip(), rest)
            edges.append(current)
    return edges


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build", type=Path, required=True)
    arguments = parser.parse_args()
    build = arguments.build.resolve()
    enabled = option(build)
    edges = link_statements(build)
    linking = sorted(output for output, inputs in edges if TREE_SITTER.search(inputs) or TREE_SITTER.search(output))
    outside = [output for output in linking if not output.startswith("tests/syntax_evaluation/")]
    stage = (build / "_bpr" / "tree-sitter").exists()
    adapter = build / "src/adapters/syntax/libbetter_pull_review_adapter_syntax.a"
    adapter_symbols = []
    if adapter.exists():
        names = subprocess.run(["nm", "-g", str(adapter)], capture_output=True, text=True).stdout
        adapter_symbols = sorted({line.split()[-1] for line in names.splitlines() if re.search(r"\b_?ts_[a-z_]+$", line) and " U " not in line})
    verdict = {
        "build": str(build), "option": enabled, "tree_sitter_stage_present": stage,
        "edges_linking_tree_sitter": len(linking), "edges_outside_evaluation": outside,
        "adapter_archive": str(adapter) if adapter.exists() else None, "adapter_tree_sitter_symbols": adapter_symbols,
    }
    if enabled:
        verdict["ok"] = not outside and not adapter_symbols
    else:
        verdict["ok"] = not linking and not stage and not adapter_symbols
    print(json.dumps(verdict, indent=1))
    return 0 if verdict["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
