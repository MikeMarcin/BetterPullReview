#!/usr/bin/env python3
"""Writes the constructed Conversations pull request the product can serve.

BPR-1293. The owner judges Conversations, the pull request's finder, against
prototype v2 (`docs/design/BPR-1290-inline-drawer-v2/index.html`), whose made-up
pull request has twenty-two conversations in ten files, one of them a deep
thread with branching-thread-v1's shape. No fixture had conversations across
that many files or a thread that branches, so this one is that pull request:
the prototype's ten files verbatim from its `code.js` (each at the revision
named there), and its conversations from
`sources/conversations.json`, extracted verbatim from the prototype's `C0`.
The prototype's draft (line 1142) is a local note, not a thread, so it is not
here. The changed-line ranges are the prototype's (illustrative): seven lines
above and nine below each thread, and its four ranges on
`conversation_cards_view.cpp`; the base is the head without them.

A post's age decides its posting order, oldest first, so a deep thread's posts
are in the order they were written, as GitHub keeps them. The prototype's new
posts are then the end of every conversation, and `read_state.json` names, per
thread, the last post a reader has seen: an attended launch records exactly
that through the review ledger (`--attended-read-state`), so the window shows
the prototype's new posts. Nothing here was fetched, no identity is real, and
none of the source is built or run.

Usage:
    build_conversations_fixture.py <construction.json>
        Writes the construction and, beside it, read_state.json.
    build_conversations_fixture.py --threads <contract directory>
        Replaces the served tree's empty review threads with the twenty-one
        threads.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_conventional_comments_fixture as conventional  # noqa: E402
import build_nested_folders_fixture as nested  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
CODE = ROOT / "docs/design/BPR-1290-inline-drawer-v2/code.js"
SOURCE = ROOT / "tests/fixtures/github/pull-requests/conversations-v1/sources/conversations.json"
LONG_PATH = "src/adapters/qt/src/conversation_cards_view.cpp"

# The commits build_recorded_pull_request_pack.py builds from these files.
BASE_OID = "9a0c46377e514e4760639ffa972cd5812b30c5b2"
HEAD_OID = "f05373e1184e7067994839ecbe6e2db7a933fa97"

# The prototype's four ranges on the long file; elsewhere each thread's.
LONG_ADDED = ((601, 630), (656, 673), (1129, 1150), (1166, 1188))

ME = "fixture-author"
AUTHORS = {
    **conventional.AUTHORS,
    "reviewer-one": "U_fixture_reviewer-one",
    "reviewer-four": "U_fixture_reviewer-four",
    "observer-three": "U_fixture_observer-three",
    "author-two": "U_fixture_author-two",
}


def code_files() -> dict[str, list[str]]:
    text = CODE.read_text(encoding="utf-8")
    start = text.index("{")
    document = json.loads(text[start:text.rstrip().rindex("}") + 1])
    return {path: entry["src"] for path, entry in document.items()}


def conversations() -> list[dict]:
    return [entry for entry in json.loads(SOURCE.read_text(encoding="utf-8"))["conversations"] if not entry["draft"]]


def age_minutes(age: str) -> int:
    match = re.fullmatch(r"(\d+)([mhd])", age)
    if match is None:
        raise ValueError(f"unknown age {age!r}")
    return int(match.group(1)) * {"m": 1, "h": 60, "d": 1440}[match.group(2)]


def ordered_posts(conversation: dict) -> list[dict]:
    """Posts oldest first, each naming the post it answers by its new index.

    A flat thread's replies answer its first comment, as GitHub records them;
    the deep thread's posts name their parents."""
    posts = conversation["posts"]
    names = [post["id"] if conversation["tree"] else str(index) for index, post in enumerate(posts)]
    order = sorted(range(len(posts)), key=lambda index: (-age_minutes(posts[index]["t"]), index))
    position = {names[index]: place for place, index in enumerate(order)}
    result = []
    for place, index in enumerate(order):
        post = posts[index]
        if place == 0:
            parent = None
        elif conversation["tree"]:
            parent = position[post["p"]]
        else:
            parent = 0
        result.append({"author": post["a"], "body": post["b"], "new": post["u"], "parent": parent})
    return result


def added_ranges(path: str, lines: list[int]) -> set[int]:
    if path == LONG_PATH:
        return {line for first, last in LONG_ADDED for line in range(first, last + 1)}
    return {line for thread in lines for line in range(max(1, thread - 7), thread + 10)}


