#!/usr/bin/env python3
"""Checks the recorded public pull request is what it claims and nothing more.

A committed blob of somebody else's code with no provenance is impossible to
audit later, and a recording with a place to put a credential invites a later
fetch that fills it in. Both are asserted by reading the file rather than by
trusting the intent behind it. BPR-0187.
"""

from __future__ import annotations

import base64
import difflib
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
# Each recorded pull request as it was on its retrieval date. A re-fetch that
# drifted must fail here rather than change the content a presentation judgement
# was made against.
#
# `MinimumModificationPairs` is why there are two. A pure addition has no removed
# line to pair an added line against, so it cannot produce intraline emphasis at
# all -- BPR-0214. A recording that claims to carry that shape has to be held to
# it, or the next capture taken from it fails the same way and nobody knows why.
RECORDINGS = (
    {
        "Directory": "directxmath-336",
        "Source": "https://api.github.com/repos/microsoft/DirectXMath/pulls/336",
        "Head": "c5d86c3dfc28fc661cc03a151a3643f41da7a12d",
        "Base": "0332efa39ab80fd1417d5ba120abbde6373e15ed",
        "Number": 336,
        "Additions": 221,
        "Deletions": 0,
        "Files": 2,
        "Commits": 2,
        "MinimumModificationPairs": 0,
        "RequiresBlobs": False,
    },
    {
        "Directory": "uiforetw-74",
        "Source": "https://api.github.com/repos/google/UIforETW/pulls/74",
        "Head": "9b01c587b3e46637ff69c3801841993bd054f106",
        "Base": "7c519e97eb906bdc6b83c8f576c91c063d6ceee9",
        "Number": 74,
        "Additions": 46,
        "Deletions": 6,
        "Files": 5,
        "Commits": 1,
        "MinimumModificationPairs": 3,
        "RequiresBlobs": True,
    },
)

# Credential-shaped strings. Not an exhaustive list of every secret format, and
# not meant to be: an unauthenticated read has no credential to leak, so any
# match at all means the recording was produced by a path this card forbids.
FORBIDDEN = (
    "authorization",
    "set-cookie",
    "cookie",
    "x-ratelimit",
    "bearer ",
    "api_key",
    "apikey",
    "ghp_",
    "gho_",
    "github_pat_",
    "private_key",
    "client_secret",
)

FAILURES: list[str] = []


def expect(condition: bool, message: str) -> None:
    if not condition:
        FAILURES.append(message)


def modification_pairs(recording: dict) -> int:
    """Counts removed/added line pairs that are mostly the same text.

    Deliberately crude -- a shared prefix and suffix, not the product's own
    aligner. Asking the aligner whether the fixture can exercise the aligner
    would be circular; this only has to establish that the raw text contains
    pairs an aligner could reasonably match.
    """
    found = 0
    for entry in recording.get("files", []):
        lines = entry.get("patch", "").split("\n")
        for index in range(len(lines) - 1):
            removed, added = lines[index], lines[index + 1]
            if not (removed.startswith("-") and not removed.startswith("---")):
                continue
            if not (added.startswith("+") and not added.startswith("+++")):
                continue
            old_text, new_text = removed[1:], added[1:]
            if not old_text or not new_text or old_text == new_text:
                continue
            prefix = 0
            while (prefix < min(len(old_text), len(new_text))
                   and old_text[prefix] == new_text[prefix]):
                prefix += 1
            suffix = 0
            while (suffix < min(len(old_text), len(new_text)) - prefix
                   and old_text[-1 - suffix] == new_text[-1 - suffix]):
                suffix += 1
            if (prefix + suffix) / max(len(old_text), len(new_text)) > 0.5:
                found += 1
    return found


