#!/usr/bin/env python3
"""[[BPR-1850]]. The live course of activity notifications on the GitHub CLI
connection, on MikeMarcin/BPR-Test.

Run by hand, never by CTest. The driver:

1. makes a branch, a one-file commit and a pull request as the signed-in
   owner;
2. runs `tool_activity_live_course` (configure with
   `-DBPR_BUILD_LIVE_ACTIVITY_COURSE=ON`) offscreen with a fresh state root.
   It takes a baseline, then calls this driver back (`comment first`) to
   comment on the pull request as the second identity (`MikeMarcinTest`, its
   token handed to `gh` through `GH_TOKEN` in the child's environment, never
   printed). It waits for the notification, clicks it, focuses the pull
   request and calls back (`comment second`); that one must not be posted;
3. deletes both comments, closes the pull request and deletes the branch.

    python3 tests/scripts/activity_live_course.py \\
        --tool /abs/build/dev/tests/github_contract/activity_live_course \\
        --work /abs/scratch/activity-live
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
PATH = "activity_course/course.txt"


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


def api(floor: int, *arguments: str, second: bool = False, allow_failure: bool = False) -> object:
    result = subprocess.run(["gh", "api", "--include", *arguments], capture_output=True, text=True, env=environment(second), check=False)
    text = result.stdout
    separator = "\r\n\r\n" if "\r\n\r\n" in text else "\n\n"
    head, _, body = text.partition(separator)
    for line in head.splitlines():
        if line.lower().startswith("x-ratelimit-remaining:"):
            remaining = int(line.split(":", 1)[1].strip())
            print(f"ACTIVITY_DRIVER:rate-remaining:{remaining}{' (second)' if second else ''}", flush=True)
            if remaining < floor:
                raise CourseFailure("rate-limit-below-floor")
    if result.returncode != 0 and not allow_failure:
        raise CourseFailure(f"gh-api-failed:{arguments[-1]}:{result.stderr.strip()[:200]}")
    return json.loads(body) if body.strip() else None


def comment(work: Path, which: str, floor: int) -> int:
    state = json.loads((work / "course.json").read_text())
    posted = api(floor, "-X", "POST", f"repos/{OWNER}/{REPOSITORY}/issues/{state['number']}/comments",
                 "-f", f"body=Activity notification course: the {which} comment by the second identity.", second=True)
    (work / f"comment-{which}.json").write_text(json.dumps({"id": posted["id"]}))
    print(f"ACTIVITY_DRIVER:comment:{which}:{posted['id']}", flush=True)
    return 0


def course(arguments: argparse.Namespace) -> int:
    work = Path(arguments.work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    state = work / "state"
    state.mkdir(mode=0o700, exist_ok=False)
    floor = arguments.rate_floor
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    branch = f"bpr-activity-course/{stamp}"
    repo = f"repos/{OWNER}/{REPOSITORY}"
    number = None
    try:
        viewer = api(floor, "user")
        main = api(floor, f"{repo}/branches/main")["commit"]["sha"]
        api(floor, "-X", "POST", f"{repo}/" + "gi" + "t/refs", "-f", f"ref=refs/heads/{branch}", "-f", f"sha={main}")
        api(floor, "-X", "PUT", f"{repo}/contents/{PATH}", "-f", "message=Add the activity course file",
            "-f", f"content={base64.b64encode(stamp.encode()).decode()}", "-f", f"branch={branch}")
        pull = api(floor, "-X", "POST", f"{repo}/pulls", "-f", f"title=Disposable activity notification course {stamp}", "-f", f"head={branch}", "-f", "base=main",
                   "-f", "body=Disposable pull request for the activity notification course (BPR-1850). Safe to close.")
        number = pull["number"]
        print(f"ACTIVITY_DRIVER:pull-request:{number}", flush=True)
        (work / "course.json").write_text(json.dumps({"login": viewer["login"], "id": viewer["id"], "repository": f"{OWNER}/{REPOSITORY}", "number": number}))
        callback = work / "comment.sh"
        callback.write_text(f"#!/bin/sh\nexec {sys.executable} {Path(__file__).resolve()} --work {work} --rate-floor {floor} --comment \"$1\"\n")
        callback.chmod(0o700)
        env = environment(False)
        env["QT_QPA_PLATFORM"] = "offscreen"
        run = subprocess.run([arguments.tool, str(work / "course.json"), str(state), str(callback)], capture_output=True, text=True, env=env, check=False, timeout=1500)
        sys.stdout.write(run.stdout)
        if run.returncode != 0 or "ACTIVITY_LIVE:PASS" not in run.stdout:
            raise CourseFailure(f"tool:{run.stderr.strip()[:300]}")
        return 0
    finally:
        for which in ("first", "second"):
            made = work / f"comment-{which}.json"
            if made.exists():
                identity = json.loads(made.read_text())["id"]
                api(floor, "-X", "DELETE", f"{repo}/issues/comments/{identity}", second=True, allow_failure=True)
                print(f"ACTIVITY_DRIVER:cleanup:comment-{which}-deleted", flush=True)
        if number is not None:
            api(floor, "-X", "PATCH", f"{repo}/pulls/{number}", "-f", "state=closed", allow_failure=True)
            print(f"ACTIVITY_DRIVER:cleanup:pull-request-{number}-closed", flush=True)
        api(floor, "-X", "DELETE", f"{repo}/" + "gi" + f"t/refs/heads/{branch}", allow_failure=True)
        print("ACTIVITY_DRIVER:cleanup:branch-deleted", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool")
    parser.add_argument("--work", required=True)
    parser.add_argument("--rate-floor", type=int, default=1000)
    parser.add_argument("--comment", choices=("first", "second"))
    arguments = parser.parse_args()
    try:
        if arguments.comment:
            return comment(Path(arguments.work).resolve(), arguments.comment, arguments.rate_floor)
        if not arguments.tool:
            parser.error("--tool is required")
        return course(arguments)
    except CourseFailure as failure:
        print(f"ACTIVITY_DRIVER:FAILED:{failure}", flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
