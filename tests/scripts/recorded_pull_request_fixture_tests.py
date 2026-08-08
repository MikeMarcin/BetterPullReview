#!/usr/bin/env python3
"""Checks the recorded public pull request is what it claims and nothing more.

A committed blob of somebody else's code with no provenance is impossible to
audit later, and a recording with a place to put a credential invites a later
fetch that fills it in. Both are asserted by reading the file rather than by
trusting the intent behind it. BPR-0187.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RECORDING = (
    ROOT / "tests/fixtures/github/pull-requests/directxmath-336/recording.json"
)

# The pull request as it was on the retrieval date. A re-fetch that drifted must
# fail here rather than change the content a presentation judgement was made
# against.
EXPECTED_HEAD = "c5d86c3dfc28fc661cc03a151a3643f41da7a12d"
EXPECTED_BASE = "0332efa39ab80fd1417d5ba120abbde6373e15ed"
EXPECTED_SOURCE = "https://api.github.com/repos/microsoft/DirectXMath/pulls/336"

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


def main() -> int:
    expect(RECORDING.is_file(), f"the recording exists at {RECORDING}")
    if not RECORDING.is_file():
        print("FAILED: the recording is missing", file=sys.stderr)
        return 1

    raw = RECORDING.read_text()
    lowered = raw.lower()
    for token in FORBIDDEN:
        expect(
            token not in lowered,
            f"the recording carries no {token!r}; an unauthenticated read has "
            "no credential to record, so a match means it was not one",
        )

    urls = [url.rstrip('",') for url in re.findall(r"https?://\S+", raw)]
    expect(
        urls == [EXPECTED_SOURCE],
        f"the recording contains exactly one URL and it is the provenance "
        f"line; found {urls!r}",
    )

    recording = json.loads(raw)
    expect(
        recording.get("recorded_from") == EXPECTED_SOURCE,
        "the recording names where it came from",
    )
    expect(
        bool(recording.get("recorded_on")),
        "the recording names when it was retrieved",
    )

    pull = recording.get("pull_request", {})
    expect(
        pull.get("head", {}).get("sha") == EXPECTED_HEAD,
        f"the recorded head is {EXPECTED_HEAD}; a re-fetch that drifted is a "
        "failure and not a silent content change",
    )
    expect(
        pull.get("base", {}).get("sha") == EXPECTED_BASE,
        f"the recorded base is {EXPECTED_BASE}",
    )
    expect(pull.get("number") == 336, "the recorded pull request is #336")
    expect(pull.get("additions") == 221 and pull.get("deletions") == 0,
           "the recorded shape is +221 -0")

    files = recording.get("files", [])
    expect(len(files) == 2, f"the recording carries two files, found {len(files)}")
    for entry in files:
        expect(
            bool(entry.get("patch")),
            f"{entry.get('filename')!r} carries a patch, which is the whole "
            "point of recording it rather than writing one",
        )

    commits = recording.get("commits", [])
    expect(len(commits) == 2, f"the recording carries two commits, found {len(commits)}")
    expect(
        any(commit.get("sha") == EXPECTED_HEAD for commit in commits),
        "the head commit is among the recorded commits",
    )

    for failure in FAILURES:
        print(f"FAILED: {failure}", file=sys.stderr)
    if FAILURES:
        return 1
    print("RECORDED_FIXTURE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
