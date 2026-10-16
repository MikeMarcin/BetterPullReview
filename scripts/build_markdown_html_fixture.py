#!/usr/bin/env python3
"""Writes the constructed Markdown-HTML contract the product can serve.

BPR-1544. The owner judges the reader's `<details>`, GitHub's HTML subset and
emoji shortcodes in the application, beside GitHub's own rendering of the
same bodies. Like `build_markdown_parity_fixture.py` this copies the
constructed nested-folders contract and replaces its description with the
`pr-html` fixture and its pull request comments with the details, html
and emoji fixtures of `tests/fixtures/markdown/github-parity-v1`,
byte for byte. Nothing here was fetched, and no identity is real.

Usage:
    build_markdown_html_fixture.py <repository root>
"""

from __future__ import annotations

import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_markdown_parity_fixture as parity  # noqa: E402

TARGET = "tests/fixtures/github/contracts/v1/markdown-html-v1"

COMMENTS = (
    ("1", "tbutler", "details.md"),
    ("2", "jiayin", "html.md"),
    ("3", "tbutler", "emoji.md"),
)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    root = Path(sys.argv[1])
    source = root / parity.SOURCE
    target = root / TARGET
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)
    pull_path = target / "responses/pull_request.json"
    pull = json.loads(pull_path.read_text(encoding="utf-8"))
    pull["title"] = "Speed up the cache writer and fix the crash on resume"
    pull["body"] = parity.body(root, "pr-html.md")
    parity.write(pull_path, pull)
    parity.write(target / "responses/issue_comments.json", [
        {"node_id": f"IC_mh_{identity}", "user": {"node_id": parity.AUTHORS[author], "login": author}, "body": parity.body(root, name)}
        for identity, author, name in COMMENTS
    ])
    print(f"MARKDOWN_HTML_FIXTURE_OK:{len(COMMENTS)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
