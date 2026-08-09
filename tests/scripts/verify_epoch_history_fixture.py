#!/usr/bin/env python3
"""Verify the fixed public epoch graph, packs, and non-fast-forward topology."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def fail(message: str) -> None:
    raise SystemExit(f"epoch-history-fixture: {message}")


def run(
    git: Path,
    repository: Path,
    *arguments: str,
    input_bytes: bytes | None = None,
    expected: int = 0,
) -> subprocess.CompletedProcess[bytes]:
    environment = {
        "GIT_CONFIG_NOSYSTEM": "1",
        "GIT_CONFIG_GLOBAL": os.devnull,
        "GIT_CONFIG_SYSTEM": os.devnull,
        "GIT_TERMINAL_PROMPT": "0",
        "LC_ALL": "C",
        "PATH": os.environ.get("PATH", ""),
    }
    completed = subprocess.run(
        [str(git), f"--git-dir={repository}", *arguments],
        input=input_bytes,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=environment,
        check=False,
    )
    if completed.returncode != expected:
        fail(
            f"git {' '.join(arguments)} returned {completed.returncode}, "
            f"expected {expected}: {completed.stderr.decode(errors='replace')}"
        )
    return completed


def frozen_pack(source: str, name: str) -> bytes:
    match = re.search(
        rf"\b{re.escape(name)}\s*\{{\s*"
        r"QByteArray::fromBase64\(QByteArrayLiteral\(\s*"
        r'"([A-Za-z0-9+/=]+)"\s*\)\)\s*\}',
        source,
    )
    if match is None:
        fail(f"cannot locate frozen pack {name}")
    return base64.b64decode(match.group(1), validate=True)


def import_pack(git: Path, repository: Path, pack: bytes) -> None:
    run(git, repository, "init", "--bare")
    run(
        git,
        repository,
        "index-pack",
        "--stdin",
        "--strict",
        "--check-self-contained-and-connected",
        "--threads=1",
        "--no-rev-index",
        input_bytes=pack,
    )


def require_object(git: Path, repository: Path, oid: str, expected_type: str) -> None:
    actual = run(git, repository, "cat-file", "-t", oid).stdout.decode().strip()
    if actual != expected_type:
        fail(f"{oid} has type {actual!r}, expected {expected_type!r}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--git", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    arguments = parser.parse_args()

    manifest = json.loads(arguments.manifest.read_text(encoding="utf-8"))
    source = arguments.source.read_text(encoding="utf-8")
    if manifest.get("schema") != "betterpullreview.git-epoch-history-fixture.v1":
        fail("unexpected manifest schema")
    if manifest.get("object_format") != "sha1":
        fail("fixture object format drifted")
    if manifest.get("repository") != {
        "owner": "octo-org",
        "name": "review-repository",
        "pull_request": 17,
        "base_ref": "refs/heads/main",
    }:
        fail("fixture repository authority drifted")

    objects = manifest["objects"]
    packs = manifest["packs"]
    frozen = {
        "initial": frozen_pack(source, "PackOne"),
        "force_push": frozen_pack(source, "PackTwo"),
    }
    for name, pack in frozen.items():
        expected = packs[name]
        if len(pack) != expected["bytes"]:
            fail(f"{name} pack size drifted")
        if hashlib.sha256(pack).hexdigest() != expected["sha256"]:
            fail(f"{name} pack digest drifted")
        if not pack.startswith(b"PACK"):
            fail(f"{name} is not a Git pack")

    # This manifest pins the synthetic three-commit graph, and only the
    # fixtures below serve it. A recorded pull request serves its own commits
    # from its own pack, so scraping the whole file would compare this manifest
    # against whichever identifiers the regex happened to see last. The
    # descriptor blocks are read individually and only these two are checked.
    #
    # The identifiers used to be `std::string Base = ...` members on
    # `deterministic_git_fixture`; they are per-fixture descriptor initialisers
    # now. It is still the C++ source being held against the manifest, which is
    # the point: what the product serves and what this fixture claims cannot
    # drift apart without one of them being edited.
    synthetic = ("github-com", "ghes-3.18")
    expected_oids = {
        "Base": objects["base"]["oid"],
        "HeadOne": objects["head_one"]["oid"],
        "HeadTwo": objects["head_two"]["oid"],
    }

    blocks = re.split(r"case\s+github_public_path_fixture::", source)[1:]
    if not blocks:
        fail(
            "no fixture descriptors found in the source; this contract reads "
            "the descriptor switch and the switch has moved or been renamed"
        )

    checked: list[str] = []
    for block in blocks:
        directory = re.search(
            r'\.ContractDirectory\s*=\s*"([^"]+)"',
            block,
        )
        if directory is None or directory.group(1) not in synthetic:
            continue
        found = {
            field.removesuffix("Oid"): oid
            for field, oid in re.findall(
                r'\.(BaseOid|HeadOneOid|HeadTwoOid)\s*=\s*"([0-9a-f]{40})"',
                block,
            )
        }
        if found != expected_oids:
            fail(
                f"fixture source OIDs disagree with the manifest for "
                f"{directory.group(1)}"
            )
        checked.append(directory.group(1))

    if sorted(checked) != sorted(synthetic):
        fail(
            f"expected the synthetic graph to be served by {sorted(synthetic)} "
            f"but the descriptor switch carries it for {sorted(checked)}"
        )
    if packs["initial"].get("head") != expected_oids["HeadOne"]:
        fail("initial pack head drifted")
    if packs["force_push"].get("head") != expected_oids["HeadTwo"]:
        fail("replacement pack head drifted")
    if packs["force_push"].get("contains_old_head") is not False:
        fail("replacement pack must declare old-head omission")

    with tempfile.TemporaryDirectory(prefix="bpr-epoch-history-") as temporary:
        root = Path(temporary)
        initial = root / "initial.git"
        replacement = root / "replacement.git"
        combined = root / "combined.git"
        import_pack(arguments.git, initial, frozen["initial"])
        import_pack(arguments.git, replacement, frozen["force_push"])
        import_pack(arguments.git, combined, frozen["initial"])
        run(
            arguments.git,
            combined,
            "index-pack",
            "--stdin",
            "--strict",
            "--check-self-contained-and-connected",
            "--threads=1",
            "--no-rev-index",
            input_bytes=frozen["force_push"],
        )

        base = objects["base"]["oid"]
        head_one = objects["head_one"]["oid"]
        head_two = objects["head_two"]["oid"]
        for repository, head in ((initial, head_one), (replacement, head_two)):
            require_object(arguments.git, repository, base, "commit")
            require_object(arguments.git, repository, head, "commit")
        run(arguments.git, replacement, "cat-file", "-e", head_one, expected=1)
        run(arguments.git, combined, "merge-base", "--is-ancestor", base, head_one)
        run(arguments.git, combined, "merge-base", "--is-ancestor", base, head_two)
        run(
            arguments.git,
            combined,
            "merge-base",
            "--is-ancestor",
            head_one,
            head_two,
            expected=1,
        )
        run(
            arguments.git,
            combined,
            "merge-base",
            "--is-ancestor",
            head_two,
            head_one,
            expected=1,
        )

        for name, record in objects.items():
            require_object(arguments.git, combined, record["oid"], "commit")
            require_object(arguments.git, combined, record["tree"], "tree")
            require_object(arguments.git, combined, record["blob"], "blob")
            content = run(
                arguments.git,
                combined,
                "cat-file",
                "blob",
                record["blob"],
            ).stdout
            if content.decode() != record["content"]:
                fail(f"{name} blob content drifted")
            tree = run(
                arguments.git,
                combined,
                "cat-file",
                "-p",
                record["tree"],
            ).stdout.decode()
            expected_tree = f"100644 blob {record['blob']}\treview.cpp\n"
            if tree != expected_tree:
                fail(f"{name} tree content drifted")
            commit = run(
                arguments.git,
                combined,
                "cat-file",
                "commit",
                record["oid"],
            ).stdout.decode()
            parent = record["parent"]
            fixture_identity = (
                "BetterPullReview Fixture <fixture@betterpullreview.invalid> "
                f"{record['timestamp']} +0000"
            )
            expected_commit = f"tree {record['tree']}\n"
            if parent:
                expected_commit += f"parent {parent}\n"
            expected_commit += (
                f"author {fixture_identity}\n"
                f"committer {fixture_identity}\n\n"
                f"{record['message']}"
            )
            if commit != expected_commit:
                fail(f"{name} commit record drifted")

    print("EPOCH_HISTORY_FIXTURE_OK")


if __name__ == "__main__":
    main()
