#!/usr/bin/env python3
"""Writes the next push of the conventional-comments pull request.

BPR-1331. The owner asked that the private progress button's flow "continues
to make sense when the PR gets an update". The conventional-comments pull
request (`bpr-fixtures/nested-folders#21`) is served with one head, so a push
could not be shown on it. This builds a third commit on top of that head that
changes only `src/engine/renderer.cpp`: the clamp its line-12 thread asks for
("Add the clamp before the next push"). Lines 12 and 13 change and no line
moves, so every review thread still lands on the line it was written on. Every
other file keeps the bytes it had.

The base and head are rebuilt from the nested-folders `construction.json` with
`build_recorded_pull_request_pack.py`'s own fixed identity and date, so they
are the commits `nested-folders-v1/objects.pack` holds; the script refuses to
write if they are not. The pack holds every object reachable from the new head
(base, head and next push), so a cache that never saw the first head can still
import it.

Usage:
    build_conventional_comments_next_push_pack.py <construction.json> <output.pack>
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.build_recorded_pull_request_pack import FIXTURE_ENVIRONMENT, build, run  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

# Pinned: the nested-folders base and head (BPR-1213) and the next push this
# script makes from them. The fixture descriptor serves these identities.
BASE_OID = "c58160c2ffe27d7cf5e6759d78b80f023547c789"
HEAD_OID = "b20cb443428212543f5911e091a0ffeb407d23cf"
PUSH_OID = "cd24836cfb58a9c14f4d3464d68a57ff8aa2a186"

CHANGED = "src/engine/renderer.cpp"
BEFORE = ("\tWidth  = aWidth;\n", "\tHeight = aHeight;\n")
AFTER = ("\tWidth  = aWidth < 1 ? 1 : aWidth;\n", "\tHeight = aHeight < 1 ? 1 : aHeight;\n")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    construction = json.loads(Path(sys.argv[1]).read_text())
    output = Path(sys.argv[2])

    with contained_temporary_directory() as repository:
        base, head = build(construction, repository)
        if (base, head) != (BASE_OID, HEAD_OID):
            print(f"FAILED: rebuilt {base}..{head}, expected {BASE_OID}..{HEAD_OID}", file=sys.stderr)
            return 1
        target = Path(repository) / CHANGED
        text = target.read_text()
        for old, new in zip(BEFORE, AFTER):
            if text.count(old) != 1:
                print(f"FAILED: {CHANGED} does not hold {old!r} exactly once", file=sys.stderr)
                return 1
            text = text.replace(old, new)
        target.write_text(text)
        run(repository, "add", "-A")
        run(repository, "commit", "-q", "-m", "Clamp the resize to one pixel")
        pushed = run(repository, "rev-parse", "HEAD").stdout.strip()
        if pushed != PUSH_OID:
            print(f"FAILED: the push built {pushed}, expected {PUSH_OID}", file=sys.stderr)
            return 1

        # The push changes one file, two lines, and moves none.
        numstat = run(repository, "diff", "--numstat", head, pushed).stdout.splitlines()
        if numstat != [f"2\t2\t{CHANGED}"]:
            print(f"FAILED: the push changes {numstat}, expected only {CHANGED} +2 -2", file=sys.stderr)
            return 1

        listing = run(repository, "rev-list", "--objects", pushed)
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

    print(f"PACK_OK:{base}:{head}:{pushed}:{len(pack)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
