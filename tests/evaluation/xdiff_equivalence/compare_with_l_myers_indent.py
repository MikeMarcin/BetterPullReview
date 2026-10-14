#!/usr/bin/env python3
"""[[BPR-1485]] AC2 equivalence: runs tool_diff_xdiff_equivalence over the
RFC-BPR-0082 evaluation corpus and compares every pair with the evaluation's
recorded `l-myers-indent` outcome (docs/verification/RFC-BPR-0082/macOS-L-raw.json):
segments, pairs and intraline digests, publication and the approximate flag,
and, for the 78 pairs with a recorded GitHub patch, GitHub agreement.

Usage: compare_with_l_myers_indent.py TOOL CORPUS_DIRECTORY RAW_JSON
Exits non-zero on any difference.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


def main() -> int:
    tool, corpus, raw = (Path(argument) for argument in sys.argv[1:4])
    manifest = json.loads((corpus / "manifest.json").read_text(encoding="utf-8"))
    recorded = json.loads(raw.read_text(encoding="utf-8"))["outcomes"]
    lines = []
    for pair in manifest["pairs"]:
        oracle = pair.get("oracle")
        hunks = ";".join(",".join(str(value) for value in hunk) for hunk in oracle["hunks"]) if oracle else "-"
        lines.append("\t".join((pair["id"], pair["options"], str(corpus / pair["left"]), str(corpus / pair["right"]), hunks or "-")))
    completed = subprocess.run([str(tool)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True)
    differences = []
    compared = github_pairs = github_agree = 0
    for row in completed.stdout.splitlines():
        identity, reason, published, segments, pairs, intraline, approximate, github = row.split("\t")
        expected = recorded[f"l-myers-indent|{identity}"]
        compared += 1
        actual = {"published": published == "1", "reason": reason, "segments_digest": segments, "pairs_digest": pairs,
                  "intraline_digest": intraline, "approximate": approximate == "1"}
        for key, value in actual.items():
            if key in expected and expected[key] != value:
                differences.append(f"{identity}: {key} {value!r} != recorded {expected[key]!r}")
        if github != "-":
            github_pairs += 1
            github_agree += github == "1"
            if (github == "1") != expected.get("oracle_exact_github_join"):
                differences.append(f"{identity}: GitHub agreement {github} != recorded {expected.get('oracle_exact_github_join')}")
    print(f"pairs compared: {compared} of {len(manifest['pairs'])}")
    print(f"segments, pairs and intraline digests equal to l-myers-indent: {compared - len({d.split(':')[0] for d in differences})} of {compared}")
    print(f"GitHub hunk agreement: {github_agree} of {github_pairs}")
    for difference in differences:
        print("DIFFERENCE " + difference)
    return 1 if differences or compared != len(manifest["pairs"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
