#!/usr/bin/env python3
"""[[BPR-1774]]. The live description-edit course on MikeMarcin/BPR-Test.

Run by hand, never by CTest. The owner designated MikeMarcin/BPR-Test as the
live GitHub target (2026-10-05); it is a constant here, not an argument. The
driver:

1. makes a branch, a one-file commit and a pull request opened by the
   signed-in owner, whose body has CRLF line endings, as GitHub's web editor
   writes them;
2. runs `tool_reaction_live_course` (configure with
   `-DBPR_BUILD_LIVE_REACTION_COURSE=ON`) as the owner with `--description`:
   the shipped session edits the body once, saves the same text again
   (nothing sent: its own read finds GitHub already has it, byte for byte),
   is refused a stale original as a conflict (nothing sent) and restores the
   body, which this driver then reads back through `gh` byte for byte;
3. runs it as the second identity (`MikeMarcinTest`, its token handed to `gh`
   through `GH_TOKEN` in the child's environment, never printed) with
   `--description-second`: no Edit is offered to it, and the session writes
   nothing;
4. checks the body is the original again, closes the pull request and
   deletes the branch.

Every `gh` call reads `X-RateLimit-Remaining` and the course stops below
`--rate-floor`.

    python3 tests/scripts/description_live_course.py \\
        --tool /abs/build/dev/tests/github_contract/tool_reaction_live_course \\
        --work /abs/scratch/description-live
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
ORIGINAL = "Made by tests/scripts/description_live_course.py.\r\n\r\nClosed and its branch deleted when the course ends.\r\n"
EDITED = ORIGINAL + "\r\nEdited through the shipped description session: `updatePullRequest`, body only.\n"


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
        print(f"description-live:rate-remaining:{remaining}")
        if remaining < floor:
            raise CourseFailure(f"rate-limit-below-floor:{remaining}")
    return json.loads(body) if body.strip() else None


def run_tool(tool: Path, mode: str, config: Path, output: Path, floor: int, second: bool) -> None:
    result = subprocess.run([str(tool), mode, str(config), str(output), str(floor)], capture_output=True, text=True, check=False,
                            env=environment(second))
    sys.stdout.write(result.stdout)
    if result.returncode != 0 or "REACTION_LIVE:OK" not in result.stdout:
        raise CourseFailure(f"tool-{mode}-failed:{result.stderr[-400:]}")


def body_of(repo: str, number: int, floor: int) -> str:
    return gh([f"{repo}/pulls/{number}"], floor)["body"] or ""


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
    branch = f"bpr-description-{stamp}"
    main_sha = gh([f"{repo}/git/ref/heads/main"], floor)["object"]["sha"]
    gh([f"{repo}/git/refs", "--method", "POST"], floor, {"ref": f"refs/heads/{branch}", "sha": main_sha})
    pull = None
    answers = options.work / "answers"
    try:
        gh([f"{repo}/contents/description-course/{stamp}.txt", "--method", "PUT"], floor,
           {"message": "Description course file", "branch": branch, "content": base64.b64encode(b"Description course.\n").decode()})
        pull = gh([f"{repo}/pulls", "--method", "POST"], floor,
                  {"title": f"Description course {stamp}", "head": branch, "base": "main", "body": ORIGINAL})
        number = pull["number"]
        if pull["user"]["login"] != OWNER or body_of(repo, number, floor) != ORIGINAL:
            raise CourseFailure("pull-request-not-as-made")
        base = {
            "owner": OWNER, "name": REPOSITORY, "number": number,
            "repositoryDatabaseId": repository["id"], "repositoryNode": repository["node_id"],
            "pullRequestDatabaseId": pull["id"], "pullRequestNode": pull["node_id"],
            "head": pull["head"]["sha"], "base": pull["base"]["sha"], "baseRef": pull["base"]["ref"], "headRef": pull["head"]["ref"],
            "author": pull["user"]["login"], "originalBody": ORIGINAL, "editedBody": EDITED,
            "reviewComment": "", "issueComment": "", "review": "", "thread": "",
        }
        configs: dict[str, Path] = {}
        for name, actor in (("owner", owner), ("second", second)):
            config = dict(base, actorId=actor["id"], actorNode=actor["node_id"], actorLogin=actor["login"])
            configs[name] = options.work / f"{name}.json"
            configs[name].write_text(json.dumps(config))
            (answers / name).mkdir(parents=True, exist_ok=True)
        run_tool(options.tool, "--description", configs["owner"], answers / "owner", floor, False)
        if body_of(repo, number, floor) != ORIGINAL:
            raise CourseFailure("owner-course-did-not-restore")
        run_tool(options.tool, "--description-second", configs["second"], answers / "second", floor, True)
        if body_of(repo, number, floor) != ORIGINAL:
            raise CourseFailure("second-identity-changed-the-body")
        print(f"description-live:pull:{number}")
    finally:
        if pull is not None:
            gh([f"{repo}/pulls/{pull['number']}", "--method", "PATCH"], floor, {"state": "closed"})
        gh([f"{repo}/git/refs/heads/{branch}", "--method", "DELETE"], floor)
        print(f"description-live:cleaned:{branch}")
    print("description-live:OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CourseFailure as failure:
        print(f"description-live:FAILED:{failure}")
        sys.exit(1)
