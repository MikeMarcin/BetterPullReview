#!/usr/bin/env python3
"""Writes and verifies the BPR-0880 fsck-severity pack fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
import zlib
from pathlib import Path

SCHEMA = "betterpullreview.git-fsck-severity-fixture.v1"
IDENTITY = (
    b"BetterPullReview Fixture <fixture@betterpullreview.invalid> 1704067200 +0000"
)
BLOB_CONTENT = b"fixture\n"

# The three packs, one per severity class Git's fsck reports for an object.
# `mode` is written into the tree byte-for-byte, so the zero-padded spelling
# survives into the object exactly as an old tool wrote it; Git's own `mktree`
# normalises it away and cannot build this fixture.
PACKS = {
    "warning-class": {
        "file": "warning-class.pack",
        "message_id": "zeroPaddedFilemode",
        "severity": "WARN",
        "entries": [("0100644", "fixture.txt")],
        "commit_message": b"Zero padded filemode fixture",
    },
    "information-class": {
        "file": "information-class.pack",
        "message_id": "gitignoreSymlink",
        "severity": "INFO",
        "entries": [("120000", ".gitignore")],
        "commit_message": b"Symlinked gitignore fixture",
    },
    "error-class": {
        "file": "error-class.pack",
        "message_id": "duplicateEntries",
        "severity": "ERROR",
        "entries": [("100644", "fixture.txt"), ("100644", "fixture.txt")],
        "commit_message": b"Duplicate entries fixture",
    },
}

OBJECT_TYPES = {"commit": 1, "tree": 2, "blob": 3}


def fail(message: str) -> None:
    raise SystemExit(f"fsck-severity-fixture: {message}")


def object_id(kind: str, payload: bytes) -> str:
    header = kind.encode() + b" " + str(len(payload)).encode() + b"\x00"
    return hashlib.sha1(header + payload).hexdigest()


def tree_payload(entries: list[tuple[str, str]], blob: str) -> bytes:
    result = b""
    for mode, name in entries:
        result += mode.encode() + b" " + name.encode() + b"\x00" + bytes.fromhex(blob)
    return result


def commit_payload(tree: str, message: bytes) -> bytes:
    return (
        b"tree "
        + tree.encode()
        + b"\nauthor "
        + IDENTITY
        + b"\ncommitter "
        + IDENTITY
        + b"\n\n"
        + message
        + b"\n"
    )


def build_objects(specification: dict) -> list[tuple[str, str, bytes]]:
    """Returns the pack's objects as (kind, oid, payload), in pack order."""
    blob = object_id("blob", BLOB_CONTENT)
    tree = tree_payload(specification["entries"], blob)
    commit = commit_payload(object_id("tree", tree), specification["commit_message"])
    return [
        ("commit", object_id("commit", commit), commit),
        ("tree", object_id("tree", tree), tree),
        ("blob", blob, BLOB_CONTENT),
    ]


def pack_bytes(objects: list[tuple[str, str, bytes]]) -> bytes:
    """Serialises a version 2 pack with no deltas."""
    result = b"PACK" + struct.pack(">II", 2, len(objects))
    for kind, _, payload in objects:
        size = len(payload)
        header = bytes([((OBJECT_TYPES[kind] << 4) | (size & 0x0F)) | (0x80 if size >> 4 else 0)])
        size >>= 4
        while size:
            header += bytes([(size & 0x7F) | (0x80 if size >> 7 else 0)])
            size >>= 7
        result += header + zlib.compress(payload, 9)
    return result + hashlib.sha1(result).digest()


def identities_for(specification: dict) -> dict:
    """The part of a pack record that no zlib or Git version can move."""
    objects = build_objects(specification)
    return {
        "file": specification["file"],
        "message_id": specification["message_id"],
        "severity": specification["severity"],
        "commit": objects[0][1],
        "tree": objects[1][1],
        "blob": objects[2][1],
    }


def write_fixture(directory: Path) -> dict:
    content: dict = {"schema": SCHEMA, "object_format": "sha1", "packs": {}}
    for name, specification in PACKS.items():
        data = pack_bytes(build_objects(specification))
        (directory / specification["file"]).write_bytes(data)
        content["packs"][name] = identities_for(specification) | {
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            # Git names a pack after the pack's own trailing checksum, so this
            # value belongs to the frozen bytes rather than to the recipe.
            "pack": data[-20:].hex(),
        }
    return content


