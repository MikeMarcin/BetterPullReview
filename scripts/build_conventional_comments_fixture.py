#!/usr/bin/env python3
"""Writes the constructed Conventional Comments contract the product can serve.

BPR-1083. The owner judges the label marks, the chip and the shortcodes in the
application, on a pull request whose review threads carry every Conventional
Comments label. This copies the constructed nested-folders contract
(`tests/fixtures/github/contracts/v1/nested-folders-v1`, BPR-1213) and replaces
its empty review threads with six threads on its changed lines. The pull
request, its files and its pack are the nested-folders ones; only the threads
differ. Nothing here was fetched, and no identity is real.

Usage:
    build_conventional_comments_fixture.py <repository root>
"""

from __future__ import annotations

import base64
import json
import shutil
import sys
from pathlib import Path

SOURCE = "tests/fixtures/github/contracts/v1/nested-folders-v1"
TARGET = "tests/fixtures/github/contracts/v1/conventional-comments-v1"

AUTHORS = {
    "tbutler": "U_fixture_tbutler",
    "jiayin": "U_fixture_jiayin",
    "fixture-author": "U_fixture-author",
}

# (thread, path, line, [(author, body), ...]). Lines are the head lines of the
# nested-folders files. Every label appears once; one post is plain and one
# carries a custom label, which gets no mark.
THREADS = (
    ("1", "src/engine/renderer.cpp", 6, (
        ("tbutler", "issue (blocking): draw_scene() runs before the clear finishes on the second view."),
        ("fixture-author", "question: Does the second view share the depth buffer?"),
        ("jiayin", "praise: Good catch, this explains the flicker."),
        ("tbutler", "Thanks, resolving after CI."),
    )),
    ("2", "src/engine/renderer.cpp", 12, (
        ("jiayin", "suggestion (non-blocking): Clamp the width to at least one pixel."),
        ("fixture-author", "todo: I will add the clamp in the next push."),
    )),
    ("3", "src/main.cpp", 6, (
        ("tbutler", "nitpick (if-minor): 1280 by 720 could be a named constant."),
        ("jiayin", "thought: A config file may want to own this later."),
    )),
    ("4", "src/ui/widgets/button.cpp", 5, (
        ("jiayin", "chore: Rename Handler to OnClick while you are here."),
        ("tbutler", "note: The old name is still used by the plugin API."),
    )),
    ("5", "README.md", 6, (
        ("tbutler", "typo: tests/ should be tests/engine/."),
        ("jiayin", "polish: Add a sentence about the release flag."),
    )),
    ("6", "src/engine/shaders/post/bloom.frag", 5, (
        ("jiayin", "quibble: 1.2 reads as a magic number."),
        ("tbutler", "security: the shader trusts uScene."),
    )),
)


def cursor(text: str) -> str:
    return base64.b64encode(f"cursor:{text}".encode("ascii")).decode("ascii")


def thread_node(identity: str, path: str, line: int, posts: tuple[tuple[str, str], ...]) -> dict:
    comments = []
    for index, (author, body) in enumerate(posts, start=1):
        comment = f"PRRC_cc_{identity}_{index}"
        comments.append({
            "id": comment,
            "bodyText": body,
            "path": path,
            "line": line,
            "originalLine": line,
            "outdated": False,
            "author": {"login": author, "id": AUTHORS[author]},
            "replyTo": None if index == 1 else {"id": f"PRRC_cc_{identity}_1"},
        })
    return {
        "id": f"PRRT_cc_{identity}",
        "isResolved": False,
        "isOutdated": False,
        "viewerCanResolve": True,
        "viewerCanUnresolve": False,
        "subjectType": "LINE",
        "path": path,
        "diffSide": "RIGHT",
        "line": line,
        "originalLine": line,
        "comments": {
            "totalCount": len(comments),
            "nodes": comments,
            "pageInfo": {"hasNextPage": False, "endCursor": cursor(comments[-1]["id"])},
        },
    }


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
    threads_path = target / "responses/review_threads.json"
    document = json.loads(threads_path.read_text(encoding="utf-8"))
    nodes = [thread_node(*thread) for thread in THREADS]
    document["data"]["repository"]["pullRequest"]["reviewThreads"] = {
        "nodes": nodes,
        "pageInfo": {"endCursor": cursor(nodes[-1]["id"]), "hasNextPage": False},
        "totalCount": len(nodes),
    }
    threads_path.write_text(json.dumps(document, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")
    print(f"CONVENTIONAL_COMMENTS_FIXTURE_OK:{len(nodes)}:{sum(len(thread[3]) for thread in THREADS)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
