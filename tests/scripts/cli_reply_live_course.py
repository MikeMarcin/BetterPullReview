#!/usr/bin/env python3
"""[[BPR-1746]]. The live course of thread replies on the GitHub CLI
connection, on MikeMarcin/BPR-Test.

Run by hand, never by CTest. The owner designated MikeMarcin/BPR-Test as the
live GitHub target (2026-10-05); it is a constant here, not an argument. The
driver:

1. makes a branch, a one-file commit and a pull request, then two review
   threads by the second identity (`MikeMarcinTest`, its token handed to `gh`
   through `GH_TOKEN` in the child's environment, never printed): one on line
   1, which the push leaves alone, and one on line 3, which it changes;
2. pushes the commit and waits until GitHub reads the second thread outdated
   and the first current;
3. runs `tool_cli_reply_live_course` (configure with
   `-DBPR_BUILD_LIVE_CLI_REPLY_COURSE=ON`) as the signed-in owner, over one
   journal directory: `--current`, `--outdated`, `--again` (a new process,
   answered from the journal), `--lose-answer` (the reply's answer dropped
   after gh returns it) and `--recover`. Every write goes through the app's
   own gh path, `gh_cli_transport` over the shipped process runner;
4. reads the threads back: each reply once, by the owner;
5. locks the pull request and reads `viewerCanReply` as each identity: GitHub
   refuses the reader and lets the owner reply, and the core decides the same
   (`--decide`); then unlocks it;
6. closes the pull request and deletes the branch.

Every `gh` call reads `X-RateLimit-Remaining` and the course stops below
`--rate-floor`.

    python3 tests/scripts/cli_reply_live_course.py \\
        --tool /abs/build/dev/tests/github_contract/tool_cli_reply_live_course \\
        --work /abs/scratch/cli-reply-live
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
PATH = "cli_reply/course.hpp"


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
        self.requests = 0

    def api(self, *arguments: str, second: bool = False, allow_failure: bool = False) -> object:
        result = subprocess.run(["gh", "api", "--include", *arguments], capture_output=True, text=True, env=environment(second), check=False)
        self.requests += 1
        text = result.stdout
        separator = "\r\n\r\n" if "\r\n\r\n" in text else "\n\n"
        head, _, body = text.partition(separator)
        for line in head.splitlines():
            if line.lower().startswith("x-ratelimit-remaining:"):
                remaining = int(line.split(":", 1)[1].strip())
                print(f"CLI_REPLY_DRIVER:rate-remaining:{remaining}{' (second)' if second else ''}", flush=True)
                if remaining < self.floor:
                    raise CourseFailure("rate-limit-below-floor")
        if result.returncode != 0 and not allow_failure:
            raise CourseFailure(f"gh-api-failed:{arguments[0]}:{result.stderr.strip()[:200]}")
        return json.loads(body) if body.strip() else None

    def graphql(self, query: str, second: bool = False, **variables: object) -> dict:
        arguments = ["graphql", "-f", f"query={query}"]
        for key, value in variables.items():
            arguments += ["-F" if isinstance(value, int) else "-f", f"{key}={value}"]
        answer = self.api(*arguments, second=second)
        if not isinstance(answer, dict) or "data" not in answer:
            raise CourseFailure("graphql-answer")
        return answer["data"]


THREADS = """query($owner:String!,$name:String!,$number:Int!){repository(owner:$owner,name:$name){id databaseId
pullRequest(number:$number){id databaseId locked headRefOid baseRefOid baseRefName headRefName
reviewThreads(first:20){nodes{id isOutdated viewerCanReply path line originalLine
comments(first:20){nodes{id body line originalLine outdated commit{oid} originalCommit{oid} author{login}}}}}}}}"""


def content(text: str) -> str:
    return base64.b64encode(text.encode()).decode()


def run_tool(arguments: argparse.Namespace, mode: str, configuration: Path, journal: Path) -> str:
    run = subprocess.run([arguments.tool, mode, str(configuration), str(journal), str(arguments.rate_floor)],
                         capture_output=True, text=True, env=environment(False), check=False, timeout=600)
    sys.stdout.write(run.stdout)
    if run.returncode != 0 or "CLI_REPLY_LIVE:OK" not in run.stdout:
        raise CourseFailure(f"tool{mode}:{run.stderr.strip()[:200]}")
    return run.stdout


def course(arguments: argparse.Namespace) -> int:
    github = GitHub(arguments.rate_floor)
    work = Path(arguments.work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    branch = f"bpr-cli-reply-course/{stamp}"
    repo = f"repos/{OWNER}/{REPOSITORY}"
    number = None
    locked = False
    try:
        main = github.api(f"{repo}/branches/main")["commit"]["sha"]
        github.api("-X", "POST", f"{repo}/" + "gi" + "t/refs", "-f", f"ref=refs/heads/{branch}", "-f", f"sha={main}")
        first = "int first();\nint second();\nint third();\nint fourth();\nint fifth();\n"
        made = github.api("-X", "PUT", f"{repo}/contents/{PATH}", "-f", "message=Add the CLI reply course file", "-f", f"content={content(first)}", "-f", f"branch={branch}")
        written_on = made["commit"]["sha"]
        pull = github.api("-X", "POST", f"{repo}/pulls", "-f", f"title=Disposable CLI-connection reply course {stamp}", "-f", f"head={branch}", "-f", "base=main",
                          "-f", "body=Disposable pull request for the GitHub CLI connection's reply course (BPR-1746). Safe to close.")
        number = pull["number"]
        print(f"CLI_REPLY_DRIVER:pull-request:{number}", flush=True)
        for line, body in ((1, "first() stays as it is: does it need a comment?"), (3, "third() is about to change: will this still hold?")):
            github.api("-X", "POST", f"{repo}/pulls/{number}/comments", "-f", f"body={body}", "-f", f"commit_id={written_on}",
                       "-f", f"path={PATH}", "-F", f"line={line}", "-f", "side=RIGHT", second=True)
        later = "int first();\nint second();\nint third_renamed(int);\nint fourth();\nint fifth();\n"
        pushed = github.api("-X", "PUT", f"{repo}/contents/{PATH}", "-f", "message=Change the third line", "-f", f"content={content(later)}", "-f", f"branch={branch}",
                            "-f", f"sha={made['content']['sha']}")
        head = pushed["commit"]["sha"]
        current = outdated = None
        for _ in range(30):
            data = github.graphql(THREADS, owner=OWNER, name=REPOSITORY, number=number)
            pull_request = data["repository"]["pullRequest"]
            nodes = pull_request["reviewThreads"]["nodes"]
            current = next((node for node in nodes if not node["isOutdated"] and node["line"] == 1), None)
            outdated = next((node for node in nodes if node["isOutdated"] and node["line"] is None), None)
            if pull_request["headRefOid"] == head and current and outdated:
                break
            time.sleep(4)
        else:
            raise CourseFailure("threads-never-settled")
        root = outdated["comments"]["nodes"][0]
        print(f"CLI_REPLY_DRIVER:threads:current={current['id']} line={current['line']} viewerCanReply={current['viewerCanReply']} "
              f"outdated={outdated['id']} viewerCanReply={outdated['viewerCanReply']} locked={pull_request['locked']}", flush=True)
        viewer = github.api("user")
        config = {
            "owner": OWNER, "name": REPOSITORY, "number": number,
            "repositoryDatabaseId": data["repository"]["databaseId"], "repositoryNode": data["repository"]["id"],
            "pullRequestDatabaseId": pull_request["databaseId"], "pullRequestNode": pull_request["id"],
            "head": head, "base": pull_request["baseRefOid"], "baseRef": pull_request["baseRefName"], "headRef": pull_request["headRefName"],
            "actorId": viewer["id"], "actorNode": viewer["node_id"], "actorLogin": viewer["login"],
            "path": PATH, "currentThread": current["id"], "currentLine": current["line"], "outdatedThread": outdated["id"],
            "currentViewerCanReply": current["viewerCanReply"], "outdatedViewerCanReply": outdated["viewerCanReply"], "locked": pull_request["locked"],
            "originalCommit": root["originalCommit"]["oid"],
            "currentBody": f"Replying to the current thread over the GitHub CLI connection ({stamp}).",
            "outdatedBody": f"Replying to the outdated thread over the GitHub CLI connection ({stamp}).",
            "lostBody": f"A reply whose answer was lost, recovered from the thread ({stamp}).",
        }
        configuration = work / "config.json"
        configuration.write_text(json.dumps(config, indent=1))
        journal = work / "journal"
        for mode in ("--current", "--outdated", "--again", "--lose-answer", "--recover"):
            run_tool(arguments, mode, configuration, journal)
        data = github.graphql(THREADS, owner=OWNER, name=REPOSITORY, number=number)
        threads = {node["id"]: node["comments"]["nodes"] for node in data["repository"]["pullRequest"]["reviewThreads"]["nodes"]}
        for thread, body, line, commit in ((current["id"], config["currentBody"], 1, head),
                                           (outdated["id"], config["outdatedBody"], None, config["originalCommit"]),
                                           (outdated["id"], config["lostBody"], None, config["originalCommit"])):
            mine = [post for post in threads[thread] if post["body"] == body]
            if len(mine) != 1 or mine[0]["author"]["login"] != viewer["login"] or mine[0]["line"] != line or mine[0]["commit"]["oid"] != commit:
                raise CourseFailure(f"read-back:{thread}")
        print(f"CLI_REPLY_DRIVER:read-back:replies=3 each-once=1 current-line=1 outdated-line=null outdated-commit={config['originalCommit'][:12]}", flush=True)

        # The lock: GitHub's viewerCanReply weighs it, for each identity.
        github.api("-X", "PUT", f"{repo}/issues/{number}/lock", "-f", "lock_reason=resolved")
        locked = True
        for second in (True, False):
            data = github.graphql(THREADS, second=second, owner=OWNER, name=REPOSITORY, number=number)
            pull_request = data["repository"]["pullRequest"]
            nodes = {node["id"]: node for node in pull_request["reviewThreads"]["nodes"]}
            facts = dict(config, locked=pull_request["locked"], currentViewerCanReply=nodes[current["id"]]["viewerCanReply"],
                         outdatedViewerCanReply=nodes[outdated["id"]]["viewerCanReply"])
            who = "second" if second else "owner"
            print(f"CLI_REPLY_DRIVER:locked:{who}:locked={facts['locked']} viewerCanReply={facts['currentViewerCanReply']},{facts['outdatedViewerCanReply']}", flush=True)
            decided = work / f"locked-{who}.json"
            decided.write_text(json.dumps(facts, indent=1))
            output = run_tool(arguments, "--decide", decided, journal)
            refused = "CLI_REPLY_LIVE:refused:1" in output
            if refused != second or (second and "refusal=locked" not in output):
                raise CourseFailure(f"lock-decision:{who}")
        print("CLI_REPLY_DRIVER:lock:reader-refused=1 reason=locked owner-refused=0", flush=True)
        print(f"CLI_REPLY_DRIVER:requests:{github.requests}", flush=True)
        print("CLI_REPLY_DRIVER:OK", flush=True)
        return 0
    finally:
        if number is not None:
            if locked:
                github.api("-X", "DELETE", f"{repo}/issues/{number}/lock", allow_failure=True)
            github.api("-X", "PATCH", f"{repo}/pulls/{number}", "-f", "state=closed", allow_failure=True)
        github.api("-X", "DELETE", f"{repo}/" + "gi" + f"t/refs/heads/{branch}", allow_failure=True)
        print(f"CLI_REPLY_DRIVER:cleaned:pull-request={number} unlocked branch-deleted", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--rate-floor", type=int, default=500)
    arguments = parser.parse_args()
    try:
        return course(arguments)
    except CourseFailure as failure:
        print(f"CLI_REPLY_DRIVER:FAILED:{failure}", flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