def construction() -> dict:
    files = code_files()
    threads = conversations()
    entries = []
    for path in sorted(files):
        head_lines = files[path]
        added = added_ranges(path, [thread["line"] for thread in threads if thread["f"] == path])
        base_lines = [text for number, text in enumerate(head_lines, start=1) if number not in added]
        base, head = nested.lines(*base_lines), nested.lines(*head_lines)
        text, additions, deletions = nested.patch(base, head)
        entries.append({
            "additions": additions,
            "base_content_base64": nested.encode(base),
            "changes": additions + deletions,
            "deletions": deletions,
            "filename": path,
            "head_content_base64": nested.encode(head),
            "patch": text,
            "sha": nested.blob_sha(head),
            "status": "modified",
        })
    return {
        "commits": [{
            "author": {"date": "2020-01-01T00:00:00Z", "name": ME},
            "message": "Keep the selected conversation across a rebuild",
            "parents": [BASE_OID],
            "sha": HEAD_OID,
        }],
        "constructed": "Synthetic BPR-1293 fixture written by scripts/build_conversations_fixture.py; not a recording",
        "files": entries,
        "pull_request": {
            "additions": sum(entry["additions"] for entry in entries),
            "author": {"login": ME},
            "base": {"ref": "main", "sha": BASE_OID},
            "body": "Constructed fixture: prototype v2's pull request, twenty-one conversations in ten files.\n",
            "changed_files": len(entries),
            "commits": 1,
            "deletions": sum(entry["deletions"] for entry in entries),
            "head": {"ref": "conversations", "sha": HEAD_OID},
            "number": 418,
            "state": "open",
            "title": "Keep the selected conversation across a rebuild",
        },
        "repository": {"name": "conversations", "owner": "bpr-fixtures"},
    }


def comment_id(thread: str, index: int) -> str:
    return f"PRRC_cv_{thread}_{index + 1}"


def thread_node(conversation: dict) -> dict:
    identity = conversation["id"]
    comments = []
    for index, post in enumerate(ordered_posts(conversation)):
        comments.append({
            "id": comment_id(identity, index),
            "bodyText": post["body"],
            "path": conversation["f"],
            "line": conversation["line"],
            "originalLine": conversation["line"],
            "outdated": False,
            "author": {"login": post["author"], "id": AUTHORS[post["author"]]},
            "replyTo": None if post["parent"] is None else {"id": comment_id(identity, post["parent"])},
        })
    resolved = conversation["res"]
    return {
        "id": f"PRRT_cv_{identity}",
        "isResolved": resolved,
        "isOutdated": False,
        "viewerCanResolve": not resolved,
        "viewerCanUnresolve": resolved,
        "viewerCanReply": True,
        "subjectType": "LINE",
        "path": conversation["f"],
        "diffSide": "RIGHT",
        "line": conversation["line"],
        "originalLine": conversation["line"],
        "comments": {
            "totalCount": len(comments),
            "nodes": comments,
            "pageInfo": {"hasNextPage": False, "endCursor": conventional.cursor(comments[-1]["id"])},
        },
    }


def read_state() -> dict:
    """Per thread, the last post seen: the one before its first new post.

    A thread with no new post is seen to its end; one whose first post is new
    is not seen at all. The new posts must be each thread's last ones, or no
    single receipt could say them."""
    threads = {}
    for conversation in conversations():
        posts = ordered_posts(conversation)
        fresh = [index for index, post in enumerate(posts) if post["new"]]
        if fresh and fresh != list(range(fresh[0], len(posts))):
            mine = [index for index in range(fresh[0], len(posts)) if index not in fresh]
            if any(posts[index]["author"] != ME for index in mine):
                raise ValueError(f"{conversation['id']}: new posts are not the thread's last")
        seen = (fresh[0] if fresh else len(posts)) - 1
        thread = f"review-thread:PRRT_cv_{conversation['id']}"
        threads[thread] = None if seen < 0 else f"review-comment:{comment_id(conversation['id'], seen)}"
    return {"threads": threads}


def write_threads(contract: Path) -> int:
    threads_path = contract / "responses/review_threads.json"
    document = json.loads(threads_path.read_text(encoding="utf-8"))
    nodes = [thread_node(conversation) for conversation in conversations()]
    document["data"]["repository"]["pullRequest"]["reviewThreads"] = {
        "nodes": nodes,
        "pageInfo": {"endCursor": conventional.cursor(nodes[-1]["id"]), "hasNextPage": False},
        "totalCount": len(nodes),
    }
    threads_path.write_text(json.dumps(document, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")
    posts = sum(len(node["comments"]["nodes"]) for node in nodes)
    print(f"CONVERSATIONS_THREADS_OK:{len(nodes)}:{posts}")
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
    (output.parent / "read_state.json").write_text(json.dumps(read_state(), indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")
    print(f"CONSTRUCTION_OK:{len(document['files'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
