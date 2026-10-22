#!/usr/bin/env python3
"""Constructs the committed RFC-BPR-0089 commit-filter course. No remote data.

The owner's pull request (2026-10-20): one commit changed a generator's input,
a second regenerated hundreds of files from it, and a third made follow-ups
to both. This builds that shape on one first-parent chain from the merge base:

    M --- C1 --- C2 --- C3

`C1` changes `src/generator.in` and line 5 of `src/app.cpp`. `C2` regenerates
all 96 files under `gen/`. `C3` changes line 40 of `src/app.cpp` and edits
two generated files by hand. GitHub's Files changed is `M...C3`, 98 files:
one recorded page holds at most 100 records, so the course stays on one page
and the live BPR-Test course carries the owner's hundreds. The commit list
carries GitHub's `parents`, and the pack holds every object reachable from
`C3`, so each commit's tree diff can be read locally.

With `--split` it writes commit-filter-split-v1 instead (RFC-BPR-0089 M3): the
same chain with one more file, `src/version.txt`, that all three commits
change (C1 its version, C2 its stamp, C3 its version again), so hiding C2
splits it into C1's and C3's own diffs. Its pack also holds `C3r`, C3 reworded
on the same parent and tree: the course serves it as the next push, a force
push whose new head carries exactly C3's change. Files changed is 99 files.

With `--comment` it writes commit-filter-comment-v1 instead (RFC-BPR-0089 M4):
the same chain with one more file, `src/notes.txt`, of 30 lines, shaped as
BPR-Test #116 where GitHub was measured. C1 changes its lines 5, 15 and 25; C3
inserts two lines at its top and changes C1's lines 5 and 25 again. With C3
hidden, a comment on C1's line 15 goes to the head's line 17 and one on C1's
line 5 goes to C1. Its review threads hold one written on C1 at line 25,
outdated, and one at the head's line 17. Files changed is 99 files.

With `--attributes` it writes commit-filter-attributes-v1 instead ([[BPR-1692]]):
the same chain, whose merge base also commits a root `.gitattributes` marking
`gen/** linguist-generated` and a nested `gen/.gitattributes` returning
`item_007.txt` with `-linguist-generated`. Neither file changes in the pull
request, so its Files changed is the same 98 files and the head's rules mark
95 of C2's paths.

    build_commit_filter_fixture.py                 writes construction, pack, contract
    build_commit_filter_fixture.py <out.json>      writes only the construction
    build_commit_filter_fixture.py --root <dir>    writes all three under <dir>
    build_commit_filter_fixture.py --attributes [--root <dir> | <out.json>]
    build_commit_filter_fixture.py --split [--root <dir> | <out.json>]
    build_commit_filter_fixture.py --comment [--root <dir> | <out.json>]
"""

from __future__ import annotations

import base64
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_nested_folders_fixture as nested  # noqa: E402
import build_recorded_pull_request_pack as pack  # noqa: E402
from contained_temporary_support import contained_temporary_directory  # noqa: E402

GENERATED = 96
HAND_EDITED = (7, 8)
# [[BPR-1692]]. The head's rules: every regenerated file is generated, except
# one a nested file returns to normal.
ATTRIBUTES = {
    ".gitattributes": "gen/** linguist-generated\n",
    "gen/.gitattributes": "item_007.txt -linguist-generated\n",
}
VARIANTS = {
    "plain": {"name": "commit-filter-v1", "repository": "commit-filter", "number": 89},
    "attributes": {"name": "commit-filter-attributes-v1", "repository": "commit-filter-attributes", "number": 1692},
    "split": {"name": "commit-filter-split-v1", "repository": "commit-filter-split", "number": 893},
    "comment": {"name": "commit-filter-comment-v1", "repository": "commit-filter-comment", "number": 894},
}
# RFC-BPR-0089 M4. The file a comment is written on with C3 hidden.
NOTES_PATH = "src/notes.txt"


