#!/usr/bin/env python3
"""[[BPR-1560]] Live merge-base parity course on MikeMarcin/BPR-Test.

Builds one pull request through the history the owner's pull request had:
several pushes, merges from the base branch while it advances, then the branch
squashed into one commit and force-pushed (on the original fork point, rebased
onto the base branch's tip, or onto an older base commit). After every step it
records what GitHub says (`base.sha`, the compare merge base against
`base.sha` and against the base branch tip), runs the shipped composition
offscreen through `live_second_account_course --course parity` on one
persistent application data root (so the install has seen every earlier
update), and checks the application's Whole PR and its interdiff from the
previous update with `bpr1560_live_parity.py`.

Run by hand on macOS. It pushes branches and opens a pull request on the
owner's designated test repository only, signs in as the active `gh` account,
and stops when `X-RateLimit-Remaining` falls below 500:

    python3 tests/end_to_end/bpr1560_live_course.py --build build/dev \\
        --work DIR --prefix bpr1560-sq [--negative-control] [--cleanup]

`--negative-control` expects the parity checks to fail (the build before the
fix). Exit status 0 means every step matched as expected.
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


def run(*command: str, cwd: Path | None = None, check: bool = True, env: dict | None = None) -> str:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, env=env)
    if check and result.returncode != 0:
        raise RuntimeError(f"{' '.join(command)} failed: {result.stderr.strip()}")
    return result.stdout


def gh_json(path: str) -> object:
    return json.loads(run("gh", "api", path))


def remaining() -> int:
    headers = run("gh", "api", "-i", "/rate_limit")
    for line in headers.splitlines():
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
        self.base = f"{self.prefix}-main"
        self.feature = f"{self.prefix}-feature"
        self.pull = 0
        self.previous_head: str | None = None
        self.results: list[tuple[str, bool]] = []

    def git(self, *arguments: str) -> str:
        return run("git", "-c", "user.name=Michael Marcin", "-c", "user.email=mike.marcin@gmail.com", *arguments, cwd=self.repo).strip()

    def edit(self, path: str, old: str, new: str) -> None:
        target = self.repo / path
        lines = target.read_text().splitlines()
        lines = [new if line == old else line for line in lines]
        target.write_text("\n".join(lines) + "\n")

    def commit(self, message: str) -> str:
        self.git("add", "-A")
        self.git("commit", "-q", "-m", message)
        return self.git("rev-parse", "HEAD")

    def push(self, branch: str, force: bool = False) -> None:
        self.git("push", "-q", *(["--force"] if force else []), "origin", f"HEAD:refs/heads/{branch}")

    def setup(self) -> None:
        self.work.mkdir(parents=True, exist_ok=True)
        self.root.mkdir(exist_ok=True)
        if not self.repo.exists():
            run("git", "clone", "-q", "--filter=blob:none", f"https://github.com/{REPOSITORY}.git", str(self.repo))
        self.git("fetch", "-q", "origin")
        self.git("switch", "-q", "--detach", "origin/main")
        folder = self.repo / self.prefix
        folder.mkdir(exist_ok=True)
        for name in ("a", "b", "c", "shared"):
            (folder / f"{name}.txt").write_text("".join(f"{name} line {n}\n" for n in range(1, 61)))
        (folder / "doomed.txt").write_text("removed by the pull request\n")
        self.commit(f"{self.prefix}: base files")
        self.push(self.base)

    def wait_for_head(self, head: str) -> dict:
        for _ in range(40):
            pull = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}")
            if pull["head"]["sha"] == head:
                time.sleep(3)
                return gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}")
            time.sleep(2)
        raise RuntimeError(f"GitHub never reported head {head}")

    def observe(self, step: str, head: str) -> None:
        if remaining() < 500:
            raise RuntimeError("X-RateLimit-Remaining below 500; stopping")
        pull = self.wait_for_head(head)
        base_sha = pull["base"]["sha"]
        tip = self.git("ls-remote", "origin", f"refs/heads/{self.base}").split()[0]
        against_recorded = gh_json(f"repos/{REPOSITORY}/compare/{base_sha}...{head}?page=2&per_page=1")["merge_base_commit"]["sha"]
        against_tip = gh_json(f"repos/{REPOSITORY}/compare/{tip}...{head}?page=2&per_page=1")["merge_base_commit"]["sha"]
        files = gh_json(f"repos/{REPOSITORY}/pulls/{self.pull}/files?per_page=100")
        print(f"\n== {step}: PR #{self.pull} head={head[:12]} base.sha={base_sha[:12]} tip={tip[:12]} "
              f"merge-base(base.sha)={against_recorded[:12]} merge-base(tip)={against_tip[:12]} "
              f"base.sha==merge-base:{base_sha == against_recorded} files={[(f['status'], f['filename'], f.get('previous_filename')) for f in files]}",
              flush=True)
        log = self.work / f"course-{step}.log"
        environment = {**os.environ, "QT_QPA_PLATFORM": "offscreen"}
        course = subprocess.run([str(Path(self.arguments.build) / "tests/end_to_end/live_second_account_course"), "--course", "parity",
                                 "--root", str(self.root), "--pull", str(self.pull)], capture_output=True, text=True, env=environment)
        log.write_text(course.stdout + course.stderr)
        for line in course.stdout.splitlines():
            if line.startswith(("parity:comparing", "parity:files", "parity:previous", "parity:interdiff", "live-second-account-course")):
                print("  " + line[:300])
        interdiff = self.root / "parity-interdiff.jsonl"
        if interdiff.exists() and course.returncode == 0 and "parity:interdiff shown=1" not in course.stdout:
            interdiff.unlink()
        check = ["python3", str(HERE / "bpr1560_live_parity.py"), "--root", str(self.root), "--pull", str(self.pull)]
        if self.previous_head:
            check += ["--interdiff-from", self.previous_head]
        if self.arguments.negative_control:
            check.append("--expect-mismatch")
        verdict = subprocess.run(check, capture_output=True, text=True)
        print("\n".join("  " + line for line in verdict.stdout.splitlines()))
        for name in ("parity.jsonl", "parity-interdiff.jsonl"):
            if (self.root / name).exists():
                (self.root / name).rename(self.work / f"{step}-{name}")
        self.results.append((step, course.returncode == 0 and verdict.returncode == 0))
        self.previous_head = head

    def course(self) -> None:
        p = self.prefix
        self.setup()
        fork = self.git("rev-parse", "HEAD")
        # Iteration 1: modify, delete, rename and add.
        self.edit(f"{p}/a.txt", "a line 3", "a line 3 PR-1")
        self.git("rm", "-q", f"{p}/doomed.txt")
        self.git("mv", f"{p}/c.txt", f"{p}/c-renamed.txt")
        (self.repo / p / "added.txt").write_text("added by the pull request\n")
        head = self.commit("PR iteration 1")
        self.push(self.feature)
        url = run("gh", "pr", "create", "-R", REPOSITORY, "-B", self.base, "-H", self.feature, "-t", f"BPR-1560 squash course ({p})",
                  "-b", "Live course for BPR-1560: merges from the base, then squashed and force-pushed. Safe to close.").strip()
        self.pull = int(url.rsplit("/", 1)[1])
        self.observe("1-open", head)

        # The base advances, touching a file the pull request touches; merged in.
        self.git("fetch", "-q", "origin", self.base)
        self.git("switch", "-q", "--detach", f"origin/{self.base}")
        self.edit(f"{p}/shared.txt", "shared line 10", "shared line 10 MAIN-1")
        self.edit(f"{p}/a.txt", "a line 40", "a line 40 MAIN-1")
        main_one = self.commit("base: change 1")
        self.push(self.base)
        self.git("switch", "-q", "--detach", head)
        self.git("merge", "-q", "--no-edit", main_one)
        self.edit(f"{p}/b.txt", "b line 5", "b line 5 PR-2")
        head = self.commit("PR iteration 2 after merging the base")
        self.push(self.feature)
        self.observe("2-merged", head)
        unsquashed = head

        # The base advances again (not merged): one change also lands the
        # pull request's own b.txt change separately.
        self.git("switch", "-q", "--detach", main_one)
        self.edit(f"{p}/a.txt", "a line 55", "a line 55 MAIN-2")
        self.edit(f"{p}/b.txt", "b line 5", "b line 5 PR-2")
        main_two = self.commit("base: change 2, lands the PR's b.txt change")
        self.push(self.base)

        # Squash on the original fork point.
        tree = self.git("rev-parse", f"{unsquashed}^{{tree}}")
        head = self.git("commit-tree", tree, "-p", fork, "-m", "PR squashed on the fork point")
        self.git("switch", "-q", "--detach", head)
        self.push(self.feature, force=True)
        self.observe("3-squash-fork-point", head)

        # Squash rebased onto the base branch tip.
        self.git("switch", "-q", "--detach", main_two)
        self.git("merge", "-q", "--squash", unsquashed)
        head = self.commit("PR squashed onto the base tip")
        self.push(self.feature, force=True)
        self.observe("4-squash-on-tip", head)

        # Squash onto the intermediate base commit.
        self.git("switch", "-q", "--detach", main_one)
        self.git("merge", "-q", "--squash", unsquashed)
        head = self.commit("PR squashed onto an older base commit")
        self.push(self.feature, force=True)
        self.observe("5-squash-on-older-base", head)

    def cleanup(self) -> None:
        if self.pull:
            run("gh", "pr", "close", "-R", REPOSITORY, str(self.pull), "--delete-branch", check=False)
        self.git("push", "-q", "origin", "--delete", self.base)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--prefix", required=True)
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
