#!/usr/bin/env python3
"""[[BPR-1560]] Compares BetterPullReview's default comparison with GitHub's
Files changed for one MikeMarcin/BPR-Test pull request.

`test_live_second_account_course --course parity --root DIR --pull N` writes
DIR/parity.jsonl: one line per file the application visited, with the blob
each side read and every removed (left) and added (right) line. This script
reads GitHub's own answer for the same pull request through `gh`: the files
list with its patches, and the merge base the compare API names. A file
matches when the application shows exactly the file set, the merge base's
blob on the left, the head's blob on the right, and exactly the removed and
added lines GitHub's patch has.

Run by hand on macOS after the course; it reads GitHub and writes nothing
there:

    python3 tests/end_to_end/bpr1560_live_parity.py --root DIR --pull N

It exits 0 when every file matches and 1 otherwise, printing each
difference. `--expect-mismatch` inverts the verdict for the negative control
(the build that still compares from the recorded base tip).
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPOSITORY = "MikeMarcin/BPR-Test"
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")


def gh(path: str, paginate: bool = False) -> object:
    output = subprocess.run(
        ["gh", "api", "--paginate", "--slurp", path] if paginate else ["gh", "api", path],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    value = json.loads(output)
    if paginate:
        merged: list[object] = []
        for page in value:
            merged.extend(page)
        return merged
    return value


def patch_lines(patch: str | None) -> tuple[list[list[object]], list[list[object]]]:
    removed: list[list[object]] = []
    added: list[list[object]] = []
    if not patch:
        return removed, added
    old = new = 0
    for line in patch.split("\n"):
        header = HUNK.match(line)
        if header:
            old = int(header.group(1))
            new = int(header.group(3))
            continue
        if line.startswith("\\"):
            continue
        if line.startswith("-"):
            removed.append([old, line[1:]])
            old += 1
        elif line.startswith("+"):
            added.append([new, line[1:]])
            new += 1
        else:
            old += 1
            new += 1
    return removed, added


def tree_blobs(commit: str) -> dict[str, str]:
    tree = gh(f"repos/{REPOSITORY}/git/trees/{commit}?recursive=1")
    if tree.get("truncated"):
        raise SystemExit(f"tree of {commit} is truncated; use a smaller course")
    return {entry["path"]: entry["sha"] for entry in tree["tree"] if entry["type"] == "blob"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--pull", type=int, required=True)
    parser.add_argument("--expect-mismatch", action="store_true")
    arguments = parser.parse_args()

    shown = [json.loads(line) for line in (arguments.root / "parity.jsonl").read_text().splitlines() if line.strip()]
    pull = gh(f"repos/{REPOSITORY}/pulls/{arguments.pull}")
    base, head = pull["base"]["sha"], pull["head"]["sha"]
    merge_base = gh(f"repos/{REPOSITORY}/compare/{base}...{head}?per_page=1&page=2")["merge_base_commit"]["sha"]
    files = gh(f"repos/{REPOSITORY}/pulls/{arguments.pull}/files?per_page=100", paginate=True)
    left_tree, right_tree = tree_blobs(merge_base), tree_blobs(head)
    print(f"github: base.sha={base} merge_base={merge_base} head={head} files={len(files)}")

    differences: list[str] = []
    by_path = {entry["path"]: entry for entry in shown}
    expected_paths = [entry["filename"] for entry in files]
    if sorted(by_path) != sorted(expected_paths):
        differences.append(f"file set: shown {sorted(by_path)} github {sorted(expected_paths)}")
    for entry in files:
        path = entry["filename"]
        app = by_path.get(path)
        if app is None:
            continue
        previous = entry.get("previous_filename", path)
        left = left_tree.get(previous) if entry["status"] != "added" else None
        right = right_tree.get(path) if entry["status"] != "removed" else None
        removed, added = patch_lines(entry.get("patch"))
        facts = []
        if app["left_blob"] != left:
            facts.append(f"left blob {app['left_blob']} != merge base {left}")
        if app["right_blob"] != right:
            facts.append(f"right blob {app['right_blob']} != head {right}")
        if app["left_blob"] is not None and app["left_blob"] == app["right_blob"]:
            facts.append("both sides show the same blob: no change shown")
        if "patch" in entry and app["removed"] != removed:
            facts.append(f"removed lines {app['removed']} != github {removed}")
        if "patch" in entry and app["added"] != added:
            facts.append(f"added lines {app['added']} != github {added}")
        verdict = "MATCH" if not facts else "DIFFERS"
        print(f"  {verdict} {entry['status']} {path} github +{entry['additions']} -{entry['deletions']} shown +{len(app['added'])} -{len(app['removed'])}")
        for fact in facts:
            print(f"      {fact}")
            differences.append(f"{path}: {fact}")
    if arguments.expect_mismatch:
        print("negative control:", "reproduced the mismatch" if differences else "NO mismatch (control failed)")
        return 0 if differences else 1
    print("parity:", "PASS" if not differences else f"FAIL ({len(differences)} differences)")
    return 0 if not differences else 1


if __name__ == "__main__":
    sys.exit(main())
