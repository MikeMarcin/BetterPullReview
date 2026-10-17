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

After each step it runs the shipped composition offscreen through
`live_second_account_course --course parity` on one persistent data root and
checks Whole PR against GitHub with `bpr1560_live_parity.py`.
`--negative-control` expects step 2 to differ (a build that refuses or uses
the branch's merge base). Run by hand on macOS; it stops when
`X-RateLimit-Remaining` falls below 500:

    python3 tests/end_to_end/bpr1572_stacked_live_course.py --build build/dev \\
        --work DIR --prefix bpr1572-live [--cleanup]
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
        url = run("gh", "pr", "create", "-R", REPOSITORY, "-B", self.a, "-H", self.b, "-t", f"BPR-1572 stacked live course ({self.prefix})",
                  "-b", "Live course for BPR-1572: a stacked pull request whose base branch takes its commits. Safe to close.").strip()
        self.pull = int(url.rsplit("/", 1)[1])

    def merge_into_a(self, label: str, commit: str) -> None:
        self.git("fetch", "-q", "origin", self.a)
        self.git("switch", "-q", "--detach", f"origin/{self.a}")
        self.git("merge", "-q", "--no-ff", "-m", f"{self.prefix}: {label}", commit)
        self.commits[label] = self.git("rev-parse", "HEAD")
        self.push(self.commits[label], self.a)

    def observe(self, step: str, expect_mismatch: bool = False) -> None:
        if remaining() < 500:
            raise RuntimeError("X-RateLimit-Remaining below 500; stopping")
        time.sleep(15)  # GitHub settles a base branch push within seconds (measured 1-6 s).
        pull = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}")
        head, base_sha = pull["head"]["sha"], pull["base"]["sha"]
        names = {sha: label for label, sha in self.commits.items()}
        files = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}/files?per_page=100")
        print(f"\n== {step}: PR #{self.pull} state={pull['state']} merged={pull.get('merged')} head={names.get(head, head[:10])} "
              f"base.sha={names.get(base_sha, base_sha[:10])} files={[f['filename'].split('/')[-1] for f in files]}", flush=True)
        log = self.work / f"course-{step}.log"
        course = subprocess.run([str(Path(self.arguments.build) / "tests/end_to_end/live_second_account_course"), "--course", "parity",
                                 "--root", str(self.root), "--pull", str(self.pull)],
                                capture_output=True, text=True, env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
        log.write_text(course.stdout + course.stderr)
        for line in course.stdout.splitlines():
            if line.startswith(("parity:comparing", "parity:files", "live-second-account-course")):
                print("  " + line[:300])
        check = ["python3", str(HERE / "bpr1560_live_parity.py"), "--root", str(self.root), "--pull", str(self.pull)]
        if expect_mismatch:
            check.append("--expect-mismatch")
        verdict = subprocess.run(check, capture_output=True, text=True)
        print("\n".join("  " + line for line in (verdict.stdout + verdict.stderr).splitlines()))
        for name in ("parity.jsonl", "parity-interdiff.jsonl"):
            if (self.root / name).exists():
                (self.root / name).rename(self.work / f"{step}-{name}")
        passed = verdict.returncode == 0 and (course.returncode == 0 or expect_mismatch)
        self.results.append((step, passed))

    def course(self) -> None:
        self.setup()
        self.observe("1-open")
        control = self.arguments.negative_control
        if self.arguments.variant == "merge":
            self.merge_into_a("A merges b1", self.commits["b1"])
            self.observe("2-A-merges-b1", expect_mismatch=control)
            if not control:
                self.merge_into_a("A merges all of B", self.commits["b3"])
                self.observe("3-A-merges-all")
        elif self.arguments.variant == "ff":
            self.push(self.commits["b1"], self.a)
            self.observe("2-A-ff-to-b1", expect_mismatch=control)
            self.push(self.commits["b2"], self.a)
            self.observe("3-A-ff-to-b2", expect_mismatch=control)
            self.git("switch", "-q", "--detach", self.commits["b3"])
            self.push(self.change("b4", "z.txt", 20, "z line 20 B-4"), self.b)
            self.observe("4-push-to-B", expect_mismatch=control)
        else:
            self.git("switch", "-q", "--detach", self.commits["M0"])
            self.push(self.change("M1", "w.txt", 25, "w line 25 MAIN-1"), self.main)
            self.git("cherry-pick", self.commits["a1"])
            self.commits["a1-rebased"] = self.git("rev-parse", "HEAD")
            self.git("push", "-q", "--force", "origin", f"{self.commits['a1-rebased']}:refs/heads/{self.a}")
            self.observe("2-A-rebased")
            self.git("cherry-pick", self.commits["b1"], self.commits["b2"], self.commits["b3"])
            self.commits["b3-rebased"] = self.git("rev-parse", "HEAD")
            self.git("push", "-q", "--force", "origin", f"{self.commits['b3-rebased']}:refs/heads/{self.b}")
            self.observe("3-B-rebased")

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
    parser.add_argument("--variant", choices=("merge", "ff", "rebase"), default="merge")
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
