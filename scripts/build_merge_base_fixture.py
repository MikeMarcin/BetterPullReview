#!/usr/bin/env python3
"""Constructs the committed BPR-1560 merge-base course. No remote data.

The owner's pull request showed changes from the base branch it did not make
and hid changes it did: Whole PR compared from the base commit GitHub records
for the pull request (`base.sha`), and that commit was ahead of the merge base
GitHub's Files changed compares from. This builds that shape exactly:

    M --- T          base branch: T is the recorded base (`base.sha`)
     \\
      H              the pull request's head

`T` changes a line of `a.txt` and of `shared.txt` the pull request does not
touch, adds and changes files the pull request never names, and lands the pull
request's own `b.txt` change separately. GitHub's Files changed (the recorded
files and patches here) is `M...H`; a comparison from `T` would show the base
branch's lines reverted and `b.txt` unchanged with the pull request's text on
both sides. The pack holds every object reachable from `T` and `H`, so either
basis can be served and only the comparison base decides what is shown.

    build_merge_base_fixture.py            writes construction, pack, contract
    build_merge_base_fixture.py <out.json> writes only the construction
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import build_nested_folders_fixture as nested
import build_recorded_pull_request_pack as pack
from contained_temporary_support import contained_temporary_directory


def numbered(name: str, count: int, changes: dict[int, str] | None = None) -> str:
    changes = changes or {}
    return "".join(f"{changes.get(line, f'{name} line {line}')}\n" for line in range(1, count + 1))


# (path, merge base M, head H). A merge base of None is an added file.
FILES: tuple[tuple[str, str | None, str], ...] = (
    ("a.txt", numbered("a", 60), numbered("a", 60, {2: "a line 2 PR-1", 12: "a line 12 PR-3"})),
    ("b.txt", numbered("b", 10), numbered("b", 10, {3: "b line 3 PR-2"})),
    ("new.txt", None, "added by the pull request\n"),
    ("shared.txt", numbered("shared", 60), numbered("shared", 60, {5: "shared line 5 PR-1"})),
)

# The recorded base T, written over M: the base branch moved on.
BASE_TIP: dict[str, str] = {
    "a.txt": numbered("a", 60, {50: "a line 50 MAIN-2"}),
    "b.txt": numbered("b", 10, {3: "b line 3 PR-2"}),
    "c.txt": numbered("c", 20, {10: "c line 10 MAIN-1"}),
    "main-only.txt": "only on the base branch\n",
    "shared.txt": numbered("shared", 60, {40: "shared line 40 MAIN-1", 55: "shared line 55 MAIN-2"}),
}
UNTOUCHED: dict[str, str] = {"c.txt": numbered("c", 20)}


def git(repository: str, *arguments: str) -> str:
    result = pack.run(repository, *arguments)
    if result.returncode != 0:
        raise RuntimeError(f"git {' '.join(arguments)}: {result.stderr}")
    return result.stdout.strip()


def write(repository: str, files: dict[str, str | None]) -> None:
    for path, text in files.items():
        target = Path(repository) / path
        if text is None:
            target.unlink(missing_ok=True)
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(text.encode("utf-8"))


def build(repository: str) -> tuple[str, str, str]:
    subprocess.run(["git", "init", "-q", "--initial-branch=main", repository], check=True, capture_output=True,
                   env={**os.environ, **pack.FIXTURE_ENVIRONMENT})
    write(repository, {**UNTOUCHED, **{path: base for path, base, _ in FILES}})
    git(repository, "add", "-A")
    git(repository, "commit", "-q", "-m", "merge base")
    merge_base = git(repository, "rev-parse", "HEAD")
    write(repository, {path: head for path, _, head in FILES})
    git(repository, "add", "-A")
    git(repository, "commit", "-q", "-m", "the pull request")
    head = git(repository, "rev-parse", "HEAD")
    git(repository, "checkout", "-q", merge_base)
    write(repository, {**{path: None for path, base, _ in FILES if base is None}, **BASE_TIP})
    git(repository, "add", "-A")
    git(repository, "commit", "-q", "-m", "the base branch moves on")
    tip = git(repository, "rev-parse", "HEAD")
    return merge_base, tip, head


def construction(merge_base: str, tip: str, head: str) -> dict:
    nested.FILES = FILES
    nested.BASE_OID = merge_base
    nested.HEAD_OID = head
    result = nested.construction()
    result["constructed"] = "Synthetic BPR-1560 merge-base course written by scripts/build_merge_base_fixture.py; not a recording"
    result["repository"] = {"name": "merge-base", "owner": "bpr-fixtures"}
    result["pull_request"].update(number=1560, title="Whole PR compares from the merge base",
                                  body="Synthetic course: the recorded base is ahead of the merge base.\n")
    result["pull_request"]["base"]["sha"] = tip
    result["pull_request"]["head"]["sha"] = head
    result["commits"][0].update(sha=head, parents=[merge_base], message="Whole PR compares from the merge base")
    result["merge_base"] = merge_base
    return result


def verify(result: dict, repository: str, merge_base: str, tip: str, head: str) -> list[str]:
    failures = pack.verify(result, repository, merge_base, head)
    two_dot = {line.split("\t")[2]: line.split("\t")[:2] for line in git(repository, "diff", "--numstat", tip, head).splitlines()}
    if two_dot.get("b.txt") is not None:
        failures.append("b.txt must not differ between the recorded base and the head")
    if two_dot.get("a.txt") == ["2", "2"]:
        failures.append("a.txt must differ two-dot by the base branch's line too")
    if git(repository, "merge-base", tip, head) != merge_base:
        failures.append("the merge base of the recorded base and the head is not M")
    return failures


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    fixture = root / "tests/fixtures/github/pull-requests/merge-base-v1"
    recording = Path(sys.argv[1]) if len(sys.argv) > 1 else fixture / "construction.json"
    with contained_temporary_directory() as repository:
        merge_base, tip, head = build(repository)
        result = construction(merge_base, tip, head)
        failures = verify(result, repository, merge_base, tip, head)
        if failures:
            for failure in failures:
                print(f"FAILED: {failure}", file=sys.stderr)
            return 1
        listing = git(repository, "rev-list", "--objects", tip, head)
        objects = subprocess.run(["git", "-C", repository, "pack-objects", "--stdout"], input=(listing + "\n").encode(), capture_output=True,
                                 env={**os.environ, **pack.FIXTURE_ENVIRONMENT}, check=True).stdout
    recording.parent.mkdir(parents=True, exist_ok=True)
    recording.write_text(json.dumps(result, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    if len(sys.argv) > 1:
        return 0
    (fixture / "objects.pack").write_bytes(objects)
    subprocess.run([sys.executable, str(root / "scripts/build_recorded_pull_request_contract.py"), str(recording),
                    str(root / "tests/fixtures/github/contracts/v1/merge-base-v1")], check=True)
    print(f"MERGE_BASE_FIXTURE_OK:merge_base={merge_base}:base={tip}:head={head}:{len(objects)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
