#!/usr/bin/env python3
"""RFC-BPR-0089 M1 live course on MikeMarcin/BPR-Test: commit filters on a
pull request of the owner's shape, through the shipped composition.

Builds a scratch base branch holding a few source files and 320 generated
ones, and a pull request of three commits on it:

    C1  source: edits src/a.txt, src/b.txt and src/version.txt, adds src/new.txt
    C2  regenerates every gen/*.txt (at least 300 files) and src/version.txt
    C3  mixes both: edits src/b.txt, src/c.txt, src/version.txt and gen/0000
        to gen/0004

It marks two files Viewed through GraphQL, then runs
`live_second_account_course --course commit-filter` offscreen, signed in
through the active `gh` account (the sparse review path, which acquires the
chain's trees through gh). The course chooses Show only selected on each
commit and Hide selected on C2 from the Commits tab's own context menu, and
writes what the review showed. This script checks it against Git:

- the Commits tab lists C1 to C3 with headline, author and the file count of
  `git diff-tree C^ C` (AC01);
- each filter's file set is exactly the paths the shown commits changed
  (AC01, AC02), and Clear all filters restored Whole PR (the course requires
  it);
- each diff shown is, path by path, the one the core's rule names: a path every
  commit that changed it is shown for keeps Whole PR's own comparison (no
  filter endpoints, so its material and receipt are Whole PR's; AC02); a path
  a hidden commit changed outside the shown span is `git diff L R -- P` with
  L the parent of the first shown commit that changed it and R the last; a
  path a hidden commit changed between two shown ones (src/version.txt under
  Hide C2) reads nothing in M1 and says why;
- the removed and added lines equal `git diff` path by path;
- GitHub's Viewed state for every file is the same before and after (AC05).

Run by hand on macOS. It stops when `X-RateLimit-Remaining` falls below 500,
and `--cleanup` closes its pull request and deletes its branches:

    python3 tests/end_to_end/rfc0089_commit_filter_live_course.py \\
        --build build/dev --work DIR --prefix rfc0089-live [--cleanup | --close N]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

REPOSITORY = "MikeMarcin/BPR-Test"
OWNER, NAME = REPOSITORY.split("/")
GENERATED = 320
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")


def run(*command: str, cwd: Path | None = None, check: bool = True, env: dict | None = None) -> str:
    for attempt in range(5):
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, env=env)
        transient = any(word in result.stderr for word in ("i/o timeout", "connection reset", "502", "504"))
        if result.returncode == 0 or not transient or command[:2] == ("gh", "pr"):
            break
        time.sleep(5 + 5 * attempt)
    if check and result.returncode != 0:
        raise RuntimeError(f"{' '.join(command[:4])} failed: {result.stderr.strip()[:400]}")
    return result.stdout


def remaining() -> int:
    for line in run("gh", "api", "-i", f"/repos/{REPOSITORY}").splitlines():
        if line.lower().startswith("x-ratelimit-remaining:"):
            return int(line.split(":", 1)[1])
    return 0


def guard(margin: int = 500) -> int:
    left = remaining()
    print(f"rate: X-RateLimit-Remaining={left}", flush=True)
    if left < margin:
        raise RuntimeError(f"X-RateLimit-Remaining {left} is below {margin}; stopping")
    return left


def patch_lines(patch: str) -> tuple[list[list[object]], list[list[object]]]:
    removed: list[list[object]] = []
    added: list[list[object]] = []
    old = new = 0
    inside = False
    for line in patch.split("\n"):
        header = HUNK.match(line)
        if header:
            old, new, inside = int(header.group(1)), int(header.group(3)), True
            continue
        if not inside or line.startswith("\\"):
            continue
        if line.startswith("-"):
            removed.append([old, line[1:]])
            old += 1
        elif line.startswith("+"):
            added.append([new, line[1:]])
            new += 1
        elif line.startswith(" "):
            old += 1
            new += 1
    return removed, added


VIEWED = """query($owner: String!, $name: String!, $number: Int!, $endCursor: String) {
  repository(owner: $owner, name: $name) { pullRequest(number: $number) {
    files(first: 100, after: $endCursor) { nodes { path viewerViewedState } pageInfo { hasNextPage endCursor } } } } }"""


class Course:
    def __init__(self, arguments: argparse.Namespace) -> None:
        self.arguments = arguments
        self.work = Path(arguments.work).resolve()
        self.repo = self.work / "repo"
        self.root = self.work / "app-root"
        self.prefix = arguments.prefix
        self.base, self.head_branch = f"{self.prefix}-base", f"{self.prefix}-head"
        self.pull = 0
        self.commits: dict[str, str] = {}
        self.touch: dict[str, list[str]] = {}
        self.failures: list[str] = []

    def git(self, *arguments: str) -> str:
        return run("git", "-c", "user.name=Michael Marcin", "-c", "user.email=mike.marcin@gmail.com", *arguments, cwd=self.repo).strip()

    def write(self, relative: str, lines: list[str]) -> None:
        target = self.repo / self.prefix / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text("\n".join(lines) + "\n")

    def edit(self, relative: str, line: int, text: str) -> None:
        target = self.repo / self.prefix / relative
        lines = target.read_text().splitlines()
        lines[line - 1] = text
        target.write_text("\n".join(lines) + "\n")

    def commit(self, label: str, headline: str) -> str:
        self.git("add", "-A")
        self.git("commit", "-q", "-m", headline)
        self.commits[label] = self.git("rev-parse", "HEAD")
        return self.commits[label]

    def setup(self) -> None:
        self.work.mkdir(parents=True, exist_ok=True)
        self.root.mkdir(exist_ok=True)
        if not self.repo.exists():
            run("git", "clone", "-q", "--filter=blob:none", f"https://github.com/{REPOSITORY}.git", str(self.repo))
        self.git("fetch", "-q", "origin")
        self.git("switch", "-q", "--detach", "origin/main")
        for name in ("a", "b", "c", "version"):
            self.write(f"src/{name}.txt", [f"{name} line {n}" for n in range(1, 31)])
        for index in range(GENERATED):
            self.write(f"gen/{index:04}.txt", [f"// generated {index:04} v1"] + [f"gen {index:04} row {n}" for n in range(1, 21)])
        self.commit("M0", f"{self.prefix}: scratch base")
        self.git("push", "-q", "origin", f"{self.commits['M0']}:refs/heads/{self.base}")
        self.edit("src/a.txt", 5, "a line 5 C1")
        self.edit("src/b.txt", 5, "b line 5 C1")
        self.edit("src/version.txt", 1, "version 2 (C1)")
        self.write("src/new.txt", ["new line 1 C1", "new line 2 C1"])
        self.commit("C1", f"{self.prefix}: C1 source change")
        for index in range(GENERATED):
            self.edit(f"gen/{index:04}.txt", 1, f"// generated {index:04} v2")
            self.edit(f"gen/{index:04}.txt", 10, f"gen {index:04} row 9 regenerated")
        self.edit("src/version.txt", 10, "version line 10 stamped (C2)")
        self.commit("C2", f"{self.prefix}: C2 regenerate {GENERATED} files")
        self.edit("src/b.txt", 20, "b line 20 C3")
        self.edit("src/c.txt", 3, "c line 3 C3")
        self.edit("src/version.txt", 25, "version line 25 C3")
        for index in range(5):
            self.edit(f"gen/{index:04}.txt", 15, f"gen {index:04} row 14 C3")
        self.commit("C3", f"{self.prefix}: C3 source and generated")
        self.git("push", "-q", "origin", f"{self.commits['C3']}:refs/heads/{self.head_branch}")
        url = run("gh", "pr", "create", "-R", REPOSITORY, "-B", self.base, "-H", self.head_branch, "-t", f"RFC-BPR-0089 commit filter live course ({self.prefix})",
                  "-b", "Live course for RFC-BPR-0089 M1: commit filters on C1 source, C2 regenerated, C3 mixed. Safe to close.").strip()
        self.pull = int(url.rsplit("/", 1)[1])
        print(f"pull request #{self.pull}: {', '.join(f'{label}={sha[:12]}' for label, sha in self.commits.items())}", flush=True)
        for _ in range(60):
            pull = json.loads(run("gh", "api", f"repos/{REPOSITORY}/pulls/{self.pull}"))
            if pull["head"]["sha"] == self.commits["C3"] and pull.get("changed_files", 0) > GENERATED:
                return
            time.sleep(2)
        raise RuntimeError("GitHub never reported the pull request's files")

    def viewed(self) -> dict[str, str]:
        output = run("gh", "api", "graphql", "--paginate", "--slurp", "-F", f"owner={OWNER}", "-F", f"name={NAME}", "-F", f"number={self.pull}", "-f", f"query={VIEWED}")
        states: dict[str, str] = {}
        for page in json.loads(output):
            for node in page["data"]["repository"]["pullRequest"]["files"]["nodes"]:
                states[node["path"]] = node["viewerViewedState"]
        return states

    def mark_viewed(self, paths: list[str]) -> None:
        node = json.loads(run("gh", "api", f"repos/{REPOSITORY}/pulls/{self.pull}"))["node_id"]
        for path in paths:
            run("gh", "api", "graphql", "-f", "query=mutation($id: ID!, $path: String!) { markFileAsViewed(input: {pullRequestId: $id, path: $path}) { clientMutationId } }",
                "-f", f"id={node}", "-f", f"path={path}")

    def touched(self, label: str) -> list[str]:
        sha = self.commits[label]
        return sorted(run("git", "diff-tree", "-r", "--no-commit-id", "--name-only", "--find-renames=100%", f"{sha}^", sha, cwd=self.repo).split())

    def blob(self, commit: str, path: str) -> str | None:
        line = run("git", "ls-tree", commit, "--", path, cwd=self.repo).strip()
        return line.split()[2] if line else None

    def expected(self, shown: list[str], path: str) -> dict:
        chain = ["C1", "C2", "C3"]
        touches = [label for label in chain if path in self.touch[label]]
        shown_touches = [label for label in touches if label in shown]
        if not shown_touches:
            return {"kind": "excluded"}
        if all(label in shown for label in touches):
            return {"kind": "unchanged", "left": self.commits["M0"], "right": self.commits["C3"]}
        first, last = shown_touches[0], shown_touches[-1]
        if any(label not in shown and chain.index(first) < chain.index(label) < chain.index(last) for label in touches):
            return {"kind": "split"}
        return {"kind": "exact", "left": self.commits[first] + "^", "right": self.commits[last]}

    def check(self) -> None:
        lines = [json.loads(line) for line in (self.root / "commit-filter.jsonl").read_text().splitlines() if line.strip()]
        commits = next(line for line in lines if line["kind"] == "commits")
        listed = [row["commit"] for row in commits["rows"]]
        if listed != [self.commits[label] for label in ("C1", "C2", "C3")]:
            self.failures.append(f"Commits tab lists {listed}")
        for row, label in zip(commits["rows"], ("C1", "C2", "C3")):
            headline = self.git("log", "-1", "--format=%s", self.commits[label])
            facts = []
            if row["headline"] != headline:
                facts.append(f"headline {row['headline']!r} != {headline!r}")
            if row["files"] != len(self.touch[label]):
                facts.append(f"files {row['files']} != git {len(self.touch[label])}")
            if not row["author"]:
                facts.append("no author")
            print(f"  {'MATCH' if not facts else 'DIFFERS'} row {label} {row['commit'][:12]} files={row['files']} author={row['author']} {row['headline']!r}")
            self.failures.extend(f"row {label}: {fact}" for fact in facts)
        names = {"only-1": ["C1"], "only-2": ["C2"], "only-3": ["C3"], "hide-2": ["C1", "C3"]}
        seen = set()
        for scenario in (line for line in lines if line["kind"] == "scenario"):
            name = scenario["name"]
            seen.add(name)
            shown = names[name]
            paths = sorted({path for label in shown for path in self.touch[label]})
            expected_paths = [path for path in paths if self.expected(shown, path)["kind"] != "excluded"]
            files = sorted(scenario["files"])
            print(f"== {name}: shown {shown} files={len(files)} chip={scenario['chip']!r}")
            if files != expected_paths:
                self.failures.append(f"{name}: file set {len(files)} != git {len(expected_paths)}: extra {sorted(set(files) - set(expected_paths))[:5]} missing {sorted(set(expected_paths) - set(files))[:5]}")
            for diff in scenario["diffs"]:
                path = diff["path"]
                want = self.expected(shown, path)
                facts = []
                if want["kind"] == "split":
                    if not diff.get("split"):
                        facts.append("a split path showed a diff")
                elif diff.get("split"):
                    facts.append(f"said split, expected {want['kind']}")
                else:
                    left, right = run("git", "rev-parse", want["left"], cwd=self.repo).strip(), want["right"]
                    if want["kind"] == "unchanged" and (diff["left"] is not None or diff["right"] is not None):
                        facts.append(f"filter endpoints {diff['left']}..{diff['right']} on a path Whole PR shows as is")
                    if want["kind"] == "exact" and (diff["left"] != left or diff["right"] != right):
                        facts.append(f"endpoints {diff['left']}..{diff['right']} != {left[:12]}..{right[:12]}")
                    if diff["left_blob"] != self.blob(left, path):
                        facts.append(f"left blob {diff['left_blob']} != {self.blob(left, path)}")
                    if diff["right_blob"] != self.blob(right, path):
                        facts.append(f"right blob {diff['right_blob']} != {self.blob(right, path)}")
                    removed, added = patch_lines(run("git", "diff", "--no-color", "--no-ext-diff", "-U3", left, right, "--", path, cwd=self.repo))
                    if diff["removed"] != removed:
                        facts.append(f"removed {diff['removed']} != git {removed}")
                    if diff["added"] != added:
                        facts.append(f"added {diff['added']} != git {added}")
                print(f"  {'MATCH' if not facts else 'DIFFERS'} {want['kind']} {path}")
                for fact in facts:
                    print(f"      {fact}")
                self.failures.extend(f"{name} {path}: {fact}" for fact in facts)
        missing = sorted(set(names) - seen)
        if missing:
            self.failures.append(f"scenarios not run: {missing}")

    def course(self) -> None:
        guard()
        self.setup()
        self.touch = {label: self.touched(label) for label in ("C1", "C2", "C3")}
        prefix = self.prefix
        self.mark_viewed([f"{prefix}/src/a.txt", f"{prefix}/gen/0001.txt"])
        before = self.viewed()
        print(f"viewed before: {sorted(path for path, state in before.items() if state == 'VIEWED')} of {len(before)}", flush=True)
        guard()
        log = self.work / "course.log"
        started = time.time()
        course = subprocess.run([str(Path(self.arguments.build).resolve() / "tests/end_to_end/live_second_account_course"), "--course", "commit-filter",
                                 "--root", str(self.root), "--pull", str(self.pull)],
                                capture_output=True, text=True, env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
        log.write_text(course.stdout + course.stderr)
        print(f"course: exit {course.returncode} in {time.time() - started:.0f}s (log {log})", flush=True)
        for line in course.stdout.splitlines():
            if line.startswith(("commit-filter:", "live-second-account-course", "login:")):
                print("  " + line[:300])
        if course.returncode != 0:
            self.failures.append("the course failed")
        after = self.viewed()
        if after != before:
            changed = sorted(path for path in set(before) | set(after) if before.get(path) != after.get(path))
            self.failures.append(f"Viewed state changed for {changed[:10]}")
        print(f"AC05: Viewed state {'unchanged' if after == before else 'CHANGED'} for {len(after)} files", flush=True)
        if (self.root / "commit-filter.jsonl").exists():
            self.check()
        guard()

    def cleanup(self) -> None:
        if self.pull:
            run("gh", "pr", "close", "-R", REPOSITORY, str(self.pull), check=False)
        for branch in (self.head_branch, self.base):
            run("git", "push", "-q", "origin", "--delete", branch, cwd=self.repo, check=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--cleanup", action="store_true")
    parser.add_argument("--close", type=int, help="only close this earlier run's pull request and delete the prefix's branches")
    arguments = parser.parse_args()
    course = Course(arguments)
    if arguments.close:
        course.pull = arguments.close
        course.cleanup()
        return 0
    try:
        course.course()
    finally:
        if arguments.cleanup:
            course.cleanup()
    print("\ncommit-filter live:", "PASS" if not course.failures else f"FAIL ({len(course.failures)})")
    for failure in course.failures:
        print("  " + failure)
    return 0 if not course.failures else 1


if __name__ == "__main__":
    sys.exit(main())
