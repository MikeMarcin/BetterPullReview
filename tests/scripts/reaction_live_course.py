#!/usr/bin/env python3
"""[[BPR-PENDING-AR1]]. The live reaction and resolution course on MikeMarcin/BPR-Test.

Run by hand, never by CTest. The owner designated MikeMarcin/BPR-Test as the
live GitHub target (2026-10-05); it is a constant here, not an argument. The
driver:

1. makes a branch, a one-file commit and a pull request, then a review
   thread on its line 2, a pull request comment and a review with words;
2. runs `tool_reaction_live_course` (configure with
   `-DBPR_BUILD_LIVE_REACTION_COURSE=ON`) three times: as the second identity
   (`MikeMarcinTest`, its token handed to `gh` through `GH_TOKEN` in the
   child's environment, never printed) to add a thumbs up; as the signed-in
   owner to read that reaction, add and remove its own reactions on all three
   kinds of comment, resolve, resolve again and unresolve; and as the second
   identity again to remove its thumbs up;
3. closes the pull request and deletes the branch.

Every `gh` call reads `X-RateLimit-Remaining` and the course stops below
`--rate-floor`. The tool keeps each answer GitHub gave in `--output`.

    python3 tests/scripts/reaction_live_course.py \\
        --tool /abs/build/dev/tests/github_contract/reaction_live_course \\
        --work /abs/scratch/reaction-live
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
        print(f"reaction-live:rate-remaining:{remaining}")
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
    branch = f"bpr-reactions-{stamp}"
    main_sha = gh([f"{repo}/git/ref/heads/main"], floor)["object"]["sha"]
    gh([f"{repo}/git/refs", "--method", "POST"], floor, {"ref": f"refs/heads/{branch}", "sha": main_sha})
    pull = None
    second_reacted = False
    configs: dict[str, Path] = {}
    answers = options.work / "answers"
    try:
        text = "\n".join(f"Reaction course line {line}." for line in range(1, 6)) + "\n"
        gh([f"{repo}/contents/reactions-course/{stamp}.txt", "--method", "PUT"], floor,
           {"message": "Reaction course file", "branch": branch, "content": base64.b64encode(text.encode()).decode()})
        pull = gh([f"{repo}/pulls", "--method", "POST"], floor,
                  {"title": f"Reaction course {stamp}", "head": branch, "base": "main",
                   "body": "Made by tests/scripts/reaction_live_course.py; closed and its branch deleted when the course ends."})
        number = pull["number"]
        head = pull["head"]["sha"]
        comment = gh([f"{repo}/pulls/{number}/comments", "--method", "POST"], floor,
                     {"body": "Reaction course: a review thread.", "commit_id": head, "path": f"reactions-course/{stamp}.txt", "line": 2, "side": "RIGHT"})
        issue = gh([f"{repo}/issues/{number}/comments", "--method", "POST"], floor, {"body": "Reaction course: a pull request comment."})
        review = gh([f"{repo}/pulls/{number}/reviews", "--method", "POST"], floor,
                    {"body": "Reaction course: a review with words.", "event": "COMMENT", "commit_id": head})
        threads = gh(["graphql", "-f", f"query=query{{repository(owner:\"{OWNER}\",name:\"{REPOSITORY}\"){{pullRequest(number:{number}){{reviewThreads(first:5){{nodes{{id comments(first:1){{nodes{{id}}}}}}}}}}}}}}"], floor)
        thread = next(node["id"] for node in threads["data"]["repository"]["pullRequest"]["reviewThreads"]["nodes"]
                      if node["comments"]["nodes"][0]["id"] == comment["node_id"])
        pull = gh([f"{repo}/pulls/{number}"], floor)
        base = {
            "owner": OWNER, "name": REPOSITORY, "number": number,
            "repositoryDatabaseId": repository["id"], "repositoryNode": repository["node_id"],
            "pullRequestDatabaseId": pull["id"], "pullRequestNode": pull["node_id"],
            "head": pull["head"]["sha"], "base": pull["base"]["sha"], "baseRef": pull["base"]["ref"], "headRef": pull["head"]["ref"],
            "reviewComment": comment["node_id"], "issueComment": issue["node_id"], "review": review["node_id"], "thread": thread,
        }
        for name, actor in (("owner", owner), ("second", second)):
            config = dict(base, actorId=actor["id"], actorNode=actor["node_id"], actorLogin=actor["login"])
            configs[name] = options.work / f"{name}.json"
            configs[name].write_text(json.dumps(config))
        for name in ("second-add", "owner", "second-remove"):
            (answers / name).mkdir(parents=True, exist_ok=True)
        run_tool(options.tool, "--second-add", configs["second"], answers / "second-add", floor, True)
        second_reacted = True
        run_tool(options.tool, "--main", configs["owner"], answers / "owner", floor, False)
        run_tool(options.tool, "--second-remove", configs["second"], answers / "second-remove", floor, True)
        second_reacted = False
        print(f"reaction-live:pull:{number}")
    finally:
        # A failed course still takes back the second identity's reaction.
        if second_reacted:
            try:
                run_tool(options.tool, "--second-remove", configs["second"], answers / "second-remove", floor, True)
            except CourseFailure as failure:
                print(f"reaction-live:cleanup-reaction-failed:{failure}")
        if pull is not None:
            gh([f"{repo}/pulls/{pull['number']}", "--method", "PATCH"], floor, {"state": "closed"})
        gh([f"{repo}/git/refs/heads/{branch}", "--method", "DELETE"], floor)
        print(f"reaction-live:cleaned:{branch}")
    print("reaction-live:OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CourseFailure as failure:
        print(f"reaction-live:FAILED:{failure}")
        sys.exit(1)
