#!/usr/bin/env python3
"""RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]). Writes the returning-review
contracts from conventional-comments-v1.

The same nested-folders pull request, read by `jiayin`, a reviewer who
reviewed it at its first head and commented in the line-12 thread there:

- `returning-review-v1`: the review and the comment are on the first head,
  `b20cb443`, which the next push's pack holds. Served under
  `--epoch-fixture-phase reuse`, the head is the next push and both returning
  questions are offered.

RFC-BPR-0128 ([[BPR-2165]]): Was this addressed? is asked of any thread, its
baseline the commit the thread started on (Q6). Every thread but README.md's
starts on the first head, by whoever wrote it; README.md's starts on the next
push, so nothing has followed it. The commit list is the next push's alone
(a rebased branch), so a press reads its threads without a commit count.

Synthetic public test data; nothing is fetched. Two runs write the same
bytes.

    python3 scripts/build_returning_review_fixture.py tests/fixtures/github/contracts/v1
"""

import copy
import json
import shutil
import sys
from pathlib import Path

FIRST_HEAD = "b20cb443428212543f5911e091a0ffeb407d23cf"
NEXT_HEAD = "2222222222222222222222222222222222222222"
REVIEWER = {"login": "jiayin", "node_id": "U_fixture_jiayin"}


def write(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent="\t", ensure_ascii=False) + "\n", encoding="utf-8")


def build(root: Path, name: str, reviewed: str) -> None:
    source = root / "conventional-comments-v1"
    target = root / name
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)
    responses = target / "responses"
    viewer = json.loads((responses / "viewer.json").read_text(encoding="utf-8"))
    viewer.update({"login": REVIEWER["login"], "name": REVIEWER["login"], "node_id": REVIEWER["node_id"]})
    write(responses / "viewer.json", viewer)
    write(responses / "reviews.json", [{
        "body": "suggestion: Clamp the resize before the next frame.",
        "commit_id": reviewed,
        "id": 7101,
        "node_id": "PRR_rr_1",
        "state": "COMMENTED",
        "user": copy.deepcopy(REVIEWER),
    }])
    threads = json.loads((responses / "review_threads.json").read_text(encoding="utf-8"))
    for thread in threads["data"]["repository"]["pullRequest"]["reviewThreads"]["nodes"]:
        for comment in thread["comments"]["nodes"]:
            # The reviewer's own comments: only the line-12 thread's carries
            # the commit it was written on, so only it is offered.
            if thread["id"] == "PRRT_cc_2" and comment["author"]["login"] == REVIEWER["login"]:
                comment["originalCommit"] = {"oid": reviewed}
                comment["commit"] = {"oid": reviewed}
    # Look ask-v002 (owner, 2026-11-14: "cant tell missing mock agent and
    # response"): the pull request's Was this addressed? reaches more threads
    # than Q5's five, so its confirmation shows. A seventh, on slider.cpp.
    connection = threads["data"]["repository"]["pullRequest"]["reviewThreads"]
    extra = copy.deepcopy(connection["nodes"][5])
    extra.update({"id": "PRRT_cc_7", "path": "src/ui/widgets/slider.cpp", "line": 3, "originalLine": 3})
    extra["comments"]["nodes"] = extra["comments"]["nodes"][:1]
    extra["comments"]["nodes"][0].update({
        "author": {"id": "U_fixture_tbutler", "login": "tbutler"},
        "bodyText": "issue: the slider accepts a value past its maximum.",
        "id": "PRRC_cc_7_1",
        "line": 3,
        "originalLine": 3,
        "path": "src/ui/widgets/slider.cpp",
    })
    extra["comments"]["pageInfo"] = {"endCursor": "Y3Vyc29yOlBSUkNfY2NfN18x", "hasNextPage": False}
    extra["comments"]["totalCount"] = 1
    connection["nodes"].append(extra)
    connection["totalCount"] = len(connection["nodes"])
    connection["pageInfo"]["endCursor"] = "Y3Vyc29yOlBSUlRfY2NfNw=="
    for thread in threads["data"]["repository"]["pullRequest"]["reviewThreads"]["nodes"]:
        first = thread["comments"]["nodes"][0]
        start = NEXT_HEAD if thread["path"] == "README.md" else reviewed
        first.setdefault("originalCommit", {"oid": start})
        first.setdefault("commit", {"oid": start})
    write(responses / "review_threads.json", threads)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    root = Path(sys.argv[1])
    build(root, "returning-review-v1", FIRST_HEAD)
    print("RETURNING_REVIEW_FIXTURE_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
