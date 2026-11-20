#!/usr/bin/env python3
"""[[BPR-2329]]. The live auto-merge course on MikeMarcin/BPR-Test.

Run by hand, never by CTest. MikeMarcin/BPR-Test is a constant, not an
argument. Its `main` requires the commit status `bpr-required-check` (a
branch protection rule the owner approved on 2026-11-20), and the
repository allows auto-merge. The course:

1. makes a branch, a one-file commit and a pull request opened by the owner,
   and posts `bpr-required-check` as pending on its head, so GitHub reports
   the pull request BLOCKED and auto-merge can be enabled;
2. runs `tool_reaction_live_course` (configure with
   `-DBPR_BUILD_LIVE_REACTION_COURSE=ON`) with `--auto-merge-enable`: the
   shipped paged read and Overview projection offer Enable, the shipped
   session enables auto-merge with squash once and sends nothing when asked
   again, and the Overview then shows it enabled and offers Disable;
3. checks through `gh` that GitHub shows auto-merge enabled with squash;
4. runs it with `--auto-merge-disable`: disabled once, nothing sent again,
   and the Overview shows it off;
5. checks through `gh` that auto-merge is off, closes the pull request and
   deletes the branch. The pending status is never made successful, so the
   pull request cannot merge while auto-merge is on.

Every `gh` call reads `X-RateLimit-Remaining` and the course stops below
`--rate-floor`.

    python3 tests/scripts/auto_merge_live_course.py \\
        --tool /abs/build/dev/tests/github_contract/reaction_live_course \\
        --work /abs/scratch/auto-merge-live
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
CHECK = "bpr-required-check"


class CourseFailure(RuntimeError):
    pass


def environment(second: bool) -> dict[str, str]:
    del second  # one identity: the owner
    env = dict(os.environ)
    env.pop("GH_TOKEN", None)
    env.pop("GITHUB_TOKEN", None)
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
        print(f"auto-merge-live:rate-remaining:{remaining}")
        if remaining < floor:
            raise CourseFailure(f"rate-limit-below-floor:{remaining}")
    return json.loads(body) if body.strip() else None


def run_tool(tool: Path, mode: str, config: Path, output: Path, floor: int, second: bool) -> None:
    result = subprocess.run([str(tool), mode, str(config), str(output), str(floor)], capture_output=True, text=True, check=False,
                            env=environment(second))
    sys.stdout.write(result.stdout)
    if result.returncode != 0 or "REACTION_LIVE:OK" not in result.stdout:
        raise CourseFailure(f"tool-{mode}-failed:{result.stderr[-400:]}")


def graphql(number: int, floor: int) -> dict:
    answer = gh(["graphql", "-f", f"query=query{{repository(owner:\"{OWNER}\",name:\"{REPOSITORY}\"){{pullRequest(number:{number}){{mergeStateStatus viewerCanEnableAutoMerge autoMergeRequest{{mergeMethod enabledBy{{login}}}}}}}}}}"], floor)
    return answer["data"]["repository"]["pullRequest"]


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
    if owner["login"] != OWNER:
        raise CourseFailure("unexpected-identity")
    repository = gh([repo], floor)
    if not repository.get("allow_auto_merge"):
        raise CourseFailure("auto-merge-not-allowed")
    protection = gh([f"{repo}/branches/main/protection"], floor)
    if protection["required_status_checks"]["contexts"] != [CHECK]:
        raise CourseFailure("unexpected-required-checks")
    stamp = time.strftime("%Y%m%d-%H%M%S")
    branch = f"bpr-auto-merge-{stamp}"
    main_sha = gh([f"{repo}/git/ref/heads/main"], floor)["object"]["sha"]
    gh([f"{repo}/git/refs", "--method", "POST"], floor, {"ref": f"refs/heads/{branch}", "sha": main_sha})
    pull = None
    answers = options.work / "answers"
    try:
        gh([f"{repo}/contents/auto-merge-course/{stamp}.txt", "--method", "PUT"], floor,
           {"message": "Auto-merge course file", "branch": branch, "content": base64.b64encode(b"Auto-merge course.\n").decode()})
        pull = gh([f"{repo}/pulls", "--method", "POST"], floor,
                  {"title": f"Auto-merge course {stamp}", "head": branch, "base": "main",
                   "body": "Made by tests/scripts/auto_merge_live_course.py; auto-merge is disabled, this is closed and its branch deleted when the course ends."})
        number = pull["number"]
        gh([f"{repo}/statuses/{pull['head']['sha']}", "--method", "POST"], floor,
           {"state": "pending", "context": CHECK, "description": "Held pending by the auto-merge course"})
        state = None
        for _ in range(10):
            state = graphql(number, floor)
            if state["mergeStateStatus"] == "BLOCKED" and state["viewerCanEnableAutoMerge"]:
                break
            time.sleep(3)
        print(f"auto-merge-live:pull:{number}:state:{state['mergeStateStatus']}:can-enable:{state['viewerCanEnableAutoMerge']}")
        if not state["viewerCanEnableAutoMerge"]:
            raise CourseFailure("auto-merge-not-enableable")
        config = {
            "owner": OWNER, "name": REPOSITORY, "number": number,
            "repositoryDatabaseId": repository["id"], "repositoryNode": repository["node_id"],
            "pullRequestDatabaseId": pull["id"], "pullRequestNode": pull["node_id"],
            "head": pull["head"]["sha"], "base": pull["base"]["sha"], "baseRef": pull["base"]["ref"], "headRef": pull["head"]["ref"],
            "author": pull["user"]["login"], "secondNode": "", "secondLogin": "",
            "originalBody": "", "editedBody": "", "reviewComment": "", "issueComment": "", "review": "", "thread": "",
            "actorId": owner["id"], "actorNode": owner["node_id"], "actorLogin": owner["login"],
        }
        path = options.work / "owner.json"
        path.write_text(json.dumps(config))
        (answers / "enable").mkdir(parents=True, exist_ok=True)
        (answers / "disable").mkdir(parents=True, exist_ok=True)
        run_tool(options.tool, "--auto-merge-enable", path, answers / "enable", floor, False)
        on = graphql(number, floor)
        print(f"auto-merge-live:github-after-enable:{json.dumps(on['autoMergeRequest'])}")
        if not on["autoMergeRequest"] or on["autoMergeRequest"]["mergeMethod"] != "SQUASH":
            raise CourseFailure("github-does-not-show-auto-merge-enabled")
        run_tool(options.tool, "--auto-merge-disable", path, answers / "disable", floor, False)
        off = graphql(number, floor)
        print(f"auto-merge-live:github-after-disable:{json.dumps(off['autoMergeRequest'])}")
        if off["autoMergeRequest"] is not None:
            raise CourseFailure("github-still-shows-auto-merge")
    finally:
        if pull is not None:
            if graphql(pull["number"], floor)["autoMergeRequest"] is not None:
                gh(["graphql", "-f", "query=mutation($p:ID!){disablePullRequestAutoMerge(input:{pullRequestId:$p}){clientMutationId}}", "-f", f"p={pull['node_id']}"], floor)
            gh([f"{repo}/pulls/{pull['number']}", "--method", "PATCH"], floor, {"state": "closed"})
        gh([f"{repo}/git/refs/heads/{branch}", "--method", "DELETE"], floor)
        print(f"auto-merge-live:cleaned:{branch}")
    print("auto-merge-live:OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CourseFailure as failure:
        print(f"auto-merge-live:FAILED:{failure}")
        sys.exit(1)
