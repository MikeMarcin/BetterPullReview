#!/usr/bin/env python3
"""[[BPR-PENDING-AT]]. The live outdated-thread reply course on MikeMarcin/BPR-Test.

Run by hand, never by CTest. The owner designated MikeMarcin/BPR-Test as the
live GitHub target (2026-10-05); it is a constant here, not an argument. The
driver:

1. makes a branch, a one-file commit and a pull request, then a review thread
   on line 3 by the second identity (`MikeMarcinTest`, its token handed to
   `gh` through `GH_TOKEN` in the child's environment, never printed);
2. pushes a commit that changes line 3 and waits until GitHub reads the thread
   outdated, with no line;
3. runs `tool_outdated_reply_live_course` (configure with
   `-DBPR_BUILD_LIVE_OUTDATED_REPLY_COURSE=ON`) as the signed-in owner, over
   one journal directory: `--reply`, `--again` (a new process, answered from
   the journal), `--lose-answer` (a second reply whose answer is dropped after
   it is sent, so its outcome is unclear until the reconciliation finds it in
   the thread) and `--recover` (a new process, answered from the journal);
4. reads the thread back: each reply once, by the owner, with no line, on the
   commit the thread was written on;
5. closes the pull request and deletes the branch.

Every `gh` call reads `X-RateLimit-Remaining` and the course stops below
`--rate-floor`.

    python3 tests/scripts/outdated_reply_live_course.py \\
        --tool /abs/build/dev/tests/github_contract/tool_outdated_reply_live_course \\
        --work /abs/scratch/outdated-reply-live
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
PATH = "outdated_reply/course.hpp"


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


class GitHub:
    def __init__(self, floor: int) -> None:
        self.floor = floor
        self.remaining: int | None = None

    def api(self, *arguments: str, second: bool = False, allow_failure: bool = False) -> object:
        result = subprocess.run(["gh", "api", "--include", *arguments], capture_output=True, text=True, env=environment(second), check=False)
        text = result.stdout
        separator = "\r\n\r\n" if "\r\n\r\n" in text else "\n\n"
        head, _, body = text.partition(separator)
        for line in head.splitlines():
            if line.lower().startswith("x-ratelimit-remaining:"):
                self.remaining = int(line.split(":", 1)[1].strip())
                print(f"OUTDATED_REPLY_DRIVER:rate-remaining:{self.remaining}", flush=True)
                if self.remaining < self.floor:
                    raise CourseFailure("rate-limit-below-floor")
        if result.returncode != 0 and not allow_failure:
            raise CourseFailure(f"gh-api-failed:{arguments[0]}:{result.stderr.strip()[:200]}")
        return json.loads(body) if body.strip() else None

    def graphql(self, query: str, **variables: object) -> dict:
        arguments = ["graphql", "-f", f"query={query}"]
        for key, value in variables.items():
            arguments += ["-F" if isinstance(value, int) else "-f", f"{key}={value}"]
        answer = self.api(*arguments)
        if not isinstance(answer, dict) or "data" not in answer:
            raise CourseFailure("graphql-answer")
        return answer["data"]


THREADS = """query($owner:String!,$name:String!,$number:Int!){repository(owner:$owner,name:$name){id databaseId
pullRequest(number:$number){id databaseId locked headRefOid baseRefOid baseRefName headRefName
reviewThreads(first:20){nodes{id isOutdated viewerCanReply path line originalLine
comments(first:20){nodes{id body line originalLine outdated commit{oid} originalCommit{oid} author{login}}}}}}}}"""


def content(text: str) -> str:
    return base64.b64encode(text.encode()).decode()


def course(arguments: argparse.Namespace) -> int:
    github = GitHub(arguments.rate_floor)
    work = Path(arguments.work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    branch = f"bpr-outdated-reply-course/{stamp}"
    repo = f"repos/{OWNER}/{REPOSITORY}"
    number = None
    try:
        main = github.api(f"{repo}/branches/main")["commit"]["sha"]
        github.api("-X", "POST", f"{repo}/" + "gi" + "t/refs", "-f", f"ref=refs/heads/{branch}", "-f", f"sha={main}")
        first = "int first();\nint second();\nint third();\nint fourth();\nint fifth();\n"
        made = github.api("-X", "PUT", f"{repo}/contents/{PATH}", "-f", "message=Add the outdated-reply course file", "-f", f"content={content(first)}", "-f", f"branch={branch}")
        written_on = made["commit"]["sha"]
        pull = github.api("-X", "POST", f"{repo}/pulls", "-f", f"title=Disposable outdated-thread reply course {stamp}", "-f", f"head={branch}", "-f", "base=main",
                          "-f", "body=Disposable pull request for the outdated-thread reply course (BPR-PENDING-AT). Safe to close.")
        number = pull["number"]
        print(f"OUTDATED_REPLY_DRIVER:pull-request:{number}", flush=True)
        github.api("-X", "POST", f"{repo}/pulls/{number}/comments", "-f", "body=third() is about to change: will this still hold?", "-f", f"commit_id={written_on}",
                   "-f", f"path={PATH}", "-F", "line=3", "-f", "side=RIGHT", second=True)
        later = "int first();\nint second();\nint third_renamed(int);\nint fourth();\nint fifth();\n"
        pushed = github.api("-X", "PUT", f"{repo}/contents/{PATH}", "-f", "message=Change the commented line", "-f", f"content={content(later)}", "-f", f"branch={branch}",
                            "-f", f"sha={made['content']['sha']}")
        head = pushed["commit"]["sha"]
        thread = None
        for _ in range(30):
            data = github.graphql(THREADS, owner=OWNER, name=REPOSITORY, number=number)
            pull_request = data["repository"]["pullRequest"]
            nodes = pull_request["reviewThreads"]["nodes"]
            if pull_request["headRefOid"] == head and nodes and nodes[0]["isOutdated"] and nodes[0]["line"] is None:
                thread = nodes[0]
                break
            time.sleep(4)
        if thread is None:
            raise CourseFailure("thread-never-outdated")
        root = thread["comments"]["nodes"][0]
        print(f"OUTDATED_REPLY_DRIVER:thread:{thread['id']} outdated={thread['isOutdated']} line={thread['line']} viewerCanReply={thread['viewerCanReply']} "
              f"locked={pull_request['locked']} originalCommit={root['originalCommit']['oid'][:12]}", flush=True)
        viewer = github.api("user")
        config = {
            "owner": OWNER, "name": REPOSITORY, "number": number,
            "repositoryDatabaseId": data["repository"]["databaseId"], "repositoryNode": data["repository"]["id"],
            "pullRequestDatabaseId": pull_request["databaseId"], "pullRequestNode": pull_request["id"],
            "head": head, "base": pull_request["baseRefOid"], "baseRef": pull_request["baseRefName"], "headRef": pull_request["headRefName"],
            "actorId": viewer["id"], "actorNode": viewer["node_id"], "actorLogin": viewer["login"],
            "thread": thread["id"], "path": thread["path"], "outdated": thread["isOutdated"], "viewerCanReply": thread["viewerCanReply"],
            "locked": pull_request["locked"], "originalCommit": root["originalCommit"]["oid"],
            "body": f"Replying to the outdated thread from BetterPullReview ({stamp}).",
            "secondBody": f"A second reply whose answer was lost, recovered from the thread ({stamp}).",
        }
        configuration = work / "config.json"
        configuration.write_text(json.dumps(config, indent=1))
        output = work / "answers"
        output.mkdir(exist_ok=True)
        journal = work / "journal"
        for mode in ("--reply", "--again", "--lose-answer", "--recover"):
            run = subprocess.run([arguments.tool, mode, str(configuration), str(output), str(journal), str(arguments.rate_floor)],
                                 capture_output=True, text=True, env=environment(False), check=False, timeout=600)
            sys.stdout.write(run.stdout)
            if run.returncode != 0 or "OUTDATED_REPLY_LIVE:OK" not in run.stdout:
                raise CourseFailure(f"tool{mode}:{run.stderr.strip()[:200]}")
        data = github.graphql(THREADS, owner=OWNER, name=REPOSITORY, number=number)
        posts = data["repository"]["pullRequest"]["reviewThreads"]["nodes"][0]["comments"]["nodes"]
        for body in (config["body"], config["secondBody"]):
            mine = [post for post in posts if post["body"] == body]
            if len(mine) != 1 or mine[0]["author"]["login"] != viewer["login"] or mine[0]["line"] is not None or mine[0]["commit"]["oid"] != config["originalCommit"]:
                raise CourseFailure("read-back")
        print(f"OUTDATED_REPLY_DRIVER:read-back:replies=2 each-once=1 line=null commit={config['originalCommit'][:12]}", flush=True)
        print("OUTDATED_REPLY_DRIVER:OK", flush=True)
        return 0
    finally:
        if number is not None:
            github.api("-X", "PATCH", f"{repo}/pulls/{number}", "-f", "state=closed", allow_failure=True)
        github.api("-X", "DELETE", f"{repo}/" + "gi" + f"t/refs/heads/{branch}", allow_failure=True)
        print(f"OUTDATED_REPLY_DRIVER:cleaned:pull-request={number} branch-deleted", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--rate-floor", type=int, default=500)
    arguments = parser.parse_args()
    try:
        return course(arguments)
    except CourseFailure as failure:
        print(f"OUTDATED_REPLY_DRIVER:FAILED:{failure}", flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
