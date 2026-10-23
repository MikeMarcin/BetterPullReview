#!/usr/bin/env python3
"""Writes the constructed Overview-labels contract the product can serve.

BPR-1089. The owner judges the Overview's label chips, Open and All, the ?
count and the drill-down into Discuss on a pull request with many labelled
conversations across its folders. conventional-comments-v1 has six threads on
four files, too few to judge a column of chips. This copies the constructed
nested-folders contract (`tests/fixtures/github/contracts/v1/nested-folders-v1`,
BPR-1213) and replaces its empty review threads, reviews and pull request
comments:

- 33 threads on all 17 files: every Conventional Comments label, several on
  one file, resolved and outdated ones, a plain one and a custom label (both
  Other), and a file comment whose root begins with a link, which the
  counter cannot read (?);
- one thread on a file the pull request no longer lists (Not on a file);
- three pull request comments and two review summaries, which have no
  resolution (?, and Not on a file in All).

The pull request, its files and its pack are the nested-folders ones. Nothing
here was fetched, and no identity is real.

Usage:
    build_overview_labels_fixture.py <repository root>
"""

from __future__ import annotations

import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_conventional_comments_fixture as conventional  # noqa: E402

SOURCE = "tests/fixtures/github/contracts/v1/nested-folders-v1"
TARGET = "tests/fixtures/github/contracts/v1/overview-labels-v1"

AUTHORS = {
    **conventional.AUTHORS,
    "reviewer-four": "U_fixture_reviewer-four",
}

HEAD = "2222222222222222222222222222222222222222"

# (thread, path, line, resolved, outdated, [(author, body), ...]). Lines are
# the head lines of the nested-folders files; no line is a file comment.
THREADS = (
    ("1", "src/engine/renderer.cpp", 6, False, False, (
        ("tbutler", "issue (blocking): draw_scene() runs before the clear finishes on the second view."),
        ("fixture-author", "Good find. The second view shares the command list."),
    )),
    ("2", "src/engine/renderer.cpp", 8, False, False, (
        ("jiayin", "issue: The viewport is set after the first draw."),
    )),
    ("3", "src/engine/renderer.cpp", 10, True, False, (
        ("jiayin", "suggestion: Clamp the width to at least one pixel."),
        ("fixture-author", "Done in the second commit."),
    )),
    ("4", "src/engine/renderer.cpp", 12, False, False, (
        ("reviewer-four", "question: Does the second view share the depth buffer?"),
    )),
    ("5", "src/engine/renderer.cpp", 13, False, False, (
        ("tbutler", "nitpick (non-blocking): std::max reads better than the ternary here."),
    )),
    ("6", "src/engine/renderer.cpp", 4, True, False, (
        ("jiayin", "praise: The early return makes this much easier to follow."),
    )),
    ("7", "src/engine/renderer.cpp", 11, False, True, (
        ("tbutler", "todo: Add the clamp before the next push."),
    )),
    ("8", "src/engine/renderer.h", 5, False, False, (
        ("reviewer-four", "question: Should Renderer be non-copyable?"),
    )),
    ("9", "src/engine/renderer.h", 9, False, False, (
        ("jiayin", "nitpick (style): Keep the members in declaration order."),
    )),
    ("10", "src/engine/renderer.h", 12, True, False, (
        ("tbutler", "issue: The destructor is not virtual."),
        ("fixture-author", "Fixed; it is final now."),
    )),
    ("11", "src/engine/shaders/basic.frag", 3, False, False, (
        ("jiayin", "suggestion (if-minor): mediump precision is enough here."),
    )),
    ("12", "src/engine/shaders/basic.vert", 2, True, False, (
        ("tbutler", "typo: postion should be position."),
    )),
    ("13", "src/engine/shaders/post/bloom.frag", 5, False, False, (
        ("jiayin", "quibble: 1.2 reads as a magic number."),
    )),
    ("14", "src/engine/shaders/post/bloom.frag", 3, False, False, (
        ("tbutler", "security: The shader trusts uScene."),
    )),
    ("15", "src/engine/shaders/post/tonemap.frag", 4, False, False, (
        ("reviewer-four", "thought: ACES might suit the HDR path better."),
    )),
    ("16", "src/engine/shaders/post/tonemap.frag", 2, False, False, (
        ("tbutler", "issue (blocking): The exposure uniform is never set."),
    )),
    ("17", "src/main.cpp", 6, False, False, (
        ("tbutler", "nitpick (if-minor): 1280 by 720 could be a named constant."),
    )),
    ("18", "src/main.cpp", 3, False, False, (
        ("reviewer-four", "Why is the window not resizable?"),
    )),
    ("19", "src/main.cpp", 8, True, False, (
        ("jiayin", "chore: Move the argument parsing into its own function."),
    )),
    ("20", "src/ui/widgets/button.cpp", 5, False, False, (
        ("jiayin", "chore: Rename Handler to OnClick while you are here."),
    )),
    # A file comment that begins with a link may be a permalink envelope whose
    # prose the counter cannot map, so its label is unreadable (?).
    ("21", "src/ui/widgets/button.cpp", None, False, False, (
        ("tbutler", "https://github.com/bpr-fixtures/nested-folders/pull/21 issue: The same clamp is missing here."),
    )),
    ("22", "src/ui/widgets/button.h", 6, False, False, (
        ("tbutler", "note: The old name is still used by the plugin API."),
    )),
    ("23", "src/ui/widgets/slider.cpp", 3, False, False, (
        ("reviewer-four", "issue: The value is not clamped to the range."),
    )),
    ("24", "src/ui/widgets/slider.cpp", 5, False, False, (
        ("jiayin", "suggestion (non-blocking): Emit valueChanged only when the value changes."),
    )),
    ("25", "src/ui/widgets/slider.cpp", 2, True, False, (
        ("tbutler", "praise: A small, clear widget."),
    )),
    ("26", "README.md", 6, False, False, (
        ("tbutler", "typo: tests/ should be tests/engine/."),
    )),
    ("27", "README.md", 4, False, False, (
        ("jiayin", "polish: Add a sentence about the release flag."),
    )),
    ("28", "docs/guide.md", 3, False, False, (
        ("reviewer-four", "polish: The second step needs a screenshot."),
    )),
    ("29", "docs/api/reference.md", 2, True, False, (
        ("jiayin", "note: v1 stays documented until the next release."),
    )),
    ("30", "docs/api/v2/endpoints.md", 1, False, False, (
        ("tbutler", "question: Is /v2/render paged?"),
    )),
    ("31", "tests/engine/renderer_tests.cpp", 5, False, False, (
        ("jiayin", "todo: Cover the two-view case."),
    )),
    ("32", "tests/ui/widgets/button_tests.cpp", 4, False, False, (
        ("reviewer-four", "praise: These tests read like a specification."),
    )),
    ("33", "tools/build/scripts/package.py", 3, False, False, (
        ("tbutler", "issue: The archive name ignores the platform."),
    )),
    # A file the pull request no longer lists: counted in Not on a file.
    ("34", "src/legacy/old_renderer.cpp", 10, False, False, (
        ("jiayin", "issue: This file is gone; the fix belongs in renderer.cpp."),
    )),
)

