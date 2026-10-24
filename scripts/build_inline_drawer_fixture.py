#!/usr/bin/env python3
"""Writes the constructed inline-drawer pull request the product can serve.

BPR-1290. The owner judges the inline Comments drawer against its clickable
prototype (`docs/design/BPR-1290-inline-drawer-v1/index.html`), whose code is a
long file with several threads hanging from its lines. No fixture had threads
on a long file, so this one is the conventional-comments-v1 pull request (the
nested-folders files and their six labelled threads, BPR-1083) plus that long
file: this repository's `src/adapters/qt/src/conversation_cards_view.cpp` at
`fbbabb3ef` (1557 lines, the prototype's code verbatim), as `src/ui/
conversation_cards_view.cpp`, with the prototype's eight threads at its lines.
The changed-line ranges are the prototype's (illustrative): the base is the head
without them. Nothing here was fetched, no identity is real, and none of the
source is built or run.

Usage:
    build_inline_drawer_fixture.py <construction.json>
        Writes the construction, which build_recorded_pull_request_pack.py and
        build_recorded_pull_request_contract.py turn into the pack and the
        served tree.
    build_inline_drawer_fixture.py --threads <contract directory>
        Replaces the served tree's empty review threads with the fourteen
        threads (45 posts).
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_conventional_comments_fixture as conventional  # noqa: E402
import build_nested_folders_fixture as nested  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tests/fixtures/github/pull-requests/inline-drawer-v1/sources/conversation_cards_view.cpp.txt"
LONG_PATH = "src/ui/conversation_cards_view.cpp"

# The commits build_recorded_pull_request_pack.py builds from these files,
# pinned as the nested-folders builder pins its own.
BASE_OID = "916aad158d0a7cd7aaf15d6ca64123c71deaea2d"
HEAD_OID = "89df5e592e5db97a526a05a445735733395e4832"

# The prototype's added ranges, one-based head lines, inclusive.
ADDED = ((601, 630), (656, 673), (1129, 1150), (1166, 1188))

AUTHORS = {
    **conventional.AUTHORS,
    "reviewer-four": "U_fixture_reviewer-four",
    "observer-three": "U_fixture_observer-three",
    "author-two": "U_fixture_author-two",
}

# The prototype's threads on the long file (its draft at 1142 is a local note,
# not a thread). The line-38 thread is resolved.
LONG_THREADS = (
    ("7", 38, True, (
        ("jiayin", "nitpick (non-blocking): `card_gap` and `card_margin` are both 8; one constant would do."),
        ("fixture-author", "They drift apart at Discuss widths, so I kept both."),
        ("jiayin", "praise: Fair, and the names say why."),
    )),
    ("8", 430, False, (
        ("tbutler", "question: Who still calls `set_feed()` once Discuss has its own list?"),
        # BPR-1290 v005: a reply written in Markdown, which the open thread
        # renders through the shared reader (RFC-BPR-0036).
        ("fixture-author", (
            "Only the panel. **Two callers** before this change, one after:\n"
            "\n"
            "- [x] `github_conversation_panel` switches it with Discuss\n"
            "- [ ] `workbench_window` still sets it on a profile switch\n"
            "\n"
            "```cpp\n"
            "Cards->set_feed(Discuss);\n"
            "```\n"
            "\n"
            "| Caller | Before | After |\n"
            "| --- | --- | --- |\n"
            "| panel | yes | yes |\n"
            "| window | yes | **no** |\n"
        )),
    )),
    ("9", 656, False, (
        ("tbutler", "issue (blocking): `set_current_thread()` keeps a reply's id when no card holds it, so `Current` can name nothing."),
        ("fixture-author", "question: Should it clear `Current` instead, or keep the last card?"),
        ("jiayin", "praise: Good catch, this explains the lost highlight after a refresh."),
        ("tbutler", "Clear it. I'll push the fix with a test."),
    )),
    ("10", 667, False, (
        ("reviewer-four", "suggestion (non-blocking, if-minor): Look the id up in an index instead of walking every card's posts."),
        ("author-two", "The index would need rebuilding on every refresh too."),
        ("fixture-author", "It's built in `relayout()` already, so the lookup is free here."),
        ("reviewer-four", "Measured: 0.4 ms against 0.02 at 500 cards. Not urgent."),
        ("fixture-author", "todo (if-minor): Leave a comment pointing at the measurement."),
    )),
    ("11", 927, False, (
        ("tbutler", "suggestion: `relayout()` is 200 lines; measuring and placing could be two functions."),
        ("jiayin", "They share `compactMeasurements`, so splitting means passing it around."),
        ("fixture-author", "Passing one struct is fine. The measuring half has no side effects."),
        ("tbutler", "thought: Then the placing half could be tested without fonts."),
        ("observer-three", "Which would have caught the 280 px clipping last week."),
        ("jiayin", "note (perf): `relayout()` runs twice on a scope change; the second pass finds nothing to do."),
        ("fixture-author", "Splitting after this lands, on its own card."),
        ("tbutler", "praise: Thanks, happy with that."),
    )),
    ("12", 1080, False, (
        ("observer-three", "Why `width - 2 * card_margin` and not the viewport's `contentsRect()`?"),
        ("fixture-author", "The viewport has no margins here, so `contentsRect()` is the same width."),
        ("observer-three", "nitpick (non-blocking): Then a comment saying so would stop the next person asking."),
    )),
    ("13", 1176, False, (
        ("jiayin", "thought (perf): `std::max` here pushes every later card down one by one; does a run of close lines cost a pass each?"),
        ("tbutler", "It's one pass over sorted cards, so linear either way."),
        ("jiayin", "Right, I misread the loop."),
    )),
    ("14", 1510, False, (
        ("tbutler", "chore (non-blocking): Say in a comment that Shift+F10 and the Menu key open the same thread menu."),
        ("jiayin", "+1, the second half of the condition on 1512 is easy to miss."),
    )),
)


def long_file() -> tuple[str, str]:
    head_lines = SOURCE.read_text(encoding="utf-8").splitlines()
    added = {line for first, last in ADDED for line in range(first, last + 1)}
    base_lines = [text for number, text in enumerate(head_lines, start=1) if number not in added]
    return nested.lines(*base_lines), nested.lines(*head_lines)


def construction() -> dict:
    document = nested.construction()
    base, head = long_file()
    text, additions, deletions = nested.patch(base, head)
    document["files"].append({
        "additions": additions,
        "base_content_base64": nested.encode(base),
        "changes": additions + deletions,
        "deletions": deletions,
        "filename": LONG_PATH,
        "head_content_base64": nested.encode(head),
        "patch": text,
        "sha": nested.blob_sha(head),
        "status": "modified",
    })
    document["files"].sort(key=lambda entry: entry["filename"])
    document["commits"][0]["parents"] = [BASE_OID]
    document["commits"][0]["sha"] = HEAD_OID
    document["commits"][0]["message"] = "Keep the conversation cards in the ui folder"
    document["constructed"] = (
        "Synthetic BPR-1290 fixture written by scripts/build_inline_drawer_fixture.py; "
        "not a recording"
    )
    pull = document["pull_request"]
    pull["additions"] = sum(entry["additions"] for entry in document["files"])
    pull["deletions"] = sum(entry["deletions"] for entry in document["files"])
    pull["changed_files"] = len(document["files"])
    pull["base"]["sha"] = BASE_OID
    pull["head"] = {"ref": "inline-drawer", "sha": HEAD_OID}
    pull["body"] = "Constructed fixture: the nested-folders files and a long file, with threads on both.\n"
    pull["number"] = 22
    pull["title"] = "Keep the conversation cards in the ui folder"
    document["repository"] = {"name": "inline-drawer", "owner": "bpr-fixtures"}
    return document


def thread_node(identity: str, path: str, line: int, resolved: bool, posts: tuple[tuple[str, str], ...]) -> dict:
    saved = conventional.AUTHORS
    conventional.AUTHORS = AUTHORS
    try:
        node = conventional.thread_node(identity, path, line, posts)
    finally:
        conventional.AUTHORS = saved
    node["isResolved"] = resolved
    node["viewerCanResolve"] = not resolved
    node["viewerCanUnresolve"] = resolved
    node["viewerCanReply"] = True
    return node


def write_threads(contract: Path) -> int:
    threads_path = contract / "responses/review_threads.json"
    document = json.loads(threads_path.read_text(encoding="utf-8"))
    nodes = [thread_node(identity, path, line, False, posts) for identity, path, line, posts in conventional.THREADS]
    nodes += [thread_node(identity, LONG_PATH, line, resolved, posts) for identity, line, resolved, posts in LONG_THREADS]
    document["data"]["repository"]["pullRequest"]["reviewThreads"] = {
        "nodes": nodes,
        "pageInfo": {"endCursor": conventional.cursor(nodes[-1]["id"]), "hasNextPage": False},
        "totalCount": len(nodes),
    }
    threads_path.write_text(json.dumps(document, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")
    posts = sum(len(thread[3]) for thread in conventional.THREADS) + sum(len(thread[3]) for thread in LONG_THREADS)
    print(f"INLINE_DRAWER_THREADS_OK:{len(nodes)}:{posts}")
    return 0


def main() -> int:
    if len(sys.argv) == 3 and sys.argv[1] == "--threads":
        return write_threads(Path(sys.argv[2]))
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    document = construction()
    output.write_text(json.dumps(document, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")
    print(f"CONSTRUCTION_OK:{len(document['files'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
