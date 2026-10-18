#!/usr/bin/env python3
"""Constructs the committed BPR-1572 stacked merge-base course. No remote data.

A stacked pull request whose base branch took one of its commits by fast
forward while GitHub kept the pull request's recorded base (`base.sha`):

    M --- A1 --- B1 --- B2     A1: the recorded base (`base.sha`)
                 ^      ^      B1: the base branch's tip (fast-forwarded)
               branch  head    B2: the pull request's head

GitHub's Files changed, measured on github.com (2026-10-17), still compares
from the merge base of the recorded base and the head (A1...B2: x.txt and
y.txt); the base branch's merge base B1 would hide x.txt's change, showing the
pull request's text on both sides. The pack holds every object reachable from
B2 (which contains A1 and B1).

    build_stacked_merge_base_fixture.py                 writes construction, pack, contract
    build_stacked_merge_base_fixture.py <out.json>      writes only the construction
    build_stacked_merge_base_fixture.py --root <dir>    writes all three under <dir>
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_merge_base_fixture as single  # noqa: E402
import build_nested_folders_fixture as nested  # noqa: E402
import build_recorded_pull_request_pack as pack  # noqa: E402
from contained_temporary_support import contained_temporary_directory  # noqa: E402

numbered = single.numbered
git = single.git
write = single.write

# (path, recorded base A1, head B2): GitHub's Files changed.
FILES: tuple[tuple[str, str | None, str], ...] = (
    ("x.txt", numbered("x", 30), numbered("x", 30, {3: "x line 3 B-1"})),
    ("y.txt", numbered("y", 30), numbered("y", 30, {3: "y line 3 B-2"})),
)
FORK: dict[str, str] = {"w.txt": numbered("w", 30), **{path: base for path, base, _ in FILES}}
RECORDED: dict[str, str] = {"w.txt": numbered("w", 30, {1: "w line 1 A-1"})}


def build(repository: str) -> dict[str, str]:
    subprocess.run(["git", "init", "-q", "--initial-branch=main", repository], check=True, capture_output=True,
                   env={**os.environ, **pack.FIXTURE_ENVIRONMENT})
    commits: dict[str, str] = {}
    for label, files, message in (("fork", FORK, "the fork point"), ("recorded", RECORDED, "the base branch's own change"),
                                  ("tip", {"x.txt": FILES[0][2]}, "the pull request's first commit"),
                                  ("head", {"y.txt": FILES[1][2]}, "the pull request's second commit")):
        write(repository, files)
        git(repository, "add", "-A")
        git(repository, "commit", "-q", "-m", message)
        commits[label] = git(repository, "rev-parse", "HEAD")
    return commits


def construction(commits: dict[str, str]) -> dict:
    nested.FILES = FILES
    nested.BASE_OID = commits["recorded"]
    nested.HEAD_OID = commits["head"]
    result = nested.construction()
    result["constructed"] = "Synthetic BPR-1572 stacked merge-base course written by scripts/build_stacked_merge_base_fixture.py; not a recording"
    result["repository"] = {"name": "stacked-merge-base", "owner": "bpr-fixtures"}
    result["pull_request"].update(number=1572, title="A stacked pull request whose base branch took its first commit", commits=2,
                                  body="Synthetic course: the base branch fast-forwarded to the head's first commit.\n")
    result["pull_request"]["base"].update(ref="feature-a", sha=commits["recorded"])
    result["pull_request"]["head"].update(ref="feature-b", sha=commits["head"])
    author = result["commits"][0]["author"]
    result["commits"] = [
        {"author": author, "message": "the pull request's first commit", "parents": [commits["recorded"]], "sha": commits["tip"]},
        {"author": author, "message": "the pull request's second commit", "parents": [commits["tip"]], "sha": commits["head"]},
    ]
    # Every commit listed is in this builder's pack (`rev-list --objects` of
    # the head), not the one-head pack `build_recorded_pull_request_pack.py`
    # writes.
    result["packed_history"] = True
    result["merge_base"] = commits["recorded"]
    result["branch_tip"] = commits["tip"]
    return result


def verify(result: dict, repository: str, commits: dict[str, str]) -> list[str]:
    failures = pack.verify(result, repository, commits["recorded"], commits["head"])
    if git(repository, "merge-base", commits["recorded"], commits["head"]) != commits["recorded"]:
        failures.append("the merge base of the recorded base and the head is not the recorded base")
    if git(repository, "merge-base", commits["tip"], commits["head"]) != commits["tip"]:
        failures.append("the base branch's merge base is not its tip")
    from_tip = {line.split("\t")[2] for line in git(repository, "diff", "--numstat", commits["tip"], commits["head"]).splitlines()}
    if "x.txt" in from_tip:
        failures.append("x.txt must not differ between the base branch's merge base and the head")
    return failures


def main() -> int:
    source = Path(__file__).resolve().parents[1]
    arguments = sys.argv[1:]
    root = source
    if arguments[:1] == ["--root"] and len(arguments) == 2:
        root = Path(arguments[1]).resolve()
        arguments = []
    fixture = root / "tests/fixtures/github/pull-requests/stacked-merge-base-v1"
    recording = Path(arguments[0]) if arguments else fixture / "construction.json"
    with contained_temporary_directory() as repository:
        commits = build(repository)
        result = construction(commits)
        failures = verify(result, repository, commits)
        if failures:
            for failure in failures:
                print(f"FAILED: {failure}", file=sys.stderr)
            return 1
        listing = git(repository, "rev-list", "--objects", commits["head"])
        objects = subprocess.run(["git", "-C", repository, "pack-objects", "--stdout"], input=(listing + "\n").encode(), capture_output=True,
                                 env={**os.environ, **pack.FIXTURE_ENVIRONMENT}, check=True).stdout
    recording.parent.mkdir(parents=True, exist_ok=True)
    recording.write_text(json.dumps(result, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    if arguments:
        return 0
    (fixture / "objects.pack").write_bytes(objects)
    subprocess.run([sys.executable, str(source / "scripts/build_recorded_pull_request_contract.py"), str(recording),
                    str(root / "tests/fixtures/github/contracts/v1/stacked-merge-base-v1")], check=True, capture_output=True)
    print(f"STACKED_MERGE_BASE_FIXTURE_OK:recorded={commits['recorded']}:tip={commits['tip']}:head={commits['head']}:{len(objects)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