def comment_threads(commits: dict[str, str]) -> dict:
    """RFC-BPR-0089 M4. Two threads already on src/notes.txt, as GitHub
    reports them: one written on C1 at its line 25, which C3 changed, so GitHub
    keeps it on C1, outdated; and one written at the head at line 17."""
    def thread(identity: str, written: str, current: str | None, line: int | None, original: int) -> dict:
        post = {
            "id": f"PRRC_m4_{identity}",
            "bodyText": f"Already here: {identity}.",
            "path": NOTES_PATH,
            "line": line,
            "originalLine": original,
            "outdated": line is None,
            "author": {"login": "fixture-author", "id": "U_fixture-author"},
            "replyTo": None,
            "commit": {"oid": current or written},
            "originalCommit": {"oid": written},
        }
        return {
            "id": f"PRRT_m4_{identity}",
            "isResolved": False,
            "isOutdated": line is None,
            "viewerCanResolve": True,
            "viewerCanUnresolve": False,
            "subjectType": "LINE",
            "path": NOTES_PATH,
            "diffSide": "RIGHT",
            "line": line,
            "originalLine": original,
            "comments": {"totalCount": 1, "nodes": [post], "pageInfo": {"hasNextPage": False, "endCursor": cursor(post["id"])}},
        }
    nodes = [thread("on_c1", commits["C1"], None, None, 25), thread("at_head", commits["C3"], commits["C3"], 17, 17)]
    return {"nodes": nodes, "pageInfo": {"endCursor": cursor(nodes[-1]["id"]), "hasNextPage": False}, "totalCount": len(nodes)}


def cursor(text: str) -> str:
    return base64.b64encode(f"cursor:{text}".encode("ascii")).decode("ascii")


def notes(stage: str) -> str:
    lines = [f"notes line {n}" for n in range(1, 31)]
    if stage != "M":
        for line in (5, 15, 25):
            lines[line - 1] = f"notes line {line} C1"
    if stage == "C3":
        lines[4] = "notes line 5 C3"
        lines[24] = "notes line 25 C3"
        lines = ["inserted 1 C3", "inserted 2 C3"] + lines
    return "".join(f"{line}\n" for line in lines)
# RFC-BPR-0089 M3. The file every commit changes, so hiding C2 splits it.
SPLIT_PATH = "src/version.txt"
VERSION = {
    "M": "version 1\nstamp none\n",
    "C1": "version 2\nstamp none\n",
    "C2": "version 2\nstamp regenerated\n",
    "C3": "version 3\nstamp regenerated\n",
}
REWORDED = "Follow-ups, reworded"


def numbered(name: str, count: int, changes: dict[int, str] | None = None) -> str:
    changes = changes or {}
    return "".join(f"{changes.get(line, f'{name} line {line}')}\n" for line in range(1, count + 1))


def generated(value: int, index: int, adjusted: bool = False) -> str:
    note = f"item {index}, adjusted by hand\n" if adjusted else f"item {index}\n"
    return f"generated from value {value}\n{note}checksum {value * 1000 + index}\n"


def snapshot(stage: str, variant: str = "plain") -> dict[str, str]:
    """The tree at M, C1, C2 or C3."""
    attributes = variant == "attributes"
    value = 1 if stage == "M" else 2
    app = numbered("app", 60, {
        **({5: "app line 5 C1"} if stage != "M" else {}),
        **({40: "app line 40 C3"} if stage == "C3" else {}),
    })
    files = {
        "README.md": "A generator, its input and what it generates.\n",
        "src/app.cpp": app,
        "src/generator.in": f"value = {value}\n",
    }
    regenerated = 2 if stage in ("C2", "C3") else 1
    for index in range(GENERATED):
        files[f"gen/item_{index:03}.txt"] = generated(regenerated, index, stage == "C3" and index in HAND_EDITED)
    if attributes:
        files.update(ATTRIBUTES)
    if variant == "split":
        files[SPLIT_PATH] = VERSION[stage]
    if variant == "comment":
        files[NOTES_PATH] = notes(stage)
    return files


MESSAGES = {
    "C1": "Change the generator input",
    "C2": "Regenerate",
    "C3": "Follow-ups",
}