def check(expected: dict) -> None:
    path = ROOT / "tests/fixtures/github/pull-requests" / expected["Directory"] / "recording.json"
    name = expected["Directory"]
    expect(path.is_file(), f"{name}: the recording exists at {path}")
    if not path.is_file():
        return

    raw = path.read_text()
    lowered = raw.lower()
    for token in FORBIDDEN:
        expect(
            token not in lowered,
            f"{name}: the recording carries no {token!r}; an unauthenticated "
            "read has no credential to record, so a match means it was not one",
        )

    # URLs inside a patch are reviewed content -- the pull request's own source
    # may legitimately contain links. Only the recording's own fields are held
    # to carrying nothing but the provenance line.
    recording = json.loads(raw)
    patches = [entry.get("patch", "") for entry in recording.get("files", [])]
    fields = raw
    for patch in patches:
        fields = fields.replace(json.dumps(patch)[1:-1], "")
    urls = [url.rstrip('",') for url in re.findall(r"https?://\S+", fields)]
    expect(
        urls == [expected["Source"]],
        f"{name}: outside the recorded patches the only URL is the provenance "
        f"line; found {urls!r}",
    )

    expect(
        recording.get("recorded_from") == expected["Source"],
        f"{name}: the recording names where it came from",
    )
    expect(
        bool(recording.get("recorded_on")),
        f"{name}: the recording names when it was retrieved",
    )

    pull = recording.get("pull_request", {})
    expect(
        pull.get("head", {}).get("sha") == expected["Head"],
        f"{name}: the recorded head is {expected['Head']}; a re-fetch that "
        "drifted is a failure and not a silent content change",
    )
    expect(
        pull.get("base", {}).get("sha") == expected["Base"],
        f"{name}: the recorded base is {expected['Base']}",
    )
    expect(
        pull.get("number") == expected["Number"],
        f"{name}: the recorded pull request is #{expected['Number']}",
    )
    expect(
        pull.get("additions") == expected["Additions"]
        and pull.get("deletions") == expected["Deletions"],
        f"{name}: the recorded shape is "
        f"+{expected['Additions']} -{expected['Deletions']}",
    )

    files = recording.get("files", [])
    expect(
        len(files) == expected["Files"],
        f"{name}: the recording carries {expected['Files']} files, found {len(files)}",
    )
    for entry in files:
        expect(
            bool(entry.get("patch")),
            f"{name}: {entry.get('filename')!r} carries a patch, which is the "
            "whole point of recording it rather than writing one",
        )

    commits = recording.get("commits", [])
    expect(
        len(commits) == expected["Commits"],
        f"{name}: the recording carries {expected['Commits']} commits, "
        f"found {len(commits)}",
    )
    expect(
        any(commit.get("sha") == expected["Head"] for commit in commits),
        f"{name}: the head commit is among the recorded commits",
    )

    # The shape claim, held rather than described. A recording kept for
    # intraline emphasis that stopped carrying modification pairs would still
    # look like a valid recording, and the capture taken from it would silently
    # show nothing -- which is exactly how BPR-0214 went unnoticed.
    pairs = modification_pairs(recording)
    expect(
        pairs >= expected["MinimumModificationPairs"],
        f"{name}: carries at least {expected['MinimumModificationPairs']} "
        f"near-identical removed/added pairs, found {pairs}",
    )
    # Blobs, and blobs that actually agree with the recorded shape. The
    # comparison surface diffs real file content out of git objects, not the
    # API's patch text, so a recording without base and head contents cannot be
    # served at all -- a patch is hunks with context, not whole files. BPR-0187.
    #
    # Reconstructing the counts is the load-bearing half. Two base64 strings
    # that decode are not evidence of anything; two that diff to exactly the
    # additions and deletions the API reported are the same change the fixture
    # claims to carry.
    if expected["RequiresBlobs"]:
        for entry in recording.get("files", []):
            label = entry.get("filename")
            if entry.get("status") == "added":
                continue
            base_text = entry.get("base_content_base64")
            head_text = entry.get("head_content_base64")
            expect(
                bool(base_text) and bool(head_text),
                f"{name}: {label!r} carries base and head content, which is what "
                "a git object can be built from",
            )
            if not (base_text and head_text):
                continue
            before = base64.b64decode(base_text).decode("utf-8", "replace").splitlines()
            after = base64.b64decode(head_text).decode("utf-8", "replace").splitlines()
            adds = sum(
                1 for line in difflib.unified_diff(before, after, n=0)
                if line.startswith("+") and not line.startswith("+++")
            )
            dels = sum(
                1 for line in difflib.unified_diff(before, after, n=0)
                if line.startswith("-") and not line.startswith("---")
            )
            expect(
                adds == entry.get("additions") and dels == entry.get("deletions"),
                f"{name}: {label!r} blobs diff to "
                f"+{adds} -{dels}, and the recording says "
                f"+{entry.get('additions')} -{entry.get('deletions')}",
            )

    if expected["MinimumModificationPairs"] == 0:
        expect(
            pull.get("deletions") == 0,
            f"{name}: is recorded as a pure addition, so it cannot show "
            "intraline emphasis and must not be used to claim it does",
        )



