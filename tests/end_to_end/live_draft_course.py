#!/usr/bin/env python3
"""RFC-BPR-0111 AC01 (BPR-2029): Create draft PR from a local draft on
MikeMarcin/BPR-Test (github.com), through the app's own authorization.

Opt-in and run by hand on macOS; never a registered ctest row. It spends the
active `gh` account's rate limit and pushes one disposable branch with the
author's own Git, as the author would.

What it does:

1. Clones MikeMarcin/BPR-Test into DIR/work, makes a branch
   `bpr-0111-live-<time>` with one commit, and pushes nothing.
2. Runs `live_second_account_course --course live-draft` offscreen, signed in
   through the active `gh` account, in a tests-enabled build composed with
   `DesignatedLivePublication` (creation only on MikeMarcin/BPR-Test). The
   course opens the worktree as a local draft and presses Check GitHub: the
   branch is missing and the push is named. At `at-push` this script runs
   exactly the named command (the author's own Git, credentials and hooks)
   and answers `go-push`. The course checks again and presses Create draft PR.
3. Reads GitHub's own answer with `gh` and writes DIR/evidence.json: the pull
   request's number, draft state, head, author, title and base.
4. With `--cleanup`, closes that pull request and deletes the branch.

It reads `X-RateLimit-Remaining` from every response and stops below
`--minimum-rate` (default 500). Nothing outside MikeMarcin/BPR-Test is touched.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

REPOSITORY = "MikeMarcin/BPR-Test"
REMAINING: list[int] = []
MINIMUM = [500]


def api(method: str, path: str, body: dict | None = None, expect: tuple[int, ...] = (200, 201, 204)) -> tuple[int, object]:
    command = ["gh", "api", "-i", "-X", method, path]
    if body is not None:
        command += ["--input", "-"]
    result = subprocess.run(command, input=json.dumps(body) if body is not None else None, capture_output=True, text=True)
    head, _, payload = result.stdout.partition("\r\n\r\n")
    if not payload and "\n\n" in result.stdout:
        head, _, payload = result.stdout.partition("\n\n")
    lines = head.splitlines()
    status = int(lines[0].split()[1]) if lines and lines[0].startswith("HTTP/") else -1
    for line in lines[1:]:
        name, _, value = line.partition(":")
        if name.strip().lower() == "x-ratelimit-remaining":
            REMAINING.append(int(value.strip()))
    data = json.loads(payload) if payload.strip() else None
    if status not in expect:
        raise SystemExit(f"{method} {path}: HTTP {status}: {payload[:400]} {result.stderr[:200]}")
    if REMAINING and REMAINING[-1] < MINIMUM[0]:
        raise SystemExit(f"rate limit low: {REMAINING[-1]} remaining")
    return status, data


def git(directory: Path, *arguments: str) -> str:
    return subprocess.run(["git", "-C", str(directory), *arguments], capture_output=True, text=True, check=True).stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--course", required=True, type=Path, help="build/dev/tests/end_to_end/live_second_account_course")
    parser.add_argument("--root", required=True, type=Path, help="an empty directory for this run")
    parser.add_argument("--cleanup", action="store_true")
    parser.add_argument("--minimum-rate", type=int, default=500)
    arguments = parser.parse_args()
    MINIMUM[0] = arguments.minimum_rate
    root = arguments.root.resolve()
    root.mkdir(parents=True, exist_ok=False)
    api("GET", f"repos/{REPOSITORY}")
    print(f"rate before: {REMAINING[-1]}", flush=True)

    work = root / "work"
    subprocess.run(["git", "clone", "--quiet", f"https://github.com/{REPOSITORY}.git", str(work)], check=True)
    branch = f"bpr-0111-live-{int(time.time())}"
    git(work, "checkout", "--quiet", "-b", branch)
    (work / "bpr-0111").mkdir(exist_ok=True)
    (work / "bpr-0111" / f"{branch}.md").write_text(f"Disposable change for the RFC-BPR-0111 live course ({branch}).\n")
    git(work, "add", "-A")
    git(work, "-c", "user.name=BPR live course", "-c", "user.email=noreply@example.invalid", "commit", "--quiet", "-m", f"RFC-BPR-0111 live course {branch}")
    head = git(work, "rev-parse", "HEAD")
    print(f"branch {branch} at {head}", flush=True)

    command = [str(arguments.course), "--course", "live-draft", "--root", str(root), "--pull", "1", "--path", str(work)]
    environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    log = (root / "course.log").open("w")
    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env=environment)
    print(f"course pid {process.pid}", flush=True)
    pushed = None
    while process.poll() is None:
        if pushed is None and (root / "at-push").exists():
            named = json.loads((root / "at-push").read_text())["command"]
            expected = f"git -C {work} push https://github.com/{REPOSITORY}.git {head}:refs/heads/{branch}"
            if named != expected:
                process.terminate()
                raise SystemExit(f"the named push is not the expected one:\n  {named}\n  {expected}")
            # The author's own Git runs exactly the command BPR named.
            pushed = subprocess.run(named, shell=True, capture_output=True, text=True)
            print(f"pushed: exit {pushed.returncode}", flush=True)
            (root / "go-push").write_text(str(pushed.returncode) + "\n")
        time.sleep(1)
    log.close()
    print(f"course exit {process.returncode}", flush=True)

    evidence: dict = {"course_exit": process.returncode, "branch": branch, "head": head, "pushed": pushed is not None and pushed.returncode == 0}
    _, pulls = api("GET", f"repos/{REPOSITORY}/pulls?state=all&head=MikeMarcin:{branch}")
    evidence["pull_requests"] = [{"number": p["number"], "draft": p["draft"], "state": p["state"], "head": p["head"]["sha"], "head_ref": p["head"]["ref"],
                                  "base": p["base"]["ref"], "author": p["user"]["login"], "title": p["title"], "body": p["body"], "url": p["html_url"]} for p in pulls]
    course = root / "live-draft.json"
    evidence["course"] = json.loads(course.read_text()) if course.exists() else None
    if arguments.cleanup:
        for pull in pulls:
            if pull["state"] == "open":
                api("PATCH", f"repos/{REPOSITORY}/pulls/{pull['number']}", {"state": "closed"})
        if evidence["pushed"]:
            api("DELETE", f"repos/{REPOSITORY}/git/refs/heads/{branch}")
        evidence["cleaned_up"] = True
    evidence["rate_remaining_after"] = REMAINING[-1] if REMAINING else None
    (root / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence, indent=2), flush=True)
    ok = process.returncode == 0 and len(pulls) == 1 and pulls[0]["draft"] and pulls[0]["head"]["sha"] == head and pulls[0]["user"]["login"] == "MikeMarcin"
    print("live-draft-course:" + ("pass" if ok else "fail"), flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
