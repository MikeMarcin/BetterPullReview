#!/usr/bin/env python3
"""[[BPR-1735]] live course on MikeMarcin/BPR-Test (github.com): comments
outside every hunk of a Whole PR diff, through the shipped composition.

Builds BPR-Test #124's shape with the Git data API: a scratch base branch
holding PREFIX/offhunk.txt, 80 numbered lines, and a head branch changing its
line 10, then opens a pull request between them. It runs
`live_second_account_course --course whole-pr-comment` offscreen, signed in
through the active `gh` account, which writes where the composition's core
places a comment on head lines 60, 14 and 13 and what the editor says, and
checks the owner's rule (2026-10-22): line 13 (three lines from the change) is
a line comment, lines 14 and 60 (four and more) are file comments whose
permalink names the head's line.

A tests-enabled composition holds no publication grant, so while the course
waits at `at-post` this script publishes exactly those placements through gh
and checks what GitHub answered:

- the review the old build would have sent, carrying line 13 and line 60 as
  line comments, is refused whole with 422 and creates nothing (what the hold
  prevents);
- line 60, held, is published on its own as the composition compiles a file
  comment: `subject_type` file at the head, the body the permalink to line 60,
  a blank line and the words; GitHub answers 201 with a file subject;
- the rest (line 13) is submitted as one review: 200, one comment at RIGHT 13;
- line 14 published alone goes straight to a file comment (201), and natively
  it is refused with 422, so the edge is GitHub's too.

It reads `X-RateLimit-Remaining` from each response and stops below
`--minimum-rate` (default 500). `--cleanup` closes the pull request and
deletes both branches. Nothing outside MikeMarcin/BPR-Test is touched.
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


def api(method: str, path: str, body: dict | None = None, expect: tuple[int, ...] = (200, 201)) -> tuple[int, dict | list | None]:
    command = ["gh", "api", "-i", "-X", method, path]
    if body is not None:
        command += ["--input", "-"]
    result = subprocess.run(command, input=json.dumps(body) if body is not None else None, capture_output=True, text=True)
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
    if status not in expect:
        raise SystemExit(f"{method} {path}: status {status}, expected {expect}: {payload[:400]}")
    try:
        return status, json.loads(payload) if payload.strip() else None
    except json.JSONDecodeError:
        return status, None


def guard(minimum: int) -> None:
    if REMAINING and REMAINING[-1] < minimum:
        raise SystemExit(f"X-RateLimit-Remaining {REMAINING[-1]} is below {minimum}; stopping")


def numbered(changed: bool) -> str:
    return "".join(("line 10 changed by the head\n" if changed and number == 10 else f"line {number:02d} of the BPR-1735 off-hunk fixture\n") for number in range(1, 81))


def commit_with(parent: str, path: str, text: str, message: str) -> str:
    _, blob = api("POST", f"repos/{REPOSITORY}/git/blobs", {"content": text, "encoding": "utf-8"})
    _, parentCommit = api("GET", f"repos/{REPOSITORY}/git/commits/{parent}")
    _, tree = api("POST", f"repos/{REPOSITORY}/git/trees", {"base_tree": parentCommit["tree"]["sha"], "tree": [{"path": path, "mode": "100644", "type": "blob", "sha": blob["sha"]}]})
    _, made = api("POST", f"repos/{REPOSITORY}/git/commits", {"message": message, "tree": tree["sha"], "parents": [parent]})
    return made["sha"]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--prefix", default="bpr1735-live")
    parser.add_argument("--minimum-rate", type=int, default=500)
    parser.add_argument("--cleanup", action="store_true")
    options = parser.parse_args()
    work = options.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    path = f"{options.prefix}/offhunk.txt"
    baseRef = f"{options.prefix}/{stamp}/base"
    headRef = f"{options.prefix}/{stamp}/head"
    evidence: dict = {"stamp": stamp, "path": path}

    _, main_ref = api("GET", f"repos/{REPOSITORY}/git/ref/heads/main")
    guard(options.minimum_rate)
    evidence["rate_before"] = REMAINING[-1] if REMAINING else None
    base = commit_with(main_ref["object"]["sha"], path, numbered(False), "BPR-1735 live course: 80-line base")
    head = commit_with(base, path, numbered(True), "BPR-1735 live course: change line 10")
    api("POST", f"repos/{REPOSITORY}/git/refs", {"ref": f"refs/heads/{baseRef}", "sha": base})
    api("POST", f"repos/{REPOSITORY}/git/refs", {"ref": f"refs/heads/{headRef}", "sha": head})
    _, pull = api("POST", f"repos/{REPOSITORY}/pulls", {"title": "BPR-1735 live course (throwaway)", "head": headRef, "base": baseRef, "body": "Throwaway pull request for BPR-1735's live course; closed by it."})
    number = pull["number"]
    evidence.update({"pull": number, "base": base, "head": head})
    print(f"opened #{number} head {head}", flush=True)
    course = None
    try:
        course_root = work / "course"
        course_root.mkdir(exist_ok=True)
        for stale in course_root.glob("*-post"):
            stale.unlink()
        executable = options.build / "tests/end_to_end/live_second_account_course"
        environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")
        log = (work / "course.log").open("w")
        course = subprocess.Popen([str(executable), "--course", "whole-pr-comment", "--root", str(course_root), "--pull", str(number), "--path", path], stdout=log, stderr=subprocess.STDOUT, env=environment)
        evidence["course_pid"] = course.pid
        deadline = time.time() + 900
        while not (course_root / "at-post").exists():
            if course.poll() is not None or time.time() > deadline:
                raise SystemExit(f"the course stopped before its placements: {(work / 'course.log').read_text()[-2000:]}")
            time.sleep(1)
        placements = {row["line"]: row for row in map(json.loads, (course_root / "whole-pr-comment.jsonl").read_text().splitlines())}
        evidence["placements"] = placements
        permalink = f"https://github.com/{REPOSITORY}/blob/{head}/{path}#L"
        for line in (59, 13):
            row = placements[line]
            if row["placement"] != 3 or row["refusal"] != 7 or row["commit"] != head or row["blocks"] or "Publish adds a comment on the file instead" not in row["notice"]:
                raise SystemExit(f"line {line + 1} is not placed as a file comment at the head: {row}")
        if placements[12]["placement"] is not None or placements[12]["notice"]:
            raise SystemExit(f"line 13 is not placed on itself: {placements[12]}")

        # What the old build sent: the whole review, line 60 among its comments.
        status, refused = api("POST", f"repos/{REPOSITORY}/pulls/{number}/reviews", {"commit_id": head, "event": "COMMENT", "body": "", "comments": [
            {"path": path, "line": 13, "side": "RIGHT", "body": "BPR-1735 live: in the hunk, three lines after the change."},
            {"path": path, "line": 60, "side": "RIGHT", "body": "BPR-1735 live: outside every hunk."}]}, expect=(422,))
        evidence["review_with_off_hunk"] = {"status": status, "answer": refused}
        _, reviews = api("GET", f"repos/{REPOSITORY}/pulls/{number}/reviews")
        if reviews:
            raise SystemExit("the refused review left a review behind")
        guard(options.minimum_rate)

        # Held line 60 published on its own as the composition's file comment.
        words = "BPR-1735 live: outside every hunk."
        status, file60 = api("POST", f"repos/{REPOSITORY}/pulls/{number}/comments", {"body": f"{permalink}60\n\n{words}", "commit_id": head, "path": path, "subject_type": "file"}, expect=(201,))
        if file60.get("subject_type") != "file" or file60.get("commit_id") != head or not file60["body"].startswith(f"{permalink}60\n\n"):
            raise SystemExit(f"line 60's file comment is not GitHub's file subject: {file60}")
        evidence["file_comment_60"] = {"status": status, "id": file60["id"], "subject_type": file60["subject_type"], "line": file60.get("line"), "commit_id": file60["commit_id"]}

        # The rest submitted as one review.
        status, review = api("POST", f"repos/{REPOSITORY}/pulls/{number}/reviews", {"commit_id": head, "event": "COMMENT", "body": "", "comments": [
            {"path": path, "line": 13, "side": "RIGHT", "body": "BPR-1735 live: in the hunk, three lines after the change."}]}, expect=(200,))
        # The review's own comment list reports positions; the pull request's
        # comment list reports line and side.
        _, listed = api("GET", f"repos/{REPOSITORY}/pulls/{number}/comments")
        held = [comment for comment in listed if comment.get("pull_request_review_id") == review["id"]]
        if len(held) != 1 or held[0].get("line") != 13 or held[0].get("side") != "RIGHT":
            raise SystemExit(f"the review does not hold exactly line 13: {held}")
        evidence["review_rest"] = {"status": status, "id": review["id"], "comments": [{"line": c.get("line"), "side": c.get("side")} for c in held]}
        guard(options.minimum_rate)

        # Line 14 alone: natively GitHub refuses it (the edge), as a file comment it is taken.
        status, native14 = api("POST", f"repos/{REPOSITORY}/pulls/{number}/comments", {"body": "BPR-1735 live: four lines after the change.", "commit_id": head, "path": path, "line": 14, "side": "RIGHT"}, expect=(422,))
        evidence["native_14"] = {"status": status, "answer": native14}
        status, file14 = api("POST", f"repos/{REPOSITORY}/pulls/{number}/comments", {"body": f"{permalink}14\n\nBPR-1735 live: four lines after the change.", "commit_id": head, "path": path, "subject_type": "file"}, expect=(201,))
        evidence["file_comment_14"] = {"status": status, "id": file14["id"], "subject_type": file14.get("subject_type")}
        _, comments = api("GET", f"repos/{REPOSITORY}/pulls/{number}/comments")
        evidence["comments_total"] = len(comments)
        if len(comments) != 3:
            raise SystemExit(f"expected three comments (two file, one line), found {len(comments)}")
        (course_root / "go-post").write_text("published\n")
        course.wait(timeout=300)
        evidence["course_exit"] = course.returncode
        if course.returncode != 0:
            raise SystemExit("the course failed after its placements")
        print("bpr1735-live:pass", flush=True)
        return 0
    finally:
        if course is not None and course.poll() is None:
            course.terminate()
            course.wait(timeout=60)
        if options.cleanup:
            api("PATCH", f"repos/{REPOSITORY}/pulls/{number}", {"state": "closed"})
            api("DELETE", f"repos/{REPOSITORY}/git/refs/heads/{headRef}", expect=(204,))
            api("DELETE", f"repos/{REPOSITORY}/git/refs/heads/{baseRef}", expect=(204,))
            evidence["cleanup"] = "closed, branches deleted"
        evidence["rate_after"] = REMAINING[-1] if REMAINING else None
        evidence["rate_minimum_seen"] = min(REMAINING) if REMAINING else None
        (work / "evidence.json").write_text(json.dumps(evidence, indent=1, default=str))
        print(json.dumps({key: evidence.get(key) for key in ("pull", "head", "rate_before", "rate_after", "rate_minimum_seen", "cleanup")}), flush=True)


if __name__ == "__main__":
    sys.exit(main())
