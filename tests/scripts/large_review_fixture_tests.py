#!/usr/bin/env python3
"""Independently inspect stored large-review inputs with real local Git."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.build_large_review_fixture import LocalGit  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests/fixtures/github/pull-requests/large-review-v1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def read_blobs(git: LocalGit, objects: list[str]) -> dict[str, bytes]:
    raw = git.run("cat-file", "--batch", data=("\n".join(objects) + "\n").encode())
    result = {}
    offset = 0
    for oid in objects:
        end = raw.index(b"\n", offset)
        identity, kind, size = raw[offset:end].split()
        require(identity.decode() == oid and kind == b"blob", "Git returned a different object")
        start = end + 1
        stop = start + int(size)
        require(raw[stop:stop + 1] == b"\n", "malformed Git blob batch")
        result[oid] = raw[start:stop]
        offset = stop + 1
    require(offset == len(raw), "unexpected trailing Git blob response")
    return result


def inspect_fixture(fixture: Path, executable: Path) -> None:
    manifest = json.loads((fixture / "manifest.json").read_text())
    recipe = json.loads((fixture / "recipe.json").read_text())
    require(manifest["schema"] == "bpr.large-review-fixture.v1" and manifest["recipe"] == recipe,
            "fixture recipe disagrees with manifest")
    require((recipe["source_files"], recipe["source_lines"], recipe["large_file_lines"],
             recipe["generated_file_lines"], recipe["binary_bytes"], recipe["head_revisions"],
             recipe["review_comments"], recipe["comments_per_page"])
            == (512, 8, 20000, 8192, 4096, 32, 1000, 100), "version-one workload shape drifted")
    pack = (fixture / "objects.pack").read_bytes()
    require(len(pack) == manifest["pack_bytes"] and hashlib.sha256(pack).hexdigest() == manifest["pack_sha256"],
            "stored pack bytes drifted")
    expected_paths = {f"src/file_{index:04d}.cpp" for index in range(512)} | {
        "large/comparison.cpp", "generated/table.inc", "assets/data.bin"}
    file_entries = {entry["path"]: entry for entry in manifest["files"]}
    require(len(manifest["files"]) == 515 and set(file_entries) == expected_paths, "wrong file inventory")
    base, heads = manifest["base"], manifest["heads"]
    require(len(heads) == 32 and len(set([base, *heads])) == 33, "duplicate or missing revision identity")
    require(all(len(oid) == 40 and all(c in "0123456789abcdef" for c in oid) for oid in [base, *heads]),
            "invalid object identity")
    with contained_temporary_directory(prefix="bpr-large-review-contract-") as raw:
        git = LocalGit(executable, Path(raw).resolve())
        git.run("index-pack", "--stdin", "--strict", data=pack)
        git.run("update-ref", "refs/heads/fixture", heads[-1])
        git.run("fsck", "--full", "--strict", "--no-reflogs")
        graph = git.run("rev-list", "--reverse", "--parents", heads[-1]).decode().splitlines()
        require(graph == [base] + [f"{oid} {parent}" for parent, oid in zip([base, *heads[:-1]], heads)],
                "stored pack does not contain the exact linear revision graph")
        trees = {}
        for side, revision in (("base", base), ("head", heads[-1])):
            entries = {}
            for row in git.run("ls-tree", "-rlz", revision).split(b"\0"):
                if row:
                    metadata, name = row.split(b"\t", 1)
                    mode, kind, oid, size = metadata.split()
                    require(mode == b"100644" and kind == b"blob", "executable or non-blob workload entry")
                    entries[name.decode()] = {"oid": oid.decode(), "bytes": int(size)}
            require(set(entries) == expected_paths, "pack tree differs from expected workload")
            require(all(file_entries[name][side] == value for name, value in entries.items()),
                    "manifest object identity or size differs from Git")
            trees[side] = entries
        blobs = read_blobs(git, sorted({value["oid"] for entries in trees.values() for value in entries.values()}))
        for side in ("base", "head"):
            def body(path: str) -> bytes:
                return blobs[trees[side][path]["oid"]]
            require(len(body("large/comparison.cpp").splitlines()) == 20000, "large-file line count drifted")
            require(len(body("generated/table.inc").splitlines()) == 8192
                    and body("generated/table.inc").startswith(b"// Generated fixture version "),
                    "generated-file input drifted")
            require(len(body("assets/data.bin")) == 4096 and b"\0" in body("assets/data.bin"), "binary input drifted")
            for index in range(512):
                lines = body(f"src/file_{index:04d}.cpp").splitlines()
                expected_revision = 0 if side == "base" else index + 2 if index < 31 else 1
                require(len(lines) == 8 and lines[0] == f"int fixture_{index:04d} = {expected_revision};".encode(),
                        "source content or final epoch change drifted")
        changes = {}
        for row in git.run("diff", "--no-renames", "--numstat", base, heads[-1]).decode().splitlines():
            added, removed, name = row.split("\t")
            changes[name] = (None, None) if added == removed == "-" else (int(added), int(removed))
        require(set(changes) == expected_paths, "not every intended file changed")
        for name, counts in changes.items():
            expected = (None, None) if name == "assets/data.bin" else (20, 20) if name == "large/comparison.cpp" else (1, 1)
            require(counts == expected and counts == (file_entries[name]["additions"], file_entries[name]["deletions"]),
                    "Git diff disagrees with independently expected change counts")
        # Every retained head adds a real change; no empty revisions pad the graph.
        for index, head in enumerate(heads[1:]):
            names = git.run("diff", "--name-only", heads[index], head).decode().splitlines()
            require(names == [f"src/file_{index:04d}.cpp"], "retained epoch does not contain its distinct source change")
    seen = set()
    pages = manifest["comment_pages"]
    require(len(pages) == 10, "wrong comment page count")
    for index, page in enumerate(pages):
        require(page["file"] == f"review-comments-{index + 1:02d}.json" and page["count"] == 100
                and page["next_page"] == (index + 2 if index < 9 else None), "comment paging drifted")
        raw = (fixture / page["file"]).read_bytes()
        require(hashlib.sha256(raw).hexdigest() == page["sha256"], "comment page bytes drifted")
        comments = json.loads(raw)
        require(len(comments) == 100, "comment page length drifted")
        for offset, comment in enumerate(comments):
            ordinal = index * 100 + offset
            identifier = 900000 + ordinal
            require(comment["id"] == identifier and identifier not in seen, "duplicate/misordered comment identity")
            require(comment["path"] == f"src/file_{ordinal % 512:04d}.cpp" and comment["line"] == 1
                    and comment["side"] == "RIGHT" and comment["commit_id"] == heads[-1], "comment anchor drifted")
            require(comment["original_line"] == 1 and comment["original_commit_id"] == heads[-1]
                    and comment["body"] == f"Synthetic benchmark observation {ordinal:04d}; no real review.",
                    "synthetic comment identity/content drifted")
            seen.add(identifier)
    require(len(seen) == 1000, "comments were lost")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--git", type=Path, required=True)
    args = parser.parse_args()
    inspect_fixture(FIXTURE, args.git)
    # Mutations have matching manifest digests where appropriate: semantic
    # checks must reject them independently of the stored checksum.
    with contained_temporary_directory(prefix="bpr-large-review-negative-") as raw:
        root = Path(raw)
        copy = root / "fixture"
        shutil.copytree(FIXTURE, copy)
        manifest = json.loads((copy / "manifest.json").read_text())
        original_page = (copy / "review-comments-01.json").read_bytes()
        for mutation in ("anchor", "identity", "pack"):
            changed = json.loads(json.dumps(manifest))
            if mutation == "pack":
                pack = bytearray((FIXTURE / "objects.pack").read_bytes())
                pack[len(pack) // 2] ^= 1
                (copy / "objects.pack").write_bytes(pack)
                changed["pack_sha256"] = hashlib.sha256(pack).hexdigest()
            else:
                page = json.loads(original_page)
                page[0]["line" if mutation == "anchor" else "id"] += 1
                raw_page = (json.dumps(page) + "\n").encode()
                (copy / "review-comments-01.json").write_bytes(raw_page)
                changed["comment_pages"][0]["sha256"] = hashlib.sha256(raw_page).hexdigest()
            (copy / "manifest.json").write_text(json.dumps(changed))
            try:
                inspect_fixture(copy, args.git)
            except (ValueError, subprocess.CalledProcessError):
                print(f"LARGE_REVIEW_NEGATIVE_OK:{mutation}")
            else:
                raise ValueError(f"corrupted {mutation} was accepted")
            (copy / "review-comments-01.json").write_bytes(original_page)
    print("LARGE_REVIEW_FIXTURE_OK files=515 source_files=512 large_lines=20000 heads=32 comments=1000")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
