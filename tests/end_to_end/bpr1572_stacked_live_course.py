#!/usr/bin/env python3
"""[[BPR-1572]] Live stacked pull request parity course on MikeMarcin/BPR-Test.

Builds a stacked pull request B whose base is branch A (A forked from a
scratch main, B three commits on A), then lets A take B's commits while
GitHub keeps B's recorded base (`base.sha`):

    --variant merge (default)
    1-open           A at a1, B = a1 + b1 + b2 + b3; both merge bases agree
    2-A-merges-b1    A merges b1: the base branch's merge base is b1, the
                     recorded base's stays a1, and GitHub's Files changed
                     still lists x.txt, y.txt and z.txt (from a1)
    3-A-merges-all   A merges all of B: GitHub marks B merged (closed)

    --variant ff
    2-A-ff-to-b1     A fast-forwarded to b1 (merge bases b1 and a1)
    3-A-ff-to-b2     A fast-forwarded to b2 (merge bases b2 and a1)
    4-push-to-B      an ordinary push to B: `base.sha` stays a1

    --variant rebase
    2-A-rebased      A force-pushed onto a newer main: GitHub resets
                     `base.sha` to A's new tip; both merge bases agree
    3-B-rebased      B rebased onto A and force-pushed

    --variant force
    2-A-advances     A gains its own commit (fast-forward): nothing moves
    3-A-forced-to-b1 A force-pushed from that commit back to b1, a
                     descendant of `base.sha` the head shares: GitHub resets
                     `base.sha` to b1 within seconds

After each step it waits only until GitHub reports the step's head, then runs
the shipped composition offscreen through `live_second_account_course
--course parity` on one persistent data root at once, while a thread records
GitHub's `pulls/N` and file list every second (`--timeline`, the window in
which GitHub replaces `base.sha` after a forced base push). A cold-start course
reads the pull request tens of seconds after it starts, past that window; the
window itself is covered deterministically by
`application_merge_base_public_path` (stacked reset course) and the session
tests. Whole PR is then checked against GitHub with `bpr1560_live_parity.py`.

`--negative-control` (a build that refuses, or uses the branch's merge base)
expects step 2 to differ from GitHub and requires the course itself to show
why: either it compared and differed, or it said Whole PR has no base
("…where this pull request branched…"). Any other failure is a failure.

`--left-wire` performs, after the stacked step, the exact reads a LEFT
publication makes (base branch, recorded base, ancestry of the resolved tip,
the pull request again) and posts a LEFT comment through `gh` on a line only
the recorded base's comparison has, then deletes it. Publishing from the app
itself needs the owner's attended designated-live approval.

`--record FILE` appends one JSON line per step: GitHub's state (pull, files,
web .diff file set, both compares, commit list), the timeline, the app's
lines and the verdict. Run by hand on macOS; it stops when
`X-RateLimit-Remaining` falls below 500:

    python3 tests/end_to_end/bpr1572_stacked_live_course.py --build build/dev \\
        --work DIR --prefix bpr1572-live [--variant merge|ff|rebase|force] \\
        [--negative-control] [--left-wire] [--record FILE] [--cleanup]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import threading
import time
import urllib.request
from pathlib import Path

REPOSITORY = "MikeMarcin/BPR-Test"
HERE = Path(__file__).resolve().parent


def run(*command: str, cwd: Path | None = None, check: bool = True) -> str:
    for attempt in range(5):
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True)
        transient = any(word in result.stderr for word in ("i/o timeout", "connection reset", "remote rejected", "502", "504"))
        if result.returncode == 0 or not transient or command[:2] == ("gh", "pr"):
            break
        time.sleep(5 + 5 * attempt)
    if check and result.returncode != 0:
        raise RuntimeError(f"{' '.join(command)} failed: {result.stderr.strip()}")
    return result.stdout


def gh_json(path: str) -> dict:
    return json.loads(run("gh", "api", path))


def web_diff_files(number: int) -> list[str]:
    """The file set of GitHub's own web diff for the pull request."""
    request = urllib.request.Request(f"https://github.com/{REPOSITORY}/pull/{number}.diff", headers={"User-Agent": "bpr1572-live-course"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            text = response.read().decode("utf-8", "replace")
    except OSError as error:
        return [f"<unavailable: {error}>"]
    return sorted({path.split("/")[-1] for path in re.findall(r"^diff --git a/\S+ b/(\S+)$", text, re.M)})


def remaining() -> int:
    for line in run("gh", "api", "-i", f"/repos/{REPOSITORY}").splitlines():
        if line.lower().startswith("x-ratelimit-remaining:"):
            return int(line.split(":", 1)[1])
    return 0


class Course:
    def __init__(self, arguments: argparse.Namespace) -> None:
        self.arguments = arguments
        self.work = Path(arguments.work).resolve()
        self.repo = self.work / "repo"
        self.root = self.work / "app-root"
        self.prefix = arguments.prefix
        self.main, self.a, self.b = (f"{self.prefix}-{name}" for name in ("main", "a", "b"))
        self.pull = 0
        self.head: str | None = None
        self.commits: dict[str, str] = {}
        self.results: list[tuple[str, bool]] = []

    def git(self, *arguments: str) -> str:
        return run("git", "-c", "user.name=Michael Marcin", "-c", "user.email=mike.marcin@gmail.com", *arguments, cwd=self.repo).strip()

    def change(self, label: str, name: str, line: int, text: str) -> str:
        target = self.repo / self.prefix / name
        lines = target.read_text().splitlines()
        lines[line - 1] = text
        target.write_text("\n".join(lines) + "\n")
        self.git("add", "-A")
        self.git("commit", "-q", "-m", f"{self.prefix}: {label}")
        self.commits[label] = self.git("rev-parse", "HEAD")
        return self.commits[label]

    def push(self, sha: str, branch: str) -> None:
        self.git("push", "-q", "origin", f"{sha}:refs/heads/{branch}")

    def setup(self) -> None:
        self.work.mkdir(parents=True, exist_ok=True)
        self.root.mkdir(exist_ok=True)
        if not self.repo.exists():
            run("git", "clone", "-q", "--filter=blob:none", f"https://github.com/{REPOSITORY}.git", str(self.repo))
        self.git("fetch", "-q", "origin")
        self.git("switch", "-q", "--detach", "origin/main")
        folder = self.repo / self.prefix
        folder.mkdir(exist_ok=True)
        for name in ("w", "x", "y", "z"):
            (folder / f"{name}.txt").write_text("".join(f"{name} line {n}\n" for n in range(1, 31)))
        self.git("add", "-A")
        self.git("commit", "-q", "-m", f"{self.prefix}: scratch main")
        self.commits["M0"] = self.git("rev-parse", "HEAD")
        self.push(self.commits["M0"], self.main)
        self.push(self.change("a1", "w.txt", 1, "w line 1 A-1"), self.a)
        self.change("b1", "x.txt", 3, "x line 3 B-1")
        self.change("b2", "y.txt", 3, "y line 3 B-2")
        self.push(self.change("b3", "z.txt", 3, "z line 3 B-3"), self.b)
        self.head = self.commits["b3"]
        url = run("gh", "pr", "create", "-R", REPOSITORY, "-B", self.a, "-H", self.b, "-t", f"BPR-1572 stacked live course ({self.prefix})",
                  "-b", "Live course for BPR-1572: a stacked pull request whose base branch takes its commits. Safe to close.").strip()
        self.pull = int(url.rsplit("/", 1)[1])

    def merge_into_a(self, label: str, commit: str) -> None:
        self.git("fetch", "-q", "origin", self.a)
        self.git("switch", "-q", "--detach", f"origin/{self.a}")
        self.git("merge", "-q", "--no-ff", "-m", f"{self.prefix}: {label}", commit)
        self.commits[label] = self.git("rev-parse", "HEAD")
        self.push(self.commits[label], self.a)

    def name(self, sha: str | None) -> str | None:
        if sha is None:
            return None
        return next((label for label, value in self.commits.items() if value == sha), sha[:12])

    def github_state(self) -> dict:
        """What GitHub says about the pull request now, by commit label."""
        pull = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}")
        head, base_sha = pull["head"]["sha"], pull["base"]["sha"]
        files = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}/files?per_page=100")
        commits = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}/commits?per_page=100")
        recorded = gh_json(f"repos/{REPOSITORY}/compare/{base_sha}...{head}?per_page=1&page=2")
        state = {
            "state": pull["state"], "merged": pull.get("merged"), "head": self.name(head), "base.sha": self.name(base_sha),
            "commits": pull["commits"], "changed_files": pull["changed_files"], "commit_list": [self.name(c["sha"]) for c in commits],
            "files": [[f["status"], f["filename"].split("/")[-1], f["additions"], f["deletions"]] for f in files],
            "web_diff_files": web_diff_files(self.pull),
            "compare_recorded": {"merge_base": self.name(recorded["merge_base_commit"]["sha"]), "ahead_by": recorded.get("ahead_by"), "status": recorded["status"]},
        }
        if pull["state"] == "open":
            branch = gh_json(f"repos/{REPOSITORY}/compare/{self.a}...{head}?per_page=1&page=2")
            state["compare_branch"] = {"tip": self.name(branch["base_commit"]["sha"]), "merge_base": self.name(branch["merge_base_commit"]["sha"]),
                                       "ahead_by": branch.get("ahead_by"), "status": branch["status"]}
        return state

    def wait_for_head(self) -> None:
        if not self.head:
            return
        for _ in range(60):
            if gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}")["head"]["sha"] == self.head:
                return
            time.sleep(1)
        raise RuntimeError(f"GitHub never reported head {self.head}")

    def timeline(self, seconds: float, into: list) -> None:
        start = time.time()
        last = None
        while time.time() - start < seconds:
            try:
                pull = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}")
                files = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}/files?per_page=100")
            except RuntimeError:
                time.sleep(1)
                continue
            key = [self.name(pull["base"]["sha"]), self.name(pull["head"]["sha"]), pull["state"], pull["commits"], sorted(f["filename"].split("/")[-1] for f in files)]
            if key != last:
                into.append([round(time.time() - start, 1), *key])
                last = key
            time.sleep(1)

    def observe(self, step: str, expect_mismatch: bool = False) -> None:
        if remaining() < 500:
            raise RuntimeError("X-RateLimit-Remaining below 500; stopping")
        self.wait_for_head()
        timeline: list = []
        recorder = threading.Thread(target=self.timeline, args=(self.arguments.timeline, timeline))
        recorder.start()
        log = self.work / f"course-{step}.log"
        course = subprocess.run([str(Path(self.arguments.build) / "tests/end_to_end/live_second_account_course"), "--course", "parity",
                                 "--root", str(self.root), "--pull", str(self.pull)],
                                capture_output=True, text=True, env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
        recorder.join()
        log.write_text(course.stdout + course.stderr)
        github = self.github_state()
        print(f"\n== {step}: PR #{self.pull} state={github['state']} head={github['head']} base.sha={github['base.sha']} "
              f"files={[f[1] for f in github['files']]} branch={github.get('compare_branch')} recorded={github['compare_recorded']}", flush=True)
        print(f"  timeline {timeline}")
        app = [line[:300] for line in course.stdout.splitlines() if line.startswith(("parity:comparing", "parity:files", "parity:waiting", "live-second-account-course"))]
        for line in app:
            print("  " + line)
        check = ["python3", str(HERE / "bpr1560_live_parity.py"), "--root", str(self.root), "--pull", str(self.pull)]
        if expect_mismatch:
            check.append("--expect-mismatch")
        verdict = subprocess.run(check, capture_output=True, text=True)
        print("\n".join("  " + line for line in (verdict.stdout + verdict.stderr).splitlines()))
        for name in ("parity.jsonl", "parity-interdiff.jsonl"):
            if (self.root / name).exists():
                (self.root / name).rename(self.work / f"{step}-{name}")
        if expect_mismatch:
            # The control must show why it differs: it compared and differed, or
            # it said Whole PR has no base. A crash or a timeout elsewhere is not
            # a reproduction.
            said = any(line.startswith("parity:waiting") and "where this pull request branched" in line for line in app)
            passed = verdict.returncode == 0 and (course.returncode == 0 or said)
        else:
            passed = verdict.returncode == 0 and course.returncode == 0
        self.results.append((step, passed))
        self.record({"step": step, "pull": self.pull, "github": github, "timeline": timeline, "app": app,
                     "parity": verdict.stdout.splitlines()[-1:] , "expect_mismatch": expect_mismatch, "passed": passed})

    def record(self, entry: dict) -> None:
        if self.arguments.record:
            with Path(self.arguments.record).open("a") as out:
                out.write(json.dumps({"prefix": self.prefix, "variant": self.arguments.variant, **entry}) + "\n")

    def left_wire(self, step: str) -> None:
        """The reads a LEFT publication makes on the stacked pull request, and
        GitHub's answer to a LEFT comment on a line only the recorded base's
        comparison has (x.txt line 3)."""
        pull = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}")
        head, base_sha = pull["head"]["sha"], pull["base"]["sha"]
        branch = gh_json(f"repos/{REPOSITORY}/compare/{self.a}...{head}?per_page=1&page=2")
        tip = branch["base_commit"]["sha"]
        recorded = gh_json(f"repos/{REPOSITORY}/compare/{base_sha}...{head}?per_page=1&page=2")
        ancestry = gh_json(f"repos/{REPOSITORY}/compare/{base_sha}...{tip}?per_page=1&page=2")
        again = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}")
        comment = json.loads(run("gh", "api", "-X", "POST", f"repos/{REPOSITORY}/pulls/{self.pull}/comments", "-f",
                                 "body=BPR-1572 live LEFT probe (deleted by the course)", "-f", f"commit_id={head}", "-f",
                                 f"path={self.prefix}/x.txt", "-F", "line=3", "-f", "side=LEFT"))
        run("gh", "api", "-X", "DELETE", f"repos/{REPOSITORY}/pulls/comments/{comment['id']}", check=False)
        entry = {
            "branch": {"tip": self.name(tip), "merge_base": self.name(branch["merge_base_commit"]["sha"]), "url": branch["url"].split("/compare/")[1]},
            "recorded": {"merge_base": self.name(recorded["merge_base_commit"]["sha"]), "ahead_by": recorded.get("ahead_by"), "commits": pull["commits"],
                         "url": recorded["url"].split("/compare/")[1]},
            "ancestry": {"merge_base": self.name(ancestry["merge_base_commit"]["sha"]), "base_commit": self.name(ancestry["base_commit"]["sha"]),
                         "url": ancestry["url"].split("/compare/")[1]},
            "pull_again": {"base.sha": self.name(again["base"]["sha"]), "head": self.name(again["head"]["sha"]), "commits": again["commits"]},
            "left_comment": {"side": comment.get("side"), "line": comment.get("line"), "path": comment.get("path"), "diff_hunk": comment.get("diff_hunk")},
        }
        # B forks at a1, the recorded base, so the recorded base's merge base
        # is `base.sha` itself: the comparison base the app named for this
        # state. On another fork point the expected merge base would be that
        # commit, not `base.sha`.
        rule = (entry["recorded"]["merge_base"] == self.name(base_sha) and entry["recorded"]["ahead_by"] == pull["commits"]
                and entry["ancestry"]["merge_base"] == self.name(base_sha) and entry["pull_again"]["base.sha"] == self.name(base_sha)
                and entry["pull_again"]["commits"] == pull["commits"])
        accepted = comment.get("side") == "LEFT" and comment.get("line") == 3
        print(f"\n== {step}-left-wire: {entry}\n  publication rule holds: {rule}; GitHub accepted the LEFT line: {accepted}", flush=True)
        self.results.append((f"{step}-left-wire", rule and accepted))
        self.record({"step": f"{step}-left-wire", "pull": self.pull, "left_wire": entry, "passed": rule and accepted})

    def course(self) -> None:
        self.setup()
        self.observe("1-open")
        control = self.arguments.negative_control
        if self.arguments.variant == "merge":
            self.merge_into_a("A merges b1", self.commits["b1"])
            self.observe("2-A-merges-b1", expect_mismatch=control)
            if self.arguments.left_wire:
                self.left_wire("2-A-merges-b1")
            if not control:
                self.merge_into_a("A merges all of B", self.commits["b3"])
                self.observe("3-A-merges-all")
        elif self.arguments.variant == "ff":
            self.push(self.commits["b1"], self.a)
            self.observe("2-A-ff-to-b1", expect_mismatch=control)
            self.push(self.commits["b2"], self.a)
            self.observe("3-A-ff-to-b2", expect_mismatch=control)
            if self.arguments.left_wire:
                self.left_wire("3-A-ff-to-b2")
            self.git("switch", "-q", "--detach", self.commits["b3"])
            self.push(self.change("b4", "z.txt", 20, "z line 20 B-4"), self.b)
            self.head = self.commits["b4"]
            self.observe("4-push-to-B", expect_mismatch=control)
        elif self.arguments.variant == "rebase":
            self.git("switch", "-q", "--detach", self.commits["M0"])
            self.push(self.change("M1", "w.txt", 25, "w line 25 MAIN-1"), self.main)
            self.git("cherry-pick", self.commits["a1"])
            self.commits["a1-rebased"] = self.git("rev-parse", "HEAD")
            self.git("push", "-q", "--force", "origin", f"{self.commits['a1-rebased']}:refs/heads/{self.a}")
            self.observe("2-A-rebased")
            self.git("cherry-pick", self.commits["b1"], self.commits["b2"], self.commits["b3"])
            self.commits["b3-rebased"] = self.git("rev-parse", "HEAD")
            self.head = self.commits["b3-rebased"]
            self.git("push", "-q", "--force", "origin", f"{self.commits['b3-rebased']}:refs/heads/{self.b}")
            self.observe("3-B-rebased")
        elif self.arguments.variant == "force":
            self.git("switch", "-q", "--detach", self.commits["a1"])
            self.push(self.change("a2", "w.txt", 10, "w line 10 A-2"), self.a)
            self.observe("2-A-advances")
            self.git("push", "-q", "--force", "origin", f"{self.commits['b1']}:refs/heads/{self.a}")
            self.observe("3-A-forced-to-b1")

    def cleanup(self) -> None:
        if self.pull:
            run("gh", "pr", "close", "-R", REPOSITORY, str(self.pull), check=False)
        for branch in (self.b, self.a, self.main):
            run("git", "push", "-q", "origin", "--delete", branch, cwd=self.repo, check=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--variant", choices=("merge", "ff", "rebase", "force"), default="merge")
    parser.add_argument("--left-wire", action="store_true")
    parser.add_argument("--record")
    parser.add_argument("--timeline", type=float, default=20.0)
    parser.add_argument("--negative-control", action="store_true")
    parser.add_argument("--cleanup", action="store_true")
    arguments = parser.parse_args()
    course = Course(arguments)
    try:
        course.course()
    finally:
        if arguments.cleanup:
            course.cleanup()
    print("\nsummary:", ", ".join(f"{step}={'ok' if ok else 'FAILED'}" for step, ok in course.results))
    return 0 if course.results and all(ok for _, ok in course.results) else 1


if __name__ == "__main__":
    sys.exit(main())
