#!/usr/bin/env python3
"""Generate or verify the isolated, deterministic two-file reading fixture."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zlib
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def graph():
    objects = {}

    def add(kind, body):
        oid = hashlib.sha1(f"{kind} {len(body)}\0".encode() + body).hexdigest()
        objects[oid] = (kind, body)
        return oid

    commits, blobs, trees = {}, {}, {}
    for phase, value in (("base", 10), ("head1", 20), ("head2", 30)):
        files = {}
        for name, offset in (("review.cpp", 0), ("secondary.cpp", 100)):
            stem = name.removesuffix(".cpp")
            body = "".join(f"int {stem}_line_{line} = {value + offset + line};\n" for line in range(1, 7))
            files[name] = add("blob", body.encode())
        blobs[phase] = files
        tree = add("tree", b"".join(b"100644 " + name.encode() + b"\0" + bytes.fromhex(oid) for name, oid in sorted(files.items())))
        trees[phase] = tree
        parent = "" if phase == "base" else f"parent {commits['base']}\n"
        commit = (f"tree {tree}\n{parent}author BPR Fixture <fixture@example.invalid> 1700000000 +0000\n"
                  f"committer BPR Fixture <fixture@example.invalid> 1700000000 +0000\n\n{phase} reading fixture\n")
        commits[phase] = add("commit", commit.encode())
    return objects, commits, blobs, trees


def make_pack(objects, commits, head):
    included = set()

    def visit(oid):
        if oid in included:
            return
        included.add(oid)
        kind, body = objects[oid]
        if kind == "commit":
            for line in body.splitlines():
                if line.startswith((b"tree ", b"parent ")):
                    visit(line.split()[1].decode())
        elif kind == "tree":
            while body:
                end = body.index(b"\0") + 1
                visit(body[end:end + 20].hex())
                body = body[end + 20:]

    visit(commits[head])
    pack = b"PACK" + struct.pack(">II", 2, len(included))
    for oid in sorted(included):
        kind, body = objects[oid]
        size = len(body)
        header = bytearray([({"commit": 1, "tree": 2, "blob": 3}[kind] << 4) | (size & 15)])
        size >>= 4
        while size:
            header[-1] |= 128
            header.append(size & 127)
            size >>= 7
        pack += header + zlib.compress(body, 9)
    return pack + hashlib.sha1(pack).digest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--git", default="git")
    parser.add_argument("--generate", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    fixture = root / "tests/fixtures/github/pull-requests/profile-window-restoration-v1"
    objects, commits, blobs, trees = graph()
    semantic = {"schema": "bpr.profile-window-restoration.v1", "commits": commits, "blobs": blobs, "trees": trees}
    if args.generate:
        packs = {"initial.pack": make_pack(objects, commits, "head1"), "replacement.pack": make_pack(objects, commits, "head2")}
        expected = {**semantic, "packs": {name: {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
                    "contains_old_head": name == "initial.pack"} for name, data in packs.items()}}
        fixture.mkdir(parents=True, exist_ok=True)
        for name, data in packs.items():
            (fixture / name).write_bytes(data)
        (fixture / "manifest.json").write_text(json.dumps(expected, indent=2) + "\n")
        print(json.dumps(expected, indent=2))
    manifest = json.loads((fixture / "manifest.json").read_text())
    assert {key: manifest[key] for key in semantic} == semantic
    assert manifest["packs"]["replacement.pack"]["contains_old_head"] is False
    source = (root / "src/app/src/github_public_path_fixture.cpp").read_text()
    for oid in commits.values():
        assert oid in source, f"serving descriptor must name {oid}"
    for name, head, absent in (("initial.pack", "head1", "head2"), ("replacement.pack", "head2", "head1")):
        payload = (fixture / name).read_bytes()
        assert hashlib.sha256(payload).hexdigest() == manifest["packs"][name]["sha256"]
        assert len(payload) == manifest["packs"][name]["bytes"]
        with contained_temporary_directory(prefix="bpr-reading-graph-") as directory:
            env = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
            env.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)

            def git(*command, data=None, check=True):
                return subprocess.run([args.git, "-C", directory, *command], input=data, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE, env=env, check=check)

            git("init", "--bare", "--object-format=sha1")
            git("index-pack", "--stdin", "--strict", "--check-self-contained-and-connected", data=payload)
            assert git("rev-list", "--count", commits[head]).stdout.strip() == b"2"
            assert git("rev-parse", commits[head] + "^").stdout.decode().strip() == commits["base"]
            assert git("cat-file", "-e", commits[absent], check=False).returncode != 0
            assert git("diff-tree", "--no-commit-id", "--name-only", "-r", commits["base"], commits[head]).stdout.splitlines() == [b"review.cpp", b"secondary.cpp"]
            assert git("diff", "--numstat", commits["base"], commits[head]).stdout.splitlines() == [b"6\t6\treview.cpp", b"6\t6\tsecondary.cpp"]
            for phase in ("base", head):
                assert git("rev-parse", commits[phase] + "^{tree}").stdout.decode().strip() == trees[phase]
                for path, oid in blobs[phase].items():
                    content = git("show", f"{commits[phase]}:{path}").stdout
                    assert len(content.splitlines()) == 6
                    assert hashlib.sha1(f"blob {len(content)}\0".encode() + content).hexdigest() == oid
    print("PROFILE_WINDOW_RESTORATION_FIXTURE_OK")


if __name__ == "__main__":
    main()
