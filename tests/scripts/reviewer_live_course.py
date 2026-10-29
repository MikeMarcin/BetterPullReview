#!/usr/bin/env python3
"""[[BPR-1804]]. The live reviewer course on MikeMarcin/BPR-Test.

Run by hand, never by CTest. The owner designated MikeMarcin/BPR-Test as the
live GitHub target (2026-10-05); it is a constant here, not an argument. Its
only collaborator is the owner, who opens every pull request, so the course
measures what this repository allows:

1. makes a branch, a one-file commit and a pull request opened by the owner;
2. runs `tool_reaction_live_course` (configure with
   `-DBPR_BUILD_LIVE_REACTION_COURSE=ON`) as the owner with `--reviewers`:
   the shipped session searches everyone and the owner's own login (GitHub
   lists the owner as assignable; the picker never shows the author), asks
   for a review from the second identity, who is not a collaborator (written
   once; GitHub answers success without adding them, and the core says not
   added), and is refused a request of the author before anything is sent;
3. runs it as the second identity (`MikeMarcinTest`, its token handed to `gh`
   through `GH_TOKEN` in the child's environment, never printed) with
   `--reviewers-second`: its search is answered or refused, and asking for
   its own review is never claimed;
4. checks through `gh` that the pull request has no review request, closes
   it and deletes the branch.

Every `gh` call reads `X-RateLimit-Remaining` and the course stops below
`--rate-floor`.

    python3 tests/scripts/reviewer_live_course.py \\
        --tool /abs/build/live/tests/github_contract/tool_reaction_live_course \\
        --work /abs/scratch/reviewer-live
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
        print(f"reviewer-live:rate-remaining:{remaining}")
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
    branch = f"bpr-reviewers-{stamp}"
    main_sha = gh([f"{repo}/git/ref/heads/main"], floor)["object"]["sha"]
    gh([f"{repo}/git/refs", "--method", "POST"], floor, {"ref": f"refs/heads/{branch}", "sha": main_sha})
    pull = None
    answers = options.work / "answers"
    try:
        gh([f"{repo}/contents/reviewer-course/{stamp}.txt", "--method", "PUT"], floor,
           {"message": "Reviewer course file", "branch": branch, "content": base64.b64encode(b"Reviewer course.\n").decode()})
        pull = gh([f"{repo}/pulls", "--method", "POST"], floor,
                  {"title": f"Reviewer course {stamp}", "head": branch, "base": "main",
                   "body": "Made by tests/scripts/reviewer_live_course.py; closed and its branch deleted when the course ends."})
        number = pull["number"]
        base = {
            "owner": OWNER, "name": REPOSITORY, "number": number,
            "repositoryDatabaseId": repository["id"], "repositoryNode": repository["node_id"],
            "pullRequestDatabaseId": pull["id"], "pullRequestNode": pull["node_id"],
            "head": pull["head"]["sha"], "base": pull["base"]["sha"], "baseRef": pull["base"]["ref"], "headRef": pull["head"]["ref"],
            "author": pull["user"]["login"], "secondNode": second["node_id"], "secondLogin": second["login"],
            "originalBody": "", "editedBody": "", "reviewComment": "", "issueComment": "", "review": "", "thread": "",
        }
        configs: dict[str, Path] = {}
        for name, actor in (("owner", owner), ("second", second)):
            config = dict(base, actorId=actor["id"], actorNode=actor["node_id"], actorLogin=actor["login"])
            configs[name] = options.work / f"{name}.json"
            configs[name].write_text(json.dumps(config))
            (answers / name).mkdir(parents=True, exist_ok=True)
        run_tool(options.tool, "--reviewers", configs["owner"], answers / "owner", floor, False)
        run_tool(options.tool, "--reviewers-second", configs["second"], answers / "second", floor, True)
        requested = gh([f"{repo}/pulls/{number}/requested_reviewers"], floor)
        if requested["users"] or requested["teams"]:
            raise CourseFailure("a-review-request-was-left")
        print(f"reviewer-live:pull:{number}")
    finally:
        if pull is not None:
            gh([f"{repo}/pulls/{pull['number']}", "--method", "PATCH"], floor, {"state": "closed"})
        gh([f"{repo}/git/refs/heads/{branch}", "--method", "DELETE"], floor)
        print(f"reviewer-live:cleaned:{branch}")
    print("reviewer-live:OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CourseFailure as failure:
        print(f"reviewer-live:FAILED:{failure}")
        sys.exit(1)