def git_environment() -> dict:
    return {
        "PATH": "/usr/bin:/bin",
        "GIT_CONFIG_NOSYSTEM": "1",
        "GIT_CONFIG_GLOBAL": "/dev/null",
        "GIT_CONFIG_SYSTEM": "/dev/null",
        "GIT_TERMINAL_PROMPT": "0",
        "LC_ALL": "C",
    }


def import_pack(git: Path, repository: Path, data: bytes, *arguments: str):
    return subprocess.run(  # noqa: S603
        [str(git), f"--git-dir={repository}", "index-pack", "--stdin", "--threads=1", "--no-rev-index", *arguments],
        input=data,
        capture_output=True,
        check=False,
        env=git_environment(),
    )


def fresh_repository(git: Path, root: Path, name: str) -> Path:
    repository = root / name
    subprocess.run(  # noqa: S603
        [str(git), "init", "--quiet", "--bare", "--object-format=sha1", str(repository)],
        check=True,
        capture_output=True,
        env=git_environment(),
    )
    return repository


def git_version(git: Path) -> tuple[int, int, int]:
    reported = subprocess.run(  # noqa: S603
        [str(git), "--version"], capture_output=True, check=True, env=git_environment()
    ).stdout.decode()
    found = re.search(r"(\d+)\.(\d+)\.(\d+)", reported)
    if found is None:
        fail(f"cannot read a version out of {reported.strip()!r}")
    return (int(found.group(1)), int(found.group(2)), int(found.group(3)))


def policy_message_ids(source: Path, version: tuple[int, int, int]) -> list[str]:
    """Reads the closed ignore list out of the adapter that ships it.

    The adapter is the single source of truth: this rebuilds the same list
    from the same table, with the same version gate, rather than restating
    it, so the two cannot drift.
    """
    text = source.read_text(encoding="utf-8")
    block = re.search(
        r"BPR_0880_FSCK_MESSAGE_TABLE_BEGIN(.*?)BPR_0880_FSCK_MESSAGE_TABLE_END",
        text,
        re.DOTALL,
    )
    if block is None:
        fail(f"{source} carries no BPR_0880_FSCK_MESSAGE_TABLE block")
    declared = re.search(r"std::array<fsck_message,\s*(\d+)U>", block.group(1))
    if declared is None:
        fail(f"{source} does not declare the fsck message table's size")
    rows = re.findall(
        r'\{\s*"([A-Za-z][A-Za-z0-9]*)"\s*,\s*fsck_severity::(\w+)\s*,'
        r"\s*(\d+)U\s*,\s*(\d+)U\s*,\s*(\d+)U\s*\}",
        block.group(1),
    )
    # Every declared row must parse. A row this cannot read would silently
    # drop out of the probe below and take its guarantee with it.
    if len(rows) != int(declared.group(1)):
        fail(f"{source} declares {declared.group(1)} rows but {len(rows)} parse")
    identifiers = [
        identifier
        for identifier, severity, major, minor, patch in rows
        if severity in {"warn", "info"}
        and version >= (int(major), int(minor), int(patch))
    ]
    if not identifiers:
        fail(f"{source} ignores no message identifier at Git {version}")
    return identifiers


