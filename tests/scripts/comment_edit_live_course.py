#!/usr/bin/env python3
"""[[BPR-1775]]. The live comment-edit course on MikeMarcin/BPR-Test.

Run by hand, never by CTest. The owner designated MikeMarcin/BPR-Test as the
live GitHub target (2026-10-05); it is a constant here, not an argument. The
driver:

1. makes a branch, a one-file commit and a pull request opened by the
   signed-in owner, and as the owner one comment of each kind with CRLF line
   endings: a review (its body) carrying a comment on a line, and a pull
   request comment; as the second identity (`MikeMarcinTest`, its token handed
   to `gh` through `GH_TOKEN` in the child's environment, never printed) one
   pull request comment;
2. runs `tool_reaction_live_course` (configure with
   `-DBPR_BUILD_LIVE_REACTION_COURSE=ON`) as the owner with `--comments`: for
   each of the three, the shipped session edits the body once, saves the same
   text again (nothing sent), is refused a stale original as a conflict
   (nothing sent) and restores the body, which this driver reads back
   through `gh` byte for byte;
3. runs it as the second identity with `--comments-other` on the owner's
   three comments, and as the owner on the second identity's comment: no Edit
   is offered (not theirs) and the session, asked anyway, writes nothing
   because GitHub refuses (`viewerCanUpdate` false);
4. checks every body is the original again, deletes the course's comments,
   closes the pull request and deletes the branch.

Every `gh` call reads `X-RateLimit-Remaining` and the course stops below
`--rate-floor`.

    python3 tests/scripts/comment_edit_live_course.py \\
        --tool /abs/build/dev/tests/github_contract/reaction_live_course \\
        --work /abs/scratch/comment-live
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import subprocess
import sys
import time
from pathlib import Path

if sys.version_info < (3, 10):
    sys.exit(f"Python 3.10 or newer is required; found {sys.version.split()[0]} at {sys.executable}")

OWNER = "MikeMarcin"
REPOSITORY = "BPR-Test"
SECOND = "MikeMarcinTest"
REVIEW_BODY = "Review body made by tests/scripts/comment_edit_live_course.py.\r\n\r\nRestored when the course ends.\r\n"
REVIEW_COMMENT = "Line comment made by the comment-edit course.\r\nSecond line.\r\n"
ISSUE_COMMENT = "Pull request comment made by the comment-edit course.\r\n\r\n- one\r\n- two\r\n"
SECOND_COMMENT = "The second identity's comment, which the owner may not edit.\r\n"


class CourseFailure(RuntimeError):
    pass


def environment(second: bool) -> dict[str, str]:
    env = dict(os.environ)
    env.pop("GH_TOKEN", None)
    env.pop("GITHUB_TOKEN", None)
    if second:
        token = subprocess.run(["gh", "auth", "token", "--user", SECOND], capture_output=True, text=True, check=False)
        if token.returncode != 0 or not token.stdout.strip():
            raise CourseFailure("second-identity-token-unavailable")
        env["GH_TOKEN"] = token.stdout.strip()
    return env


def gh(arguments: list[str], floor: int, payload: dict | None = None, second: bool = False) -> object:
    command = ["gh", "api", "-i", *arguments]
    if payload is not None:
        command += ["--input", "-"]
    result = subprocess.run(command, input=None if payload is None else json.dumps(payload), capture_output=True, text=True,
                            check=False, env=environment(second))
    head, _, body = result.stdout.partition("\r\n\r\n")
    if not body:
        head, _, body = result.stdout.partition("\n\n")
    remaining = None
    for line in head.splitlines():
        name, _, value = line.partition(":")
        if name.strip().lower() == "x-ratelimit-remaining":
            remaining = int(value.strip())
    if result.returncode != 0:
        raise CourseFailure(f"gh-failed:{arguments[0]}:{head.splitlines()[0] if head else ''}:{body[:200]}")
    if remaining is not None:
        print(f"comment-live:rate-remaining:{remaining}")
        if remaining < floor:
            raise CourseFailure(f"rate-limit-below-floor:{remaining}")
    return json.loads(body) if body.strip() else None


def run_tool(tool: Path, mode: str, config: Path, output: Path, floor: int, second: bool) -> None:
    result = subprocess.run([str(tool), mode, str(config), str(output), str(floor)], capture_output=True, text=True, check=False,
                            env=environment(second))
    sys.stdout.write(result.stdout)
    if result.returncode != 0 or "REACTION_LIVE:OK" not in result.stdout:
        raise CourseFailure(f"tool-{mode}-failed:{result.stderr[-400:]}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tool", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--rate-floor", type=int, default=500)
    options = parser.parse_args()
    if not options.tool.is_absolute() or not options.work.is_absolute():
        raise CourseFailure("paths-must-be-absolute")
    options.work.mkdir(parents=True, exist_ok=True)
    floor = options.rate_floor
    repo = f"repos/{OWNER}/{REPOSITORY}"

    owner = gh(["user"], floor)
    second = gh(["user"], floor, second=True)
    if owner["login"] != OWNER or second["login"] != SECOND:
        raise CourseFailure("unexpected-identities")
    repository = gh([repo], floor)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    branch = f"bpr-comment-edit-{stamp}"
    main_sha = gh([f"{repo}/git/ref/heads/main"], floor)["object"]["sha"]
    gh([f"{repo}/git/refs", "--method", "POST"], floor, {"ref": f"refs/heads/{branch}", "sha": main_sha})
    pull = None
    made: list[tuple[str, bool]] = []
    answers = options.work / "answers"
    try:
        path = f"comment-edit-course/{stamp}.txt"
        gh([f"{repo}/contents/{path}", "--method", "PUT"], floor,
           {"message": "Comment-edit course file", "branch": branch, "content": base64.b64encode(b"one\ntwo\nthree\n").decode()})
        pull = gh([f"{repo}/pulls", "--method", "POST"], floor,
                  {"title": f"Comment-edit course {stamp}", "head": branch, "base": "main", "body": "Closed and its branch deleted when the course ends."})
        number = pull["number"]
        review = gh([f"{repo}/pulls/{number}/reviews", "--method", "POST"], floor,
                    {"commit_id": pull["head"]["sha"], "event": "COMMENT", "body": REVIEW_BODY,
                     "comments": [{"path": path, "line": 2, "side": "RIGHT", "body": REVIEW_COMMENT}]})
        line = next(comment for comment in gh([f"{repo}/pulls/{number}/comments"], floor) if comment["pull_request_review_id"] == review["id"])
        made.append((f"{repo}/pulls/comments/{line['id']}", False))
        issue = gh([f"{repo}/issues/{number}/comments", "--method", "POST"], floor, {"body": ISSUE_COMMENT})
        made.append((f"{repo}/issues/comments/{issue['id']}", False))
        foreign = gh([f"{repo}/issues/{number}/comments", "--method", "POST"], floor, {"body": SECOND_COMMENT}, second=True)
        made.append((f"{repo}/issues/comments/{foreign['id']}", True))
        # GitHub's own bytes are the originals.
        bodies = {
            "review": gh([f"{repo}/pulls/{number}/reviews/{review['id']}"], floor)["body"],
            "reviewComment": gh([f"{repo}/pulls/comments/{line['id']}"], floor)["body"],
            "issueComment": gh([f"{repo}/issues/comments/{issue['id']}"], floor)["body"],
            "foreign": gh([f"{repo}/issues/comments/{foreign['id']}"], floor)["body"],
        }
        print("comment-live:crlf-kept:" + ",".join(f"{name}={int(chr(13) in body)}" for name, body in bodies.items()))
        base = {
            "owner": OWNER, "name": REPOSITORY, "number": number,
            "repositoryDatabaseId": repository["id"], "repositoryNode": repository["node_id"],
            "pullRequestDatabaseId": pull["id"], "pullRequestNode": pull["node_id"],
            "head": pull["head"]["sha"], "base": pull["base"]["sha"], "baseRef": pull["base"]["ref"], "headRef": pull["head"]["ref"],
            "author": pull["user"]["login"], "thread": "",
            "reviewComment": f"review-comment:{line['node_id']}", "reviewCommentBody": bodies["reviewComment"],
            "issueComment": f"issue-comment:{issue['node_id']}", "issueCommentBody": bodies["issueComment"],
            "review": f"review:{review['node_id']}", "reviewBody": bodies["review"],
        }
        owners_comments = [{"post": base["reviewComment"], "body": bodies["reviewComment"]},
                           {"post": base["issueComment"], "body": bodies["issueComment"]},
                           {"post": base["review"], "body": bodies["review"]}]
        seconds_comment = [{"post": f"issue-comment:{foreign['node_id']}", "body": bodies["foreign"]}]
        configs: dict[str, Path] = {}
        for name, actor, others in (("owner", owner, seconds_comment), ("second", second, owners_comments)):
            config = dict(base, actorId=actor["id"], actorNode=actor["node_id"], actorLogin=actor["login"], otherComments=others)
            configs[name] = options.work / f"{name}.json"
            configs[name].write_text(json.dumps(config))
            (answers / name).mkdir(parents=True, exist_ok=True)
        run_tool(options.tool, "--comments", configs["owner"], answers / "owner", floor, False)

        def unchanged() -> bool:
            return (gh([f"{repo}/pulls/{number}/reviews/{review['id']}"], floor)["body"] == bodies["review"]
                    and gh([f"{repo}/pulls/comments/{line['id']}"], floor)["body"] == bodies["reviewComment"]
                    and gh([f"{repo}/issues/comments/{issue['id']}"], floor)["body"] == bodies["issueComment"]
                    and gh([f"{repo}/issues/comments/{foreign['id']}"], floor)["body"] == bodies["foreign"])

        if not unchanged():
            raise CourseFailure("owner-course-did-not-restore")
        run_tool(options.tool, "--comments-other", configs["second"], answers / "second", floor, True)
        run_tool(options.tool, "--comments-other", configs["owner"], answers / "owner-other", floor, False)
        if not unchanged():
            raise CourseFailure("another-identity-changed-a-comment")
        print(f"comment-live:pull:{number}")
    finally:
        for endpoint, as_second in made:
            try:
                gh([endpoint, "--method", "DELETE"], floor, second=as_second)
            except CourseFailure as failure:
                print(f"comment-live:cleanup-failed:{failure}")
        if pull is not None:
            gh([f"{repo}/pulls/{pull['number']}", "--method", "PATCH"], floor, {"state": "closed"})
        gh([f"{repo}/git/refs/heads/{branch}", "--method", "DELETE"], floor)
        print(f"comment-live:cleaned:{branch}")
    print("comment-live:OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CourseFailure as failure:
        print(f"comment-live:FAILED:{failure}")
        sys.exit(1)