# Pull request comments and review summaries have no resolution.
ISSUE_COMMENTS = (
    ("1", "tbutler", "issue: The release notes do not mention the moved folders."),
    ("2", "jiayin", "praise: Thanks for splitting the shaders out."),
    ("3", "fixture-author", "Rebased on main; the checks pass."),
)
REVIEWS = (
    ("1", 5101, "tbutler", "CHANGES_REQUESTED", "issue: The exposure uniform blocks this; the rest can follow."),
    ("2", 5102, "reviewer-four", "COMMENTED", "question: Could the widgets move land separately?"),
)


def thread_node(identity: str, path: str, line: int | None, resolved: bool, outdated: bool, posts: tuple[tuple[str, str], ...]) -> dict:
    saved = conventional.AUTHORS
    conventional.AUTHORS = AUTHORS
    try:
        node = conventional.thread_node(f"ol_{identity}", path, line, posts)
    finally:
        conventional.AUTHORS = saved
    if line is None:
        node["subjectType"] = "FILE"
    node["isResolved"] = resolved
    node["isOutdated"] = outdated
    node["viewerCanResolve"] = not resolved
    node["viewerCanUnresolve"] = resolved
    node["viewerCanReply"] = True
    for comment in node["comments"]["nodes"]:
        comment["outdated"] = outdated
    return node


def write(path: Path, document: object) -> None:
    path.write_text(json.dumps(document, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")


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
        "pageInfo": {"endCursor": conventional.cursor(nodes[-1]["id"]), "hasNextPage": False},
        "totalCount": len(nodes),
    }
    write(threads_path, document)
    write(target / "responses/issue_comments.json", [
        {"node_id": f"IC_ol_{identity}", "user": {"node_id": AUTHORS[author], "login": author}, "body": body}
        for identity, author, body in ISSUE_COMMENTS
    ])
    write(target / "responses/reviews.json", [
        {"id": number, "node_id": f"PRR_ol_{identity}", "user": {"node_id": AUTHORS[author], "login": author},
         "body": body, "state": state, "commit_id": HEAD}
        for identity, number, author, state, body in REVIEWS
    ])
    posts = sum(len(thread[5]) for thread in THREADS)
    print(f"OVERVIEW_LABELS_FIXTURE_OK:{len(nodes)}:{posts}:{len(ISSUE_COMMENTS)}:{len(REVIEWS)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