# [[BPR-1213]]. Constructed pull requests: no provenance line to hold, but the
# same served-tree and pack checks, plus the construction itself regenerating
# to the same bytes and its blobs diffing to the counts it claims.
CONSTRUCTIONS = (
    {
        "Directory": "nested-folders-v1",
        "Generator": "scripts/build_nested_folders_fixture.py",
        "Files": 17,
        "Folders": 12,
    },
    # [[BPR-1290]]. The nested-folders files plus a long one; its served tree
    # also gains the generator's review threads.
    {
        "Directory": "inline-drawer-v1",
        "Generator": "scripts/build_inline_drawer_fixture.py",
        "Files": 18,
        "Folders": 13,
        "Threads": "scripts/build_inline_drawer_fixture.py",
    },
    # [[BPR-1293]]. Prototype v2's ten files; its served tree gains the
    # generator's twenty-one review threads.
    {
        "Directory": "conversations-v1",
        "Generator": "scripts/build_conversations_fixture.py",
        "Files": 10,
        "Folders": 5,
        "Threads": "scripts/build_conversations_fixture.py",
    },
)


def check_construction(expected: dict) -> None:
    name = expected["Directory"]
    path = ROOT / "tests/fixtures/github/pull-requests" / name / "construction.json"
    expect(path.is_file(), f"{name}: the construction exists at {path}")
    if not path.is_file():
        return
    with contained_temporary_directory() as directory:
        regenerated = Path(directory) / "construction.json"
        result = subprocess.run(
            [sys.executable, str(ROOT / expected["Generator"]), str(regenerated)],
            capture_output=True,
            text=True,
            check=False,
        )
        expect(
            result.returncode == 0 and regenerated.read_bytes() == path.read_bytes(),
            f"{name}: construction.json is exactly what {expected['Generator']} "
            "writes, so it was not edited by hand",
        )
    raw = path.read_text(encoding="utf-8")
    construction = json.loads(raw)
    expect(
        "recorded_from" not in construction and "recorded_on" not in construction
        and bool(construction.get("constructed")),
        f"{name}: a construction says so and makes no recording claim",
    )
    expect(
        re.findall(r"https?://", raw) == [],
        f"{name}: a construction names no URL",
    )
    lowered = raw.lower()
    for token in FORBIDDEN:
        expect(token not in lowered, f"{name}: the construction carries no {token!r}")
    files = construction.get("files", [])
    expect(len(files) == expected["Files"], f"{name}: carries {expected['Files']} files, found {len(files)}")
    folders = {entry["filename"].rpartition("/")[0] for entry in files}
    expect(
        len(folders) == expected["Folders"],
        f"{name}: its files sit in {expected['Folders']} folders, found {len(folders)}",
    )
    for entry in files:
        before = base64.b64decode(entry.get("base_content_base64", "")).decode("utf-8").splitlines()
        after = base64.b64decode(entry["head_content_base64"]).decode("utf-8").splitlines()
        diff = list(difflib.unified_diff(before, after, n=0))
        adds = sum(1 for line in diff if line.startswith("+") and not line.startswith("+++"))
        dels = sum(1 for line in diff if line.startswith("-") and not line.startswith("---"))
        expect(
            (adds, dels) == (entry["additions"], entry["deletions"])
            and (entry["status"] == "added") == ("base_content_base64" not in entry),
            f"{name}: {entry['filename']!r} blobs diff to +{adds} -{dels} as claimed",
        )


