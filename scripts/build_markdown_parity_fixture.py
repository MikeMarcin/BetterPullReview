#!/usr/bin/env python3
"""Writes the constructed Markdown-parity contract the product can serve.

BPR-1543. The owner judges the reader's GitHub parity (hidden comments, line
breaks, alerts, footnotes) in the application, beside GitHub's own rendering
of the same bodies. This copies the constructed nested-folders contract
(`tests/fixtures/github/contracts/v1/nested-folders-v1`, BPR-1213) and replaces
its description with the PR-template fixture and its empty pull request
comments with the other fixtures of `tests/fixtures/markdown/github-parity-v1`,
each byte for byte. The pull request, its files and its pack are the
nested-folders ones. Nothing here was fetched, and no identity is real.

Usage:
    build_markdown_parity_fixture.py <repository root>
"""

from __future__ import annotations

import json
import shutil
import sys
from pathlib import Path

SOURCE = "tests/fixtures/github/contracts/v1/nested-folders-v1"
TARGET = "tests/fixtures/github/contracts/v1/markdown-parity-v1"
BODIES = "tests/fixtures/markdown/github-parity-v1"

# (comment, author, fixture) in the order the drawer lists them.
COMMENTS = (
    ("1", "tbutler", "comments.md"),
    ("2", "jiayin", "breaks.md"),
    ("3", "tbutler", "alerts.md"),
    ("4", "jiayin", "footnotes.md"),
)

AUTHORS = {
    "tbutler": "U_fixture_tbutler",
    "jiayin": "U_fixture_jiayin",
}


def write(path: Path, document: object) -> None:
    path.write_text(json.dumps(document, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")


def body(root: Path, name: str) -> str:
    return (root / BODIES / name).read_bytes().decode("utf-8")


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    root = Path(sys.argv[1])
    source = root / SOURCE
    target = root / TARGET
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)
    pull_path = target / "responses/pull_request.json"
    pull = json.loads(pull_path.read_text(encoding="utf-8"))
    pull["title"] = "Move the cache writer onto the serialized lane"
    pull["body"] = body(root, "pr-template.md")
    write(pull_path, pull)
    write(target / "responses/issue_comments.json", [
        {"node_id": f"IC_mp_{identity}", "user": {"node_id": AUTHORS[author], "login": author}, "body": body(root, name)}
        for identity, author, name in COMMENTS
    ])
    print(f"MARKDOWN_PARITY_FIXTURE_OK:{len(COMMENTS)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
