#!/usr/bin/env python3
"""Builds a git pack from a recorded pull request's blobs.

BPR-0187. The comparison surface diffs real file content out of git objects,
not the GitHub API's patch text: the served fixture's `responses/files.json`
carries no patch at all, and the content reaches the product as a packfile
imported through the installed-Git adapter. So a recorded pull request cannot be
served until its blobs are git objects.

This turns `recording.json` into a two-commit repository — base, then head — and
packs it. It is a generator rather than a build step: the pack is committed
alongside the recording so a test run neither needs `git` to produce one nor
depends on `git` producing byte-identical output across versions.

The verification is the point. A pack that builds is not evidence; a pack whose
diff reproduces exactly the additions and deletions the recording claims is the
same change the fixture says it carries. That check runs here and refuses to
write a pack that fails it.

Usage:
    build_recorded_pull_request_pack.py <recording.json> <output.pack>
"""

from __future__ import annotations

import base64
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

# Fixed identity and timestamps. A pack whose object identifiers changed every
# time it was regenerated could not be pinned by a test, and "the fixture
# changed" would be indistinguishable from "the recording changed".
FIXTURE_ENVIRONMENT = {
    "GIT_AUTHOR_NAME": "BetterPullReview Fixture",
    "GIT_AUTHOR_EMAIL": "fixture@invalid",
    "GIT_COMMITTER_NAME": "BetterPullReview Fixture",
    "GIT_COMMITTER_EMAIL": "fixture@invalid",
    "GIT_AUTHOR_DATE": "2020-01-01T00:00:00+0000",
    "GIT_COMMITTER_DATE": "2020-01-01T00:00:00+0000",
}


def run(repository: str, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-c", "core.autocrlf=false", "-C", repository, *arguments],
        capture_output=True,
        text=True,
        env={**os.environ, **FIXTURE_ENVIRONMENT},
        check=False,
    )


def populate(repository: str, files: list[dict], side: str) -> None:
    """Writes one side of the change into the working tree."""
    for entry in files:
        content = entry.get(f"{side}_content_base64")
        if content is None:
            continue
        target = Path(repository) / entry["filename"]
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(base64.b64decode(content))


def build(recording: dict, repository: str) -> tuple[str, str]:
    subprocess.run(
        ["git", "init", "-q", "--initial-branch=main", repository],
        check=True,
        capture_output=True,
        env={**os.environ, **FIXTURE_ENVIRONMENT},
    )
    files = recording["files"]

    populate(repository, files, "base")
    run(repository, "add", "-A")
    run(repository, "commit", "-q", "-m", "base")
    base = run(repository, "rev-parse", "HEAD").stdout.strip()

    populate(repository, files, "head")
    run(repository, "add", "-A")
    run(repository, "commit", "-q", "-m", "head")
    head = run(repository, "rev-parse", "HEAD").stdout.strip()
    return base, head


def verify(recording: dict, repository: str, base: str, head: str) -> list[str]:
    """Requires the built objects to reproduce the recorded shape exactly."""
    failures: list[str] = []
    observed: dict[str, tuple[int, int]] = {}
    for line in run(repository, "diff", "--numstat", base, head).stdout.splitlines():
        added, removed, name = line.split("\t", 2)
        observed[name] = (int(added), int(removed))

    for entry in recording["files"]:
        name = entry["filename"]
        if name not in observed:
            failures.append(f"{name} is absent from the built comparison")
            continue
        added, removed = observed[name]
        if (added, removed) != (entry["additions"], entry["deletions"]):
            failures.append(
                f"{name} builds to +{added} -{removed}, and the recording says "
                f"+{entry['additions']} -{entry['deletions']}"
            )
    extra = set(observed) - {entry["filename"] for entry in recording["files"]}
    for name in sorted(extra):
        failures.append(f"{name} appears in the built comparison but not the recording")
    return failures


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    recording = json.loads(Path(sys.argv[1]).read_text())
    output = Path(sys.argv[2])

    with contained_temporary_directory() as repository:
        base, head = build(recording, repository)
        failures = verify(recording, repository, base, head)
        if failures:
            for failure in failures:
                print(f"FAILED: {failure}", file=sys.stderr)
            print(
                "PACK_NOT_WRITTEN: a pack that does not reproduce the recording "
                "would be a fixture claiming to be a change it is not",
                file=sys.stderr,
            )
            return 1

        listing = run(repository, "rev-list", "--objects", head)
        pack = subprocess.run(
            ["git", "-C", repository, "pack-objects", "--stdout"],
            input=listing.stdout.encode(),
            capture_output=True,
            text=False,
            env={**os.environ, **FIXTURE_ENVIRONMENT},
            check=True,
        ).stdout
        if not pack.startswith(b"PACK"):
            print("FAILED: pack-objects produced no pack", file=sys.stderr)
            return 1
        output.write_bytes(pack)

    print(f"PACK_OK:{base}:{head}:{len(pack)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
