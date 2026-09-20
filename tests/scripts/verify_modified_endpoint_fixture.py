#!/usr/bin/env python3
"""Reproduce provider M against a selected base tree lacking that path."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from verify_profile_window_restoration_fixture import graph, make_pack, contained_temporary_directory


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--generate", action="store_true")
    parser.add_argument("--git", default="git")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    fixture = root / "tests/fixtures/github/pull-requests/modified-missing-endpoint-v1"
    objects, original, blobs, _ = graph()

    def add(kind, body):
        oid = hashlib.sha1(f"{kind} {len(body)}\0".encode() + body).hexdigest()
        objects[oid] = kind, body
        return oid

    # Keep secondary.cpp available at both exact endpoints as a recovery control.
    base_tree = add("tree", b"100644 secondary.cpp\0" + bytes.fromhex(blobs["base"]["secondary.cpp"]))
    old_base = objects[original["base"]][1]
    base = add("commit", b"tree " + base_tree.encode() + old_base[old_base.index(b"\n"):])
    head = add("commit", objects[original["head1"]][1].replace(original["base"].encode(), base.encode()))
    commits = {"base": base, "head1": head}
    payload = make_pack(objects, commits, "head1")
    manifest = {"schema": "bpr.modified-missing-endpoint.v1", "commits": commits,
                "base_absent_path": "review.cpp", "intact_path": "secondary.cpp",
                "sha256": hashlib.sha256(payload).hexdigest(), "bytes": len(payload)}
    if args.generate:
        fixture.mkdir(parents=True, exist_ok=True)
        (fixture / "objects.pack").write_bytes(payload)
        (fixture / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    assert json.loads((fixture / "manifest.json").read_text()) == manifest
    assert (fixture / "objects.pack").read_bytes() == payload
    with contained_temporary_directory(prefix="bpr-modified-endpoint-") as directory:
        env = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
        env.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull, GIT_NO_LAZY_FETCH="1")

        def git(*command, data=None):
            return subprocess.run([args.git, "-C", directory, *command], input=data,
                                  capture_output=True, check=True, env=env).stdout

        git("init", "--bare", "--object-format=sha1")
        git("index-pack", "--stdin", "--strict", "--check-self-contained-and-connected", data=payload)
        assert git("ls-tree", base, "--", "review.cpp") == b""
        assert git("ls-tree", head, "--", "review.cpp").startswith(b"100644 blob ")
        assert git("cat-file", "blob", head + ":review.cpp")
        assert git("cat-file", "blob", base + ":secondary.cpp")
        assert git("cat-file", "blob", head + ":secondary.cpp")
    print(json.dumps(manifest))
    print("MODIFIED_ENDPOINT_FIXTURE_OK")


if __name__ == "__main__":
    main()
