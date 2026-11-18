#!/usr/bin/env python3
"""RFC-BPR-0099 AC08 and RFC-BPR-0105 AC08: the agent-run live session on
MikeMarcin/BPR-Test (github.com), with no human step.

Opt-in and run by hand on macOS; never a registered ctest row. It spends the
active `gh` account's rate limit and, with `--agent`, the installed Claude
Code account (one short Haiku question).

What it does:

1. Builds a disposable pull request with the Git data API: a base branch
   holding PREFIX/retry.cpp and PREFIX/helper.cpp (40 numbered lines each), a
   head that changes retry.cpp lines 10-11 and helper.cpp lines 20 and 35,
   and a prepared next commit that puts helper.cpp line 20 back, so stop 2
   (helper.cpp lines 21-22, context of the line-20 change) is left at an
   earlier version while its lines stay the same. The pull request is opened
   by the active account (the tour's author).
2. With `--approve new`, opens a second disposable pull request as the test
   account (`gh auth token --user MikeMarcinTest`, per command; never
   `gh auth switch`), from its fork MikeMarcinTest/BPR-Test (the test account
   is not a collaborator), for the active account to approve. `--approve N`
   approves an existing pull request instead.
3. Runs `live_second_account_course --course live-session` offscreen, signed
   in through the active `gh` account, in a tests-enabled build composed with
   `DesignatedLivePublication` (publication only to MikeMarcin/BPR-Test, only
   through the window's own Publish and Approve). When the course writes
   `at-push` it fast-forwards the head to the prepared commit and answers
   `go-push`.
4. Reads GitHub's own answer with `gh` and writes DIR/evidence.json: the tour
   pull request's reviews, review comments, conversation comments and body;
   the approved pull request's reviews.
5. With `--cleanup`, closes the pull requests it opened and deletes their
   branches.

It reads `X-RateLimit-Remaining` from every response and stops below
`--minimum-rate` (default 500). Nothing outside MikeMarcin/BPR-Test is
touched.
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

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import make_contained_temporary_directory  # noqa: E402

REPOSITORY = "MikeMarcin/BPR-Test"
TEST_ACCOUNT = "MikeMarcinTest"
# The test account is not a collaborator: it opens pull requests from its fork.
TEST_FORK = "MikeMarcinTest/BPR-Test"
REMAINING: list[int] = []
MINIMUM = [500]


def api(method: str, path: str, body: dict | None = None, expect: tuple[int, ...] = (200, 201), token: str | None = None) -> tuple[int, object]:
    command = ["gh", "api", "-i", "-X", method, path]
    if body is not None:
        command += ["--input", "-"]
    environment = dict(os.environ)
    if token is not None:
        environment["GH_TOKEN"] = token
    result = subprocess.run(command, input=json.dumps(body) if body is not None else None, capture_output=True, text=True, env=environment)
    raw = result.stdout
    head, _, payload = raw.partition("\r\n\r\n")
    if not payload and "\n\n" in raw:
        head, _, payload = raw.partition("\n\n")
    lines = head.splitlines()
    status = int(lines[0].split()[1]) if lines and lines[0].startswith("HTTP/") else -1
    for line in lines[1:]:
        name, _, value = line.partition(":")
        if name.strip().lower() == "x-ratelimit-remaining":
            REMAINING.append(int(value.strip()))
    data = json.loads(payload) if payload.strip() else None
    if status not in expect:
        raise SystemExit(f"{method} {path}: HTTP {status}: {payload[:400]} {result.stderr[:200]}")
    if token is None and REMAINING and REMAINING[-1] < MINIMUM[0]:
        raise SystemExit(f"rate limit low: {REMAINING[-1]} remaining")
    return status, data


def numbered(name: str, changes: dict[int, str]) -> str:
    lines = [f"int {name}_{n}() {{ return {n}; }}" for n in range(1, 41)]
    for line, text in changes.items():
        lines[line - 1] = text
    return "\n".join(lines) + "\n"


def commit(parent: str, base_tree: str, files: dict[str, str], message: str, repository: str = REPOSITORY, token: str | None = None) -> str:
    tree = [{"path": path, "mode": "100644", "type": "blob", "content": content} for path, content in files.items()]
    _, made = api("POST", f"repos/{repository}/git/trees", {"base_tree": base_tree, "tree": tree}, token=token)
    _, created = api("POST", f"repos/{repository}/git/commits", {"message": message, "tree": made["sha"], "parents": [parent]}, token=token)
    return created["sha"]


def tree_of(sha: str, repository: str = REPOSITORY, token: str | None = None) -> str:
    _, data = api("GET", f"repos/{repository}/git/commits/{sha}", token=token)
    return data["tree"]["sha"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--course", required=True, type=Path, help="build/dev/tests/end_to_end/live_second_account_course")
    parser.add_argument("--root", type=Path, help="a directory for the run, made here (it must not exist); default: a new private temporary directory")
    parser.add_argument("--reads", default="1,2,3,43", help="pull requests opened first (read only)")
    parser.add_argument("--approve", default="new", help="'new' (a disposable pull request by the test account), a number, or 'none'")
    parser.add_argument("--agent", default="", help="installed Claude Code to ask one question (empty: no agent)")
    parser.add_argument("--minimum-rate", type=int, default=500)
    parser.add_argument("--cleanup", action="store_true")
    arguments = parser.parse_args()
    MINIMUM[0] = arguments.minimum_rate
    if arguments.root is None:
        root = make_contained_temporary_directory(prefix="live-session-").resolve()
    else:
        root = arguments.root.resolve()
        root.mkdir(mode=0o700)  # refuses a root that is already there
    os.chmod(root, 0o700)
    print(f"root: {root}", flush=True)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    prefix = f"live-session-{stamp}"
    retry, helper = f"{prefix}/retry.cpp", f"{prefix}/helper.cpp"

    _, repository = api("GET", f"repos/{REPOSITORY}")
    _, main_ref = api("GET", f"repos/{REPOSITORY}/git/ref/heads/{repository['default_branch']}")
    root_sha = main_ref["object"]["sha"]
    base = commit(root_sha, tree_of(root_sha), {retry: numbered("retry", {}), helper: numbered("helper", {})}, f"{prefix}: base")
    head = commit(base, tree_of(base), {
        retry: numbered("retry", {10: "int retry_10() { return 10 << 1; }  // bounded", 11: "int retry_11() { return 11 << 1; }  // bounded"}),
        helper: numbered("helper", {20: "int helper_20() { return 20 * 2; }", 35: "int helper_35() { return 35 * 2; }"}),
    }, f"{prefix}: bound the doubling")
    after = commit(head, tree_of(head), {helper: numbered("helper", {35: "int helper_35() { return 35 * 2; }"})}, f"{prefix}: keep helper_20 as it was")
    branches = [f"{prefix}/base", f"{prefix}/tour"]
    api("POST", f"repos/{REPOSITORY}/git/refs", {"ref": f"refs/heads/{branches[0]}", "sha": base})
    api("POST", f"repos/{REPOSITORY}/git/refs", {"ref": f"refs/heads/{branches[1]}", "sha": head})
    _, tour_pull = api("POST", f"repos/{REPOSITORY}/pulls", {
        "title": f"Live session course {stamp}: tour",
        "head": branches[1], "base": branches[0],
        "body": "Disposable pull request for the agent-run live session (RFC-BPR-0099 AC08). Closed by the course.",
    })
    opened = [tour_pull["number"]]
    approve = 0
    fork_branch = ""
    token = None
    if arguments.approve == "new":
        # Never printed or logged; used per command for the test account.
        token = subprocess.run(["gh", "auth", "token", "--user", TEST_ACCOUNT], capture_output=True, text=True, check=True).stdout.strip()
        _, fork_main = api("GET", f"repos/{TEST_FORK}/git/ref/heads/main", token=token)
        fork_root = fork_main["object"]["sha"]
        approve_head = commit(fork_root, tree_of(fork_root, TEST_FORK, token), {f"{prefix}/approve.txt": "approve me\n"}, f"{prefix}: for approval", TEST_FORK, token)
        fork_branch = f"{prefix}/approve"
        api("POST", f"repos/{TEST_FORK}/git/refs", {"ref": f"refs/heads/{fork_branch}", "sha": approve_head}, token=token)
        _, approve_pull = api("POST", f"repos/{REPOSITORY}/pulls", {
            "title": f"Live session course {stamp}: approve",
            "head": f"{TEST_ACCOUNT}:{fork_branch}", "base": repository["default_branch"],
            "body": "Disposable pull request for the agent-run live session (RFC-BPR-0105 AC08), opened by the test account.",
        }, token=token)
        approve = approve_pull["number"]
        opened.append(approve)
    elif arguments.approve != "none":
        approve = int(arguments.approve)
    print(f"prepared: tour=#{tour_pull['number']} approve=#{approve} head={head} next={after} remaining={REMAINING[-1]}", flush=True)

    command = [str(arguments.course), "--course", "live-session", "--root", str(root), "--pull", str(tour_pull["number"]),
               "--path", retry, "--second", helper, "--first-stop", "9", "--second-stop", "20", "--reads", arguments.reads]
    if approve:
        command += ["--approve", str(approve)]
    if arguments.agent:
        command += ["--agent", arguments.agent]
    environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    log = (root / "course.log").open("w")
    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env=environment)
    print(f"course pid {process.pid}", flush=True)
    pushed = False
    while process.poll() is None:
        if not pushed and (root / "at-push").exists():
            api("PATCH", f"repos/{REPOSITORY}/git/refs/heads/{branches[1]}", {"sha": after, "force": False})
            pushed = True
            (root / "go-push").write_text(after + "\n")
            print(f"pushed {after}", flush=True)
        time.sleep(1)
    log.close()
    print(f"course exit {process.returncode}", flush=True)

    number = tour_pull["number"]
    _, pull = api("GET", f"repos/{REPOSITORY}/pulls/{number}")
    _, reviews = api("GET", f"repos/{REPOSITORY}/pulls/{number}/reviews")
    _, review_comments = api("GET", f"repos/{REPOSITORY}/pulls/{number}/comments")
    _, conversation = api("GET", f"repos/{REPOSITORY}/issues/{number}/comments")
    evidence = {
        "course_exit": process.returncode,
        "tour": {
            "number": number, "head": pull["head"]["sha"], "pushed": pushed, "body": pull["body"],
            "reviews": [{"id": r["id"], "user": r["user"]["login"], "state": r["state"], "commit": r["commit_id"], "body": r["body"]} for r in reviews],
            "review_comments": [{"id": c["id"], "user": c["user"]["login"], "path": c["path"], "line": c.get("line"), "side": c.get("side"), "commit": c["commit_id"], "review": c.get("pull_request_review_id"), "body": c["body"]} for c in review_comments],
            "conversation": [{"id": c["id"], "user": c["user"]["login"], "body": c["body"], "url": c["html_url"]} for c in conversation],
        },
    }
    if approve:
        _, approve_reviews = api("GET", f"repos/{REPOSITORY}/pulls/{approve}/reviews")
        _, approve_pull = api("GET", f"repos/{REPOSITORY}/pulls/{approve}")
        evidence["approve"] = {"number": approve, "author": approve_pull["user"]["login"],
                               "reviews": [{"id": r["id"], "user": r["user"]["login"], "state": r["state"], "commit": r["commit_id"], "submitted_at": r["submitted_at"]} for r in approve_reviews]}
    evidence["rate_remaining"] = REMAINING[-1] if REMAINING else None
    (root / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps({k: v for k, v in evidence.items() if k != "tour"} | {"tour_reviews": evidence["tour"]["reviews"], "tour_comments": len(evidence["tour"]["review_comments"]), "conversation": len(evidence["tour"]["conversation"])}, indent=1)[:4000])

    if arguments.cleanup:
        for opened_number in opened:
            api("PATCH", f"repos/{REPOSITORY}/pulls/{opened_number}", {"state": "closed"})
        for branch in reversed(branches):
            api("DELETE", f"repos/{REPOSITORY}/git/refs/heads/{branch}", expect=(204,))
        if fork_branch:
            api("DELETE", f"repos/{TEST_FORK}/git/refs/heads/{fork_branch}", expect=(204,), token=token)
        print(f"closed {opened}; deleted {branches} and {TEST_FORK}:{fork_branch}", flush=True)
    else:
        (root / "leftovers.json").write_text(json.dumps({"pulls": opened, "branches": branches, "fork_branch": fork_branch}) + "\n")
        print(f"left open: {opened}; branches {branches}", flush=True)
    return 0 if process.returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