def git(repository: str, *arguments: str) -> str:
    result = pack.run(repository, *arguments)
    if result.returncode != 0:
        raise RuntimeError(f"git {' '.join(arguments)}: {result.stderr}")
    return result.stdout.strip()


def write(repository: str, files: dict[str, str]) -> None:
    for path, text in files.items():
        target = Path(repository) / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(text.encode("utf-8"))


def build(repository: str, variant: str = "plain") -> dict[str, str]:
    subprocess.run(["git", "init", "-q", "--initial-branch=main", repository], check=True, capture_output=True,
                   env={**os.environ, **pack.FIXTURE_ENVIRONMENT})
    commits: dict[str, str] = {}
    for stage in ("M", "C1", "C2", "C3"):
        write(repository, snapshot(stage, variant))
        git(repository, "add", "-A")
        git(repository, "commit", "-q", "-m", MESSAGES.get(stage, "merge base"))
        commits[stage] = git(repository, "rev-parse", "HEAD")
    if variant == "split":
        # The force push: C3 reworded, on C2 with C3's own tree.
        tree = git(repository, "rev-parse", f"{commits['C3']}^{{tree}}")
        commits["C3r"] = git(repository, "commit-tree", tree, "-p", commits["C2"], "-m", REWORDED)
    return commits


def construction(commits: dict[str, str], name: str = "plain") -> dict:
    variant = VARIANTS[name]
    base = snapshot("M", name)
    head = snapshot("C3", name)
    nested.FILES = tuple((path, base[path], head[path]) for path in sorted(head) if base.get(path) != head[path])
    nested.BASE_OID = commits["M"]
    nested.HEAD_OID = commits["C3"]
    result = nested.construction()
    result["constructed"] = f"Synthetic RFC-BPR-0089 {variant['repository']} course written by scripts/build_commit_filter_fixture.py; not a recording"
    result["repository"] = {"name": variant["repository"], "owner": "bpr-fixtures"}
    result["pull_request"].update(number=variant["number"], title="Change the generator input and regenerate",
                                  body="Synthetic course: a source change, a regeneration and follow-ups to both.\n", commits=3)
    result["pull_request"]["head"]["ref"] = variant["repository"]
    author = result["commits"][0]["author"]
    result["commits"] = [
        {"author": author, "message": MESSAGES["C1"], "parents": [commits["M"]], "sha": commits["C1"]},
        {"author": author, "message": MESSAGES["C2"], "parents": [commits["C1"]], "sha": commits["C2"]},
        {"author": author, "message": MESSAGES["C3"], "parents": [commits["C2"]], "sha": commits["C3"]},
    ]
    result["packed_history"] = True
    result["serve_parents"] = True
    result["merge_base"] = commits["M"]
    return result


