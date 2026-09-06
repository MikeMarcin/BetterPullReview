#!/usr/bin/env python3
"""Generate synthetic benchmark inputs with isolated local Git; never fetch.

The recipe and Git object identities are deterministic. Pack compression can
vary with Git versions, so the stored pack is pinned separately by SHA-256.
No workload file is executed. This supplies inputs, not performance evidence.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

FIXTURE = Path(__file__).resolve().parents[1] / "tests/fixtures/github/pull-requests/large-review-v1"
RECIPE = {
    "schema": "bpr.large-review-recipe.v1", "source_files": 512, "source_lines": 8,
    "large_file_lines": 20000, "generated_file_lines": 8192, "binary_bytes": 4096,
    "head_revisions": 32, "review_comments": 1000, "comments_per_page": 100,
}


class LocalGit:
    """A fixed local capability with no inherited Git configuration or hooks."""
    def __init__(self, executable: Path, root: Path):
        self.executable = executable.resolve(strict=True)
        self.repository = root / "repository"
        self.repository.mkdir()
        self.hooks = root / "empty-hooks"
        self.hooks.mkdir()
        self.environment = {key: value for key, value in os.environ.items()
                            if not key.startswith("GIT_")}
        self.environment.update(
            HOME=str(root), XDG_CONFIG_HOME=str(root), GIT_CONFIG_NOSYSTEM="1",
            GIT_CONFIG_GLOBAL=os.devnull, GIT_TERMINAL_PROMPT="0", GIT_OPTIONAL_LOCKS="0",
        )
        self.run("init", "--quiet", "--bare", "--object-format=sha1", f"--template={self.hooks}")

    def run(self, *arguments: str, data: bytes | None = None) -> bytes:
        return subprocess.run(
            [str(self.executable), "-C", str(self.repository),
             "-c", f"core.hooksPath={self.hooks}", "-c", "protocol.allow=never",
             "-c", "credential.helper=", "-c", "core.autocrlf=false",
             "-c", "gc.auto=0", "-c", "maintenance.auto=false", *arguments],
            input=data, capture_output=True, check=True, env=self.environment, timeout=60,
        ).stdout


def source_file(index: int, revision: int) -> bytes:
    return (f"int fixture_{index:04d} = {revision};\n"
            + "".join(f"// synthetic source {index:04d} line {line}\n" for line in range(2, 9))).encode()


def files(revision: int) -> dict[str, bytes]:
    changed = int(revision != 0)
    result = {f"src/file_{index:04d}.cpp": source_file(index, changed)
              for index in range(RECIPE["source_files"])}
    result["large/comparison.cpp"] = "".join(
        f"int sample_{line:05d} = {changed if line % 1000 == 0 else 0};\n"
        for line in range(RECIPE["large_file_lines"])).encode()
    result["generated/table.inc"] = (f"// Generated fixture version {changed}; do not edit.\n"
        + "".join(f"ENTRY({line:05d}, {line % 17})\n"
                  for line in range(1, RECIPE["generated_file_lines"]))).encode()
    result["assets/data.bin"] = bytes([changed]) + bytes(range(256)) * 15 + bytes(range(255))
    return result


def commit_stream() -> bytes:
    output = bytearray()
    for revision in range(RECIPE["head_revisions"] + 1):
        output.extend(f"commit refs/heads/workload\nmark :{revision + 1}\n"
                      f"committer BetterPullReview Fixture <fixture@invalid> {1600000000 + revision} +0000\n".encode())
        message = f"Synthetic workload revision {revision}\n".encode()
        output.extend(f"data {len(message)}\n".encode() + message)
        if revision:
            output.extend(f"from :{revision}\n".encode())
        content = files(revision) if revision <= 1 else {
            f"src/file_{revision - 2:04d}.cpp": source_file(revision - 2, revision)}
        for path, body in sorted(content.items()):
            output.extend(f"M 100644 inline {path}\ndata {len(body)}\n".encode() + body + b"\n")
        output.extend(b"\n")
    output.extend(b"done\n")
    return bytes(output)


def tree(git: LocalGit, revision: str) -> dict[str, dict]:
    result = {}
    for entry in git.run("ls-tree", "-rlz", revision).split(b"\0"):
        if entry:
            metadata, path = entry.split(b"\t", 1)
            mode, kind, oid, size = metadata.split()
            if mode != b"100644" or kind != b"blob":
                raise ValueError("workload contains a non-regular blob")
            result[path.decode()] = {"oid": oid.decode(), "bytes": int(size)}
    return result


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")


def generate(executable: Path, output: Path) -> dict:
    recipe = json.loads((FIXTURE / "recipe.json").read_text())
    if recipe != RECIPE:
        raise ValueError("recipe drift requires an explicit new fixture version")
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / "recipe.json", recipe)
    with contained_temporary_directory(prefix="bpr-large-review-build-") as raw:
        git = LocalGit(executable, Path(raw).resolve())
        git.run("fast-import", "--quiet", "--date-format=raw", data=commit_stream())
        revisions = git.run("rev-list", "--reverse", "refs/heads/workload").decode().splitlines()
        if len(revisions) != RECIPE["head_revisions"] + 1:
            raise ValueError("wrong workload revision graph")
        git.run("fsck", "--full", "--strict")
        pack = git.run("pack-objects", "--stdout", "--revs", "--threads=1",
                       data=(revisions[-1] + "\n").encode())
        base_tree, head_tree = tree(git, revisions[0]), tree(git, revisions[-1])
        changes = []
        for row in git.run("diff", "--no-renames", "--numstat", revisions[0], revisions[-1]).decode().splitlines():
            added, removed, path = row.split("\t")
            changes.append({"path": path, "additions": None if added == "-" else int(added),
                            "deletions": None if removed == "-" else int(removed),
                            "base": base_tree[path], "head": head_tree[path]})
        epoch_deltas = []
        previous_tree = tree(git, revisions[1])
        for ordinal, head in enumerate(revisions[2:], start=2):
            current_tree = tree(git, head)
            paths = git.run("diff", "--name-only", revisions[ordinal - 1], head).decode().splitlines()
            epoch_deltas.append({"previous_head": revisions[ordinal - 1], "head": head,
                                 "files": [{"path": path, "before": previous_tree[path],
                                            "after": current_tree[path]} for path in paths]})
            previous_tree = current_tree
    write_json(output / "epoch-deltas.json", {"schema": "bpr.large-review-epoch-deltas.v1",
                                             "base": revisions[0], "deltas": epoch_deltas})
    (output / "objects.pack").write_bytes(pack)
    pages = []
    for page in range(RECIPE["review_comments"] // RECIPE["comments_per_page"]):
        comments = []
        for offset in range(RECIPE["comments_per_page"]):
            index = page * RECIPE["comments_per_page"] + offset
            identifier = 900000 + index
            comments.append({
                "id": identifier, "node_id": f"SYNTHETIC_COMMENT_{identifier}",
                "body": f"Synthetic benchmark observation {index:04d}; no real review.",
                "path": f"src/file_{index % RECIPE['source_files']:04d}.cpp",
                "line": 1, "original_line": 1, "side": "RIGHT", "position": 1,
                "commit_id": revisions[-1], "original_commit_id": revisions[-1],
                "created_at": "2020-09-13T12:26:40Z", "updated_at": "2020-09-13T12:26:40Z",
                "html_url": f"https://github.com/bpr-fixtures/large-review/pull/17#discussion_r{identifier}",
                "user": {"id": 800001, "node_id": "SYNTHETIC_USER_800001", "login": "fixture-reviewer", "type": "User"},
            })
        name = f"review-comments-{page + 1:02d}.json"
        write_json(output / name, comments)
        pages.append({"file": name, "count": len(comments),
                      "next_page": page + 2 if page < 9 else None,
                      "sha256": hashlib.sha256((output / name).read_bytes()).hexdigest()})
    manifest = {"schema": "bpr.large-review-fixture.v1", "provenance": "synthetic; generated locally; no remote retrieval",
                "recipe": recipe, "base": revisions[0], "heads": revisions[1:],
                "pack_sha256": hashlib.sha256(pack).hexdigest(), "pack_bytes": len(pack),
                "files": changes, "comment_pages": pages}
    write_json(output / "manifest.json", manifest)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--git", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=FIXTURE)
    args = parser.parse_args()
    result = generate(args.git, args.output)
    print(f"LARGE_REVIEW_FIXTURE_BUILT files={len(result['files'])} heads={len(result['heads'])} pack_bytes={result['pack_bytes']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