def verify(fixture: Path, git: Path, source: Path, temporary: Path) -> None:
    recorded = json.loads((fixture / "manifest.json").read_text(encoding="utf-8"))
    if recorded.get("schema") != SCHEMA:
        fail(f"manifest schema is {recorded.get('schema')!r}, not {SCHEMA!r}")
    for name, specification in PACKS.items():
        expected = identities_for(specification)
        actual = {key: recorded["packs"].get(name, {}).get(key) for key in expected}
        if actual != expected:
            fail(f"{name}: regenerating the recipe yields {expected}, not the recorded {actual}")

    version = git_version(git)
    ignored = policy_message_ids(source, version)
    for message_id in ignored:
        repository = fresh_repository(git, temporary, f"probe-{message_id}")
        probe = import_pack(git, repository, b"", f"--strict={message_id}=ignore")
        if b"Unhandled message id" in probe.stderr:
            fail(
                f"the installed Git does not know the shipped message id {message_id!r}: "
                f"{probe.stderr.decode(errors='replace').strip()}"
            )

    for name, specification in PACKS.items():
        record = recorded["packs"][name]
        data = (fixture / record["file"]).read_bytes()
        if len(data) != record["bytes"]:
            fail(f"{record['file']} is {len(data)} bytes, not {record['bytes']}")
        if hashlib.sha256(data).hexdigest() != record["sha256"]:
            fail(f"{record['file']} does not match its recorded SHA-256")
        if data[:4] != b"PACK":
            fail(f"{record['file']} has no PACK magic")
        if data[-20:].hex() != record["pack"]:
            fail(f"{record['file']} carries a checksum other than the recorded pack identity")

        strict = import_pack(
            git,
            fresh_repository(git, temporary, f"{name}-strict"),
            data,
            "--strict",
            "--check-self-contained-and-connected",
        )
        marker = f": {specification['message_id']}: ".encode()
        if marker not in strict.stderr:
            fail(
                f"{name}: a bare --strict import said nothing about "
                f"{specification['message_id']}, so the fixture proves nothing: "
                f"{strict.stderr.decode(errors='replace').strip()}"
            )
        # An information-class finding never made `index-pack` exit nonzero.
        # It wrote a `warning:` line instead, and an accepted exit that wrote
        # to standard error is malformed output to this adapter, so the
        # repository was just as unopenable. Both dead ends are the fixture.
        if (strict.returncode == 0) != (specification["severity"] == "INFO"):
            fail(
                f"{name}: a bare --strict import exited {strict.returncode}, which is "
                f"not how a {specification['severity']}-class finding reaches this adapter"
            )

        policy = ",".join(f"{identifier}=ignore" for identifier in ignored)
        paths = [("strict", (f"--strict={policy}", "--check-self-contained-and-connected"))]
        # `--fsck-objects` only learned to take a severity parameter in Git
        # 2.44; an older installation rejects the whole option, which is why
        # the adapter keeps the bare form there.
        if version >= (2, 44, 0):
            paths.append(("sparse", (f"--fsck-objects={policy}",)))
        for label, arguments in paths:
            applied = import_pack(
                git, fresh_repository(git, temporary, f"{name}-{label}"), data, *arguments
            )
            if specification["severity"] == "ERROR":
                if applied.returncode == 0:
                    fail(f"{name}: the {label} policy accepted an error-class object")
                if marker not in applied.stderr:
                    fail(f"{name}: the {label} refusal does not name {specification['message_id']}")
                continue
            if applied.returncode != 0:
                fail(
                    f"{name}: the {label} policy refused a "
                    f"{specification['severity'].lower()}-class object: "
                    f"{applied.stderr.decode(errors='replace').strip()}"
                )
            if applied.stderr:
                fail(
                    f"{name}: the {label} policy wrote to standard error, which BPR-0846 "
                    f"reads as malformed output: {applied.stderr.decode(errors='replace').strip()}"
                )
            if applied.stdout.decode() != f"pack\t{record['pack']}\n":
                fail(f"{name}: the {label} policy reported {applied.stdout!r}, not the recorded pack")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", type=Path, help="write the packs and manifest into this directory")
    parser.add_argument("--fixture", type=Path, help="verify the fixture in this directory")
    parser.add_argument("--git", type=Path, help="the Git executable to verify against")
    parser.add_argument("--source", type=Path, help="the adapter source carrying the closed ignore list")
    arguments = parser.parse_args()

    if arguments.write is not None:
        arguments.write.mkdir(parents=True, exist_ok=True)
        content = write_fixture(arguments.write)
        (arguments.write / "manifest.json").write_text(
            json.dumps(content, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        print(f"wrote {len(content['packs'])} packs to {arguments.write}")
        return

    if arguments.fixture is None or arguments.git is None or arguments.source is None:
        fail("verification needs --fixture, --git and --source")

    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from scripts.contained_temporary_support import (  # noqa: E402
        contained_temporary_directory,
    )

    with contained_temporary_directory(prefix="bpr-fsck-severity-") as temporary:
        verify(arguments.fixture, arguments.git, arguments.source, Path(temporary))
    print("FSCK_SEVERITY_FIXTURE_OK")


if __name__ == "__main__":
    main()