def verify(result: dict, repository: str, commits: dict[str, str], variant: str = "plain") -> list[str]:
    failures = pack.verify(result, repository, commits["M"], commits["C3"])
    split = variant == "split"
    if variant == "attributes":
        # Git itself decides what the head's rules mark: 95 of C2's paths.
        paths = sorted(file["filename"] for file in result["files"])
        answered = subprocess.run(["git", "-C", repository, "check-attr", "--source", commits["C3"], "-z", "linguist-generated", "--", *paths],
                                  capture_output=True, check=True, env={**os.environ, **pack.FIXTURE_ENVIRONMENT}).stdout.decode().split("\0")
        states = dict(zip(answered[0::3], answered[2::3]))
        marked = sorted(path for path, state in states.items() if state == "set")
        expected = sorted(f"gen/item_{index:03}.txt" for index in range(GENERATED) if index != 7)
        if marked != expected or states.get("gen/item_007.txt") != "unset":
            failures.append(f"the head's rules must mark {len(expected)} files and unset gen/item_007.txt: {len(marked)} marked")
    extra = {SPLIT_PATH} if split else set()
    # RFC-BPR-0089 M4. The notes file C1 and C3 change and C2 does not.
    ends = extra | ({NOTES_PATH} if variant == "comment" else set())
    if len(result["files"]) != GENERATED + 2 + len(ends):
        failures.append(f"Whole PR must list {GENERATED + 2 + len(ends)} files, not {len(result['files'])}")
    touched = {
        stage: set(git(repository, "diff", "--name-only", f"{commits[stage]}^", commits[stage]).splitlines())
        for stage in ("C1", "C2", "C3")
    }
    if touched["C1"] != {"src/app.cpp", "src/generator.in", *ends}:
        failures.append(f"C1 must change exactly the source: {sorted(touched['C1'])}")
    if touched["C2"] - extra != {f"gen/item_{index:03}.txt" for index in range(GENERATED)} or not extra <= touched["C2"]:
        failures.append("C2 must regenerate exactly the generated files")
    if touched["C3"] != {"src/app.cpp", *(f"gen/item_{index:03}.txt" for index in HAND_EDITED), *ends}:
        failures.append(f"C3 must touch the source and the hand-edited files: {sorted(touched['C3'])}")
    if split:
        # The force push carries exactly C3's change: same parent, same tree.
        if git(repository, "rev-parse", f"{commits['C3r']}^", f"{commits['C3r']}^{{tree}}").splitlines() != [
                commits["C2"], git(repository, "rev-parse", f"{commits['C3']}^{{tree}}")]:
            failures.append("C3r must be C3 reworded on C2")
        # The synthesized tree a cherry-pick of C1 and C3 would make is in no commit.
        synthesized = "version 3\nstamp none\n"
        if synthesized in VERSION.values():
            failures.append("the synthesized src/version.txt must be absent from every commit")
    if git(repository, "rev-parse", f"{commits['C3']}^", f"{commits['C2']}^", f"{commits['C1']}^").splitlines() != [commits["C2"], commits["C1"], commits["M"]]:
        failures.append("the chain must be M, C1, C2, C3 by first parents")
    return failures


def main() -> int:
    source = Path(__file__).resolve().parents[1]
    arguments = sys.argv[1:]
    variant = "plain"
    if arguments[:1] in (["--attributes"], ["--split"], ["--comment"]):
        variant = arguments[0][2:]
        arguments = arguments[1:]
    name = VARIANTS[variant]["name"]
    root = source
    if arguments[:1] == ["--root"] and len(arguments) == 2:
        root = Path(arguments[1]).resolve()
        arguments = []
    fixture = root / "tests/fixtures/github/pull-requests" / name
    recording = Path(arguments[0]) if arguments else fixture / "construction.json"
    with contained_temporary_directory() as repository:
        commits = build(repository, variant)
        result = construction(commits, variant)
        failures = verify(result, repository, commits, variant)
        if failures:
            for failure in failures:
                print(f"FAILED: {failure}", file=sys.stderr)
            return 1
        heads = [commits["C3"]] + ([commits["C3r"]] if variant == "split" else [])
        listing = git(repository, "rev-list", "--objects", *heads)
        objects = subprocess.run(["git", "-C", repository, "pack-objects", "--stdout"], input=(listing + "\n").encode(), capture_output=True,
                                 env={**os.environ, **pack.FIXTURE_ENVIRONMENT}, check=True).stdout
    recording.parent.mkdir(parents=True, exist_ok=True)
    recording.write_text(json.dumps(result, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    if arguments:
        return 0
    (fixture / "objects.pack").write_bytes(objects)
    subprocess.run([sys.executable, str(source / "scripts/build_recorded_pull_request_contract.py"), str(recording),
                    str(root / "tests/fixtures/github/contracts/v1" / name)], check=True, capture_output=True)
    if variant == "comment":
        threads = root / "tests/fixtures/github/contracts/v1" / name / "responses/review_threads.json"
        document = json.loads(threads.read_text(encoding="utf-8"))
        document["data"]["repository"]["pullRequest"]["reviewThreads"] = comment_threads(commits)
        threads.write_text(json.dumps(document, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print("COMMIT_FILTER_FIXTURE_OK:" + ":".join(f"{stage}={sha}" for stage, sha in commits.items()) + f":{len(objects)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