def check_generated_contract_matches(name: str, source: str = "recording.json", threads: str | None = None) -> None:
    """The served tree must be exactly what the generator writes today.

    The contract directory is generated from `recording.json`, so the two can
    drift the moment somebody edits a served response by hand to make something
    pass. Regenerating into a temporary directory and comparing bytes is the
    only check that notices; comparing shapes would not.
    """
    recording = ROOT / "tests/fixtures/github/pull-requests" / name / source
    committed = ROOT / "tests/fixtures/github/contracts/v1" / name
    if not committed.is_dir():
        return

    with contained_temporary_directory() as directory:
        regenerated = Path(directory) / name
        result = subprocess.run(
            [
                sys.executable,
                str(ROOT / "scripts/build_recorded_pull_request_contract.py"),
                str(recording),
                str(regenerated),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            FAILURES.append(
                f"{name}: the generator refuses its own {source}: "
                f"{result.stderr.strip()}"
            )
            return
        if threads is not None:
            # [[BPR-1290]]. A construction whose served tree also carries
            # threads regenerates them with its own generator.
            result = subprocess.run(
                [sys.executable, str(ROOT / threads), "--threads", str(regenerated)],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode != 0:
                FAILURES.append(f"{name}: the threads generator fails: {result.stderr.strip()}")
                return

        committed_files = {
            path.relative_to(committed)
            for path in committed.rglob("*")
            if path.is_file()
        }
        regenerated_files = {
            path.relative_to(regenerated)
            for path in regenerated.rglob("*")
            if path.is_file()
        }
        if committed_files != regenerated_files:
            FAILURES.append(
                f"{name}: the committed contract tree and a freshly generated "
                f"one hold different files: "
                f"{sorted(map(str, committed_files ^ regenerated_files))}"
            )
            return
        for relative in sorted(committed_files):
            if (committed / relative).read_bytes() != (
                regenerated / relative
            ).read_bytes():
                FAILURES.append(
                    f"{name}: {relative} differs from what the generator "
                    f"writes from {source} - the served tree was edited "
                    f"by hand, or the generator changed without regenerating"
                )


def check_pack(name: str) -> None:
    """The git objects the comparison is actually built from."""
    pack = ROOT / "tests/fixtures/github/pull-requests" / name / "objects.pack"
    if not pack.is_file():
        return
    header = pack.read_bytes()[:4]
    if header != b"PACK":
        FAILURES.append(
            f"{name}: objects.pack does not begin with PACK, so it is not a "
            f"packfile and index-pack would refuse it"
        )


def main() -> int:
    for expected in RECORDINGS:
        check(expected)
        check_generated_contract_matches(expected["Directory"])
        check_pack(expected["Directory"])
    for expected in CONSTRUCTIONS:
        check_construction(expected)
        check_generated_contract_matches(expected["Directory"], "construction.json", expected.get("Threads"))
        check_pack(expected["Directory"])
        expect(
            (ROOT / "tests/fixtures/github/contracts/v1" / expected["Directory"]).is_dir(),
            f"{expected['Directory']}: the served contract tree exists",
        )

    for failure in FAILURES:
        print(f"FAILED: {failure}", file=sys.stderr)
    if FAILURES:
        return 1
    print("RECORDED_FIXTURE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
