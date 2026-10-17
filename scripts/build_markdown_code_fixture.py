#!/usr/bin/env python3
"""Writes the constructed fenced-code contract the product can serve.

BPR-1562, BPR-1563, BPR-1564. The owner judges the reader's fenced code
(language colours, diff blocks, suggested changes) in the application, beside
GitHub's own rendering. This copies the constructed conventional-comments
contract (the nested-folders pull request and its files, BPR-1083) and
replaces its description with `tests/fixtures/markdown/code-blocks-v1/
pr-sample.md`, its pull request comments with that directory's diff body and
a suggestion outside a line comment, and its review threads with suggestions
on lines of the head's `src/engine/renderer.cpp` and `src/main.cpp`: one line,
several lines, an inserting and a deleting suggestion. Each body is byte for
byte. Nothing here was fetched, and no identity is real.

Usage:
    build_markdown_code_fixture.py <repository root>
"""

from __future__ import annotations

import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_conventional_comments_fixture as conventional  # noqa: E402
import build_markdown_parity_fixture as parity  # noqa: E402

SOURCE = "tests/fixtures/github/contracts/v1/conventional-comments-v1"
TARGET = "tests/fixtures/github/contracts/v1/markdown-code-v1"
BODIES = "tests/fixtures/markdown/code-blocks-v1"

COMMENTS = (
    ("1", "tbutler", (BODIES, "diff.md")),
    ("2", "jiayin", "A suggestion outside a line comment is ordinary code, as on GitHub:\n\n```suggestion\n\tdraw_scene();\n```\n"),
)

# (thread, path, first line, last line, author, body): one-based head lines.
THREADS = (
    ("1", "src/engine/renderer.cpp", 6, 6, "tbutler", "suggestion: Name the camera the scene is drawn from.\n\n```suggestion\n\tdraw_scene(Camera);\n```\n"),
    ("2", "src/engine/renderer.cpp", 12, 13, "jiayin", "issue: A zero size breaks the projection.\n\n```suggestion\n\tWidth  = std::max(aWidth, 1);\n\tHeight = std::max(aHeight, 1);\n```\n"),
    ("3", "src/engine/renderer.cpp", 4, 4, "tbutler", "```suggestion\n{\n\tif (!Ready) {\n\t\treturn;\n\t}\n```\n"),
    ("4", "src/engine/renderer.cpp", 5, 5, "jiayin", "nitpick: `present()` clears already.\n\n```suggestion\n```\n"),
    ("5", "src/main.cpp", 6, 6, "tbutler", "Keep the size in one place:\n\n```suggestion\n\tconstexpr auto size = std::pair{ 1280, 720 };\n\trenderer.resize(size.first, size.second);\n```\n"),
)


def comment_body(root: Path, source) -> str:
    if isinstance(source, tuple):
        return (root / source[0] / source[1]).read_bytes().decode("utf-8")
    return source


def thread_node(identity: str, path: str, first: int, last: int, author: str, body: str) -> dict:
    node = conventional.thread_node(f"mc{identity}", path, last, ((author, body),))
    if first != last:
        for comment in node["comments"]["nodes"]:
            comment["startLine"] = first
            comment["originalStartLine"] = first
    return node


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
    pull["body"] = (root / BODIES / "pr-sample.md").read_bytes().decode("utf-8")
    parity.write(pull_path, pull)
    parity.write(target / "responses/issue_comments.json", [
        {"node_id": f"IC_mc_{identity}", "user": {"node_id": parity.AUTHORS[author], "login": author}, "body": comment_body(root, body)}
        for identity, author, body in COMMENTS
    ])
    threads_path = target / "responses/review_threads.json"
    document = json.loads(threads_path.read_text(encoding="utf-8"))
    nodes = [thread_node(*thread) for thread in THREADS]
    document["data"]["repository"]["pullRequest"]["reviewThreads"] = {
        "nodes": nodes,
        "pageInfo": {"endCursor": conventional.cursor(nodes[-1]["id"]), "hasNextPage": False},
        "totalCount": len(nodes),
    }
    parity.write(threads_path, document)
    print(f"MARKDOWN_CODE_FIXTURE_OK:{len(COMMENTS)}:{len(nodes)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
