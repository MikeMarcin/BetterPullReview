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
  Hide C2) keeps one row and reads, in chain order, each shown commit's own
  `git diff C^ C -- P`, blob for blob and line for line, with the hidden
  commit named between them (RFC-BPR-0089 M3, AC03);
- the removed and added lines equal `git diff` path by path;
- GitHub's Viewed state for every file is the same before and after (AC05).

With `--generated` ([[BPR-1692]], RFC-BPR-0089 M2) the scratch base also
commits `PREFIX/.gitattributes` marking `gen/** linguist-generated`, a nested
`PREFIX/gen/.gitattributes` returning `0007.txt` with `-linguist-generated`,
and a root `.gitattributes` line `PREFIX/gen/0001.txt linguist-generated=false`
that the deeper file outranks. The course runs `--course generated` and checks
against `git check-attr --source C3 linguist-generated`, the head's rules
GitHub's Files changed honours:

- Whole PR lists exactly the paths the head's rules leave normal, and the
  generated row holds exactly the others (at least 300), each because the
  attribute marks it; the Files drawer walks the same listed files;
- each commit filter, and Clear all filters, lists its shown set minus the
  generated paths, with the generated ones in its set behind the row;
- the heading says they are set aside and nothing says the rules were not read;
- GitHub's Viewed state is unchanged (AC05).

With `--force-push` (RFC-BPR-0089 M3, AC06) the course runs `--course
force-push` instead: it marks one file only C2 changed read, hides C3, and
waits while this script force-pushes C3 reworded on C2 with C3's own tree.
After the refresh it checks against Git that the chip names the gone C3,
offers exactly the reworded commit as the same change, applies nothing until
the course chooses it, keeps the identical-material receipt, and after the
choice hides the reworded commit in C3's place.

With `--comment` (RFC-BPR-0089 M4, AC07) the shape also carries
`src/notes.txt`, 30 lines shaped as BPR-Test #116: C1 edits its lines 5, 15
and 25, and C3 inserts two lines at its top and edits C1's lines 5 and 25
again. The course runs `--course filtered-comment`: with C3 hidden it selects
C1's lines 15, 5 and 10, and with only C3 shown the left side's lines 15, 10
and 13, writing where the composition's core places a comment on each and what
the editor says before Publish. This script checks each placement against Git
(a line unchanged at the head goes there at the head's line when it lies inside
a hunk of the merge base to the head, and is refused before Publish otherwise,
as GitHub refused BPR-Test #117's with 422; a line C3 changed goes on C1, with
the warning; a base line C1 changed is refused before Publish), then publishes exactly those placements through gh, because a
tests-enabled composition holds no publication grant, and checks what GitHub
answered: `commit_id` = head at the mapped line, and `commit_id` = C1 kept,
outdated, for the changed line. After the refresh the course writes the
markers: the C1 thread on C1's diff and not the head's, and on Whole PR the
head's threads and not C1's. `--cleanup` resolves every thread it made.

Run by hand on macOS. It stops when `X-RateLimit-Remaining` falls below 500,
and `--cleanup` closes its pull request and deletes its branches:

    python3 tests/end_to_end/rfc0089_commit_filter_live_course.py \\
        --build build/dev --work DIR --prefix rfc0089-live [--generated | --force-push | --comment] [--cleanup | --close N]
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
        self.posted: list[dict] = []

    def git(self, *arguments: str) -> str:
        return run("git", "-c", "user.name=Michael Marcin", "-c", "user.email=mike.marcin@gmail.com", *arguments, cwd=self.repo).strip()

    def write(self, relative: str, lines: list[str]) -> None:
        target = self.repo / self.prefix / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text("\n".join(lines) + "\n")

    def insert(self, relative: str, lines: list[str]) -> None:
        target = self.repo / self.prefix / relative
        target.write_text("\n".join(lines + target.read_text().splitlines()) + "\n")

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
        if self.arguments.comment:
            self.write("src/notes.txt", [f"notes line {n}" for n in range(1, 31)])
        for index in range(GENERATED):
            self.write(f"gen/{index:04}.txt", [f"// generated {index:04} v1"] + [f"gen {index:04} row {n}" for n in range(1, 21)])
        if self.arguments.generated:
            self.write(".gitattributes", ["gen/** linguist-generated"])
            self.write("gen/.gitattributes", ["0007.txt -linguist-generated"])
            root = self.repo / ".gitattributes"
            existing = root.read_text() if root.exists() else ""
            root.write_text(existing + ("" if not existing or existing.endswith("\n") else "\n") + f"{self.prefix}/gen/0001.txt linguist-generated=false\n")
        self.commit("M0", f"{self.prefix}: scratch base")
        self.git("push", "-q", "origin", f"{self.commits['M0']}:refs/heads/{self.base}")
        self.edit("src/a.txt", 5, "a line 5 C1")
        self.edit("src/b.txt", 5, "b line 5 C1")
        self.edit("src/version.txt", 1, "version 2 (C1)")
        self.write("src/new.txt", ["new line 1 C1", "new line 2 C1"])
        if self.arguments.comment:
            for line in (5, 15, 25):
                self.edit("src/notes.txt", line, f"notes line {line} C1")
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
        if self.arguments.comment:
            self.edit("src/notes.txt", 5, "notes line 5 C3")
            self.edit("src/notes.txt", 25, "notes line 25 C3")
            self.insert("src/notes.txt", ["inserted 1 C3", "inserted 2 C3"])
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
                        facts.append("a split path showed one diff")
                    else:
                        facts.extend(self.check_split(shown, path, diff))
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

    def check_split(self, shown: list[str], path: str, diff: dict) -> list[str]:
        """RFC-BPR-0089 M3 (owner, Q2): one exact diff per shown commit that
        changed the path, in chain order, the hidden ones named between."""
        chain = ["C1", "C2", "C3"]
        touches = [label for label in chain if path in self.touch[label]]
        shown_touches = [label for label in touches if label in shown]
        first, last = chain.index(shown_touches[0]), chain.index(shown_touches[-1])
        hidden = [label for label in touches if label not in shown and first < chain.index(label) < last]
        facts: list[str] = []
        segments = diff.get("segments", [])
        if [segment["commit"] for segment in segments] != [self.commits[label] for label in shown_touches]:
            facts.append(f"segments name {[segment['commit'][:12] for segment in segments]}, not {[self.commits[label][:12] for label in shown_touches]}")
            return facts
        chain_blobs = {self.blob(self.commits[label], path) for label in ("M0", *chain)} - {None}
        for label, segment in zip(shown_touches, segments):
            left = run("git", "rev-parse", f"{self.commits[label]}^", cwd=self.repo).strip()
            right = self.commits[label]
            if segment["left"] != left or segment["right"] != right:
                facts.append(f"{label}: endpoints {segment['left']}..{segment['right']} != {left[:12]}..{right[:12]}")
            if segment["left_blob"] != self.blob(left, path) or segment["right_blob"] != self.blob(right, path):
                facts.append(f"{label}: blobs {segment['left_blob']}..{segment['right_blob']} != {self.blob(left, path)}..{self.blob(right, path)}")
            for side in ("left_blob", "right_blob"):
                if segment[side] is not None and segment[side] not in chain_blobs:
                    facts.append(f"{label}: {side} {segment[side]} is in no commit of the chain")
            removed, added = patch_lines(run("git", "diff", "--no-color", "--no-ext-diff", "-U3", left, right, "--", path, cwd=self.repo))
            if segment["removed"] != removed or segment["added"] != added:
                facts.append(f"{label}: lines {segment['removed']} {segment['added']} != git {removed} {added}")
        names = diff.get("hidden", [])
        if len(names) != len(hidden) or any(not name.startswith(self.commits[label][:7]) or "also changed this file; hidden" not in name for label, name in zip(hidden, names)):
            facts.append(f"hidden named {names}, expected {[self.commits[label][:7] for label in hidden]}")
        print(f"      split into {len(segments)} per-commit diffs, {len(names)} hidden named")
        return facts

    def force_push(self, course: subprocess.Popen) -> str:
        """Waits at the course's `at-push`, force-pushes C3 reworded on C2 with
        C3's own tree, waits for GitHub to report it, and lets the course go."""
        at = self.root / "at-push"
        for _ in range(900):
            if at.exists() or course.poll() is not None:
                break
            time.sleep(1)
        if not at.exists():
            raise RuntimeError("the course never reached the push")
        headline = self.git("log", "-1", "--format=%s", self.commits["C3"])
        reworded = self.git("commit-tree", f"{self.commits['C3']}^{{tree}}", "-p", self.commits["C2"], "-m", f"{headline} (reworded)")
        self.commits["C3r"] = reworded
        self.git("push", "-q", "--force", "origin", f"{reworded}:refs/heads/{self.head_branch}")
        print(f"force-pushed C3r={reworded[:12]} over C3={self.commits['C3'][:12]}", flush=True)
        for _ in range(60):
            if json.loads(run("gh", "api", f"repos/{REPOSITORY}/pulls/{self.pull}"))["head"]["sha"] == reworded:
                break
            time.sleep(2)
        else:
            raise RuntimeError("GitHub never reported the force push")
        (self.root / "go-push").write_text("go\n")
        return reworded

    def check_force_push(self) -> None:
        lines = {line["kind"]: line for line in (json.loads(text) for text in (self.root / "force-push.jsonl").read_text().splitlines() if text.strip())}
        c3, c3r = self.commits["C3"], self.commits["C3r"]
        facts: list[str] = []
        before, pushed, rematched = lines.get("before"), lines.get("pushed"), lines.get("rematched")
        if before is None or before["hidden"] != c3:
            facts.append(f"before the push the course hid {before and before['hidden']}, not C3")
        if pushed is None:
            facts.append("nothing written after the push")
        else:
            listed = [row["commit"] for row in pushed["rows"]]
            if listed != [self.commits["C1"], self.commits["C2"], c3r]:
                facts.append(f"after the push the Commits tab lists {[sha[:12] for sha in listed]}")
            if pushed["gone"] != [c3[:7]] or f"Hidden commit {c3[:7]}" not in pushed["chip"] or "is no longer in the pull request" not in pushed["chip"]:
                facts.append(f"the chip does not name gone C3: {pushed['chip']!r}")
            if pushed["rematches"] != [{"gone": c3, "candidate": c3r, "basis": 0}]:
                facts.append(f"offers {pushed['rematches']}, not C3r as the same change")
            if f"{c3r[:7]} (" not in pushed["chip"] or f"makes the same change as {c3[:7]}." not in pushed["chip"] or pushed["button"] != f"Hide {c3r[:7]}":
                facts.append(f"the chip's offer reads {pushed['chip']!r} / {pushed['button']!r}")
            if pushed["rows"][2]["state"] != 0 or pushed["status"] != 0:
                facts.append("the offer applied itself: C3r is not shown before the choice")
            if not pushed["receipt_kept"]:
                facts.append("the identical-material receipt did not persist")
        if rematched is None:
            facts.append("nothing written after the choice")
        else:
            states = [row["state"] for row in rematched["rows"]]
            if [row["commit"] for row in rematched["rows"]][2] != c3r or states != [0, 0, 1] or not rematched["chip"].startswith("2 of 3 commits shown"):
                facts.append(f"after the choice rows {states}, chip {rematched['chip']!r}")
            if "no longer in the pull request" in rematched["chip"]:
                facts.append("after the choice the chip still names a gone commit")
        print(f"  {'MATCH' if not facts else 'DIFFERS'} force push: C3 {c3[:12]} -> C3r {c3r[:12]}")
        for fact in facts:
            print(f"      {fact}")
        self.failures.extend(f"force push: {fact}" for fact in facts)

    def text_at(self, commit: str, path: str) -> list[str]:
        return run("git", "show", f"{commit}:{path}", cwd=self.repo).splitlines()

    def unchanged(self, left: list[str], right: list[str]) -> dict[int, int]:
        """Each left line the diff leaves unchanged, mapped to its right line."""
        import difflib
        mapping: dict[int, int] = {}
        for block in difflib.SequenceMatcher(a=left, b=right, autojunk=False).get_matching_blocks():
            for offset in range(block.size):
                mapping[block.a + offset] = block.b + offset
        return mapping

    def changed_near(self, base: list[str], right: list[str], line: int, side: str = "RIGHT") -> bool:
        """Whether a line of one side lies within three lines of a change of
        base to right on that side: inside a hunk of GitHub's diff."""
        import difflib
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(a=base, b=right, autojunk=False).get_opcodes():
            low, high = (j1, j2) if side == "RIGHT" else (i1, i2)
            if tag != "equal" and low - 3 <= line < high + 3:
                return True
        return False

    def expected_placement(self, entry: dict) -> tuple[int, int, str | None, int]:
        """(placement, refusal, commit, target line) the core must decide: owner decision Q1."""
        path = f"{self.prefix}/src/notes.txt"
        head, fork = self.commits["C3"], self.commits["M0"]
        left, right, line = entry["left"], entry["right"], entry["line"]
        if entry["side"] == "RIGHT":
            if right == head:
                return 0, 0, head, line
            at_head = self.unchanged(self.text_at(right, path), self.text_at(head, path))
            if line in at_head:
                # GitHub resolves a line at the head only inside a hunk of the
                # merge base to the head (BPR-Test #117 refused one outside).
                if self.changed_near(self.text_at(fork, path), self.text_at(head, path), at_head[line]):
                    return 0, 0, head, at_head[line]
                return 2, 7, None, line
            if self.changed_near(self.text_at(fork, path), self.text_at(right, path), line):
                return 1, 0, right, line
            return 2, 5, None, line
        if left == fork:
            return 0, 0, head, line
        at_base = self.unchanged(self.text_at(left, path), self.text_at(fork, path))
        if line not in at_base:
            return 2, 6, None, line
        if self.changed_near(self.text_at(fork, path), self.text_at(head, path), at_base[line], "LEFT"):
            return 0, 0, head, at_base[line]
        return 2, 7, None, line

    def post(self, course: subprocess.Popen) -> None:
        """Waits at the course's `at-post`, checks each placement against Git,
        publishes exactly the placed ones through gh, checks GitHub's answer,
        and lets the course go."""
        at = self.root / "at-post"
        for _ in range(1800):
            if at.exists() or course.poll() is not None:
                break
            time.sleep(1)
        if not at.exists():
            raise RuntimeError("the course never reached the post step")
        path = f"{self.prefix}/src/notes.txt"
        head, c1 = self.commits["C3"], self.commits["C1"]
        placements = [json.loads(text) for text in (self.root / "filtered-comment.jsonl").read_text().splitlines() if text.strip()]
        placements = [entry for entry in placements if entry["kind"] == "placement"]
        if len(placements) != 6:
            self.failures.append(f"the course wrote {len(placements)} placements, not 6")
        self.posted = []
        for entry in placements:
            name = f"{entry['scenario']} {entry['side']} {entry['line'] + 1}"
            want = self.expected_placement(entry)
            got = (entry["placement"], entry["refusal"], entry["commit"], entry["target_line"])
            facts = []
            if got[:2] != want[:2] or (want[0] != 2 and got[2:] != want[2:]):
                facts.append(f"placed {got}, git says {want}")
            if want[0] == 1 and not (entry["notice"].startswith(f"Goes on commit {c1[:7]}") and "outdated" in entry["notice"] and not entry["blocks"]):
                facts.append(f"the warning before Publish reads {entry['notice']!r}")
            if want[0] == 2 and not entry["blocks"]:
                facts.append(f"a refused line does not block Publish: {entry['notice']!r}")
            if want[0] == 0 and want[3] != entry["line"] and f"line {want[3] + 1}" not in entry["notice"]:
                facts.append(f"the note before Publish reads {entry['notice']!r}")
            print(f"  {'MATCH' if not facts else 'DIFFERS'} placement {name}: {('head', 'earlier commit', 'refused')[got[0]]} line {got[3] + 1}; notice {entry['notice']!r}")
            self.failures.extend(f"placement {name}: {fact}" for fact in facts)
            if facts or want[0] == 2:
                continue
            body = f"RFC-BPR-0089 M4 live course ({self.prefix}): {name}."
            payload = json.dumps({"body": body, "commit_id": entry["commit"], "path": path, "line": entry["target_line"] + 1, "side": entry["target_side"]})
            answer = subprocess.run(["gh", "api", "-X", "POST", f"repos/{REPOSITORY}/pulls/{self.pull}/comments", "--input", "-"], input=payload, capture_output=True, text=True)
            if answer.returncode != 0:
                self.failures.append(f"GitHub refused {name}: {(answer.stdout + answer.stderr)[:300]}")
                continue
            comment = json.loads(answer.stdout)
            self.posted.append({"name": name, "placement": want[0], "id": comment["node_id"], "line": entry["target_line"]})
            facts = []
            if want[0] == 0 and (comment["commit_id"] != head or comment["line"] != entry["target_line"] + 1 or comment["side"] != entry["target_side"]):
                facts.append(f"GitHub answered commit {comment['commit_id'][:12]} line {comment['line']} side {comment['side']}")
            if want[0] == 1 and (comment["commit_id"] != c1 or comment["line"] is not None or comment["original_line"] != entry["target_line"] + 1 or comment["original_commit_id"] != c1):
                facts.append(f"GitHub answered commit {comment['commit_id'][:12]} line {comment['line']} original {comment['original_commit_id'][:12]}/{comment['original_line']}")
            print(f"  {'MATCH' if not facts else 'DIFFERS'} published {name}: commit_id {comment['commit_id'][:12]} line {comment['line']} "
                  f"original {comment['original_commit_id'][:12]}/{comment['original_line']} side {comment['side']}")
            self.failures.extend(f"published {name}: {fact}" for fact in facts)
        threads = self.threads()
        for posted in self.posted:
            outdated = next((thread["isOutdated"] for thread in threads if any(c["id"] == posted["id"] for c in thread["comments"]["nodes"])), None)
            if outdated is None or outdated != (posted["placement"] == 1):
                self.failures.append(f"{posted['name']}: GitHub's thread isOutdated={outdated}")
            else:
                print(f"  MATCH thread {posted['name']}: isOutdated={outdated}")
        (self.root / "go-post").write_text("go\n")

    def threads(self) -> list[dict]:
        query = """query($o:String!,$r:String!,$n:Int!){repository(owner:$o,name:$r){pullRequest(number:$n){reviewThreads(first:100){nodes{
          id isResolved isOutdated line originalLine diffSide comments(first:5){nodes{id}}}}}}}"""
        answer = json.loads(run("gh", "api", "graphql", "-F", f"o={OWNER}", "-F", f"r={NAME}", "-F", f"n={self.pull}", "-f", f"query={query}"))
        return answer["data"]["repository"]["pullRequest"]["reviewThreads"]["nodes"]

    def check_comment(self) -> None:
        lines = [json.loads(text) for text in (self.root / "filtered-comment.jsonl").read_text().splitlines() if text.strip()]
        marks = {line["scenario"]: line for line in lines if line["kind"] == "marks"}
        on_head = sorted(posted["line"] for posted in self.posted if posted["placement"] == 0 and "RIGHT" in posted["name"])
        on_c1 = sorted(posted["line"] for posted in self.posted if posted["placement"] == 1)
        hidden, whole = marks.get("hide-last"), marks.get("whole")
        facts = []
        if hidden is None or whole is None:
            facts.append(f"markers not written: {sorted(marks)}")
        else:
            if not set(on_c1) <= set(hidden["right"]) or set(on_head) & set(hidden["right"]):
                facts.append(f"C1's diff marks right {hidden['right']}: want C1's {on_c1}, none of the head's {on_head}")
            if not set(on_head) <= set(whole["right"]) or set(on_c1) & set(whole["right"]):
                facts.append(f"Whole PR marks right {whole['right']}: want the head's {on_head}, none of C1's {on_c1}")
            based = sorted(posted["line"] for posted in self.posted if "LEFT" in posted["name"])
            if not set(based) <= set(whole["left"]):
                facts.append(f"Whole PR marks left {whole['left']}: want {based}")
        print(f"  {'MATCH' if not facts else 'DIFFERS'} markers: C1's diff {hidden and hidden['right']}, Whole PR right {whole and whole['right']} left {whole and whole['left']}")
        self.failures.extend(f"markers: {fact}" for fact in facts)

    def resolve_threads(self) -> None:
        mutation = "mutation($id:ID!){resolveReviewThread(input:{threadId:$id}){thread{isResolved}}}"
        for thread in self.threads():
            if not thread["isResolved"]:
                run("gh", "api", "graphql", "-f", f"id={thread['id']}", "-f", f"query={mutation}", check=False)

    def generated_states(self, paths: list[str]) -> dict[str, str]:
        answered = subprocess.run(["git", "check-attr", "--source", self.commits["C3"], "-z", "linguist-generated", "--", *paths],
                                  cwd=self.repo, capture_output=True, check=True).stdout.decode().split("\0")
        return dict(zip(answered[0::3], answered[2::3]))

    def check_generated(self) -> None:
        lines = [json.loads(line) for line in (self.root / "generated.jsonl").read_text().splitlines() if line.strip()]
        every = sorted({path for label in ("C1", "C2", "C3") for path in self.touch[label]})
        states = self.generated_states(every)
        generated = {path for path, state in states.items() if state not in ("unspecified", "unset", "false")}
        print(f"git check-attr at C3: {len(generated)} of {len(every)} generated; "
              f"{self.prefix}/gen/0007.txt {states.get(f'{self.prefix}/gen/0007.txt')}, {self.prefix}/gen/0001.txt {states.get(f'{self.prefix}/gen/0001.txt')}")
        if len(generated) < 300:
            self.failures.append(f"the course needs at least 300 generated files, git marks {len(generated)}")
        names = {"whole": ["C1", "C2", "C3"], "cleared": ["C1", "C2", "C3"], "only-1": ["C1"], "only-2": ["C2"], "only-3": ["C3"], "hide-2": ["C1", "C3"]}
        seen = set()
        for view in (line for line in lines if line["kind"] == "view"):
            name = view["name"]
            seen.add(name)
            shown = names[name]
            listed = {path for path in every if self.expected(shown, path)["kind"] != "excluded"} if name not in ("whole", "cleared") else set(every)
            want_files = sorted(listed - generated)
            want_aside = sorted(listed & generated)
            files, aside = sorted(view["files"]), sorted(view["generated"])
            facts = []
            if view["attributes"] != 1:
                facts.append(f"attributes state {view['attributes']}, not read")
            if files != want_files:
                facts.append(f"lists {len(files)} != {len(want_files)}: extra {sorted(set(files) - set(want_files))[:5]} missing {sorted(set(want_files) - set(files))[:5]}")
            if aside != want_aside:
                facts.append(f"sets aside {len(aside)} != {len(want_aside)}: extra {sorted(set(aside) - set(want_aside))[:5]} missing {sorted(set(want_aside) - set(aside))[:5]}")
            if any(reason != 1 for reason in view["generated"].values()):
                facts.append("a set-aside file's reason is not the attribute")
            if "linguist-generated" in view["heading"] or (want_aside and "generated set aside" not in view["heading"]):
                facts.append(f"heading {view['heading']!r}")
            print(f"  {'MATCH' if not facts else 'DIFFERS'} {name}: lists {len(files)}, sets aside {len(aside)}; row {view['row']!r}")
            self.failures.extend(f"{name}: {fact}" for fact in facts)
        walk = next((line for line in lines if line["kind"] == "walk"), None)
        if walk is None or sorted(walk["paths"]) != sorted(set(every) - generated):
            self.failures.append("the Files drawer does not walk exactly the listed files")
        else:
            print(f"  MATCH walk: {len(walk['paths'])} files")
        missing = sorted(set(names) - seen)
        if missing:
            self.failures.append(f"views not written: {missing}")

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
        chosen = "generated" if self.arguments.generated else "force-push" if self.arguments.force_push else "filtered-comment" if self.arguments.comment else "commit-filter"
        command = [str(Path(self.arguments.build).resolve() / "tests/end_to_end/live_second_account_course"), "--course", chosen,
                   "--root", str(self.root), "--pull", str(self.pull)]
        if self.arguments.force_push:
            # A file only C2 changes: its Whole PR material is the same before and after the push.
            command += ["--path", f"{prefix}/gen/0010.txt"]
        if self.arguments.comment:
            command += ["--path", f"{prefix}/src/notes.txt"]
        with log.open("w") as written:
            process = subprocess.Popen(command, stdout=written, stderr=subprocess.STDOUT, text=True, env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
            if self.arguments.force_push or self.arguments.comment:
                try:
                    self.force_push(process) if self.arguments.force_push else self.post(process)
                except Exception:
                    process.kill()
                    raise
            returncode = process.wait()
        output = log.read_text()
        print(f"course: exit {returncode} in {time.time() - started:.0f}s (log {log})", flush=True)
        for line in output.splitlines():
            if line.startswith(("commit-filter:", "generated:", "force-push:", "filtered-comment:", "live-second-account-course", "login:", "checkpoint:")):
                print("  " + line[:300])
        if returncode != 0:
            self.failures.append("the course failed")
        after = self.viewed()
        if after != before:
            changed = sorted(path for path in set(before) | set(after) if before.get(path) != after.get(path))
            self.failures.append(f"Viewed state changed for {changed[:10]}")
        print(f"AC05: Viewed state {'unchanged' if after == before else 'CHANGED'} for {len(after)} files", flush=True)
        if self.arguments.force_push:
            if (self.root / "force-push.jsonl").exists():
                self.check_force_push()
            else:
                self.failures.append("the course wrote no force-push.jsonl")
        elif self.arguments.comment:
            self.check_comment()
        elif self.arguments.generated:
            if (self.root / "generated.jsonl").exists():
                self.check_generated()
            else:
                self.failures.append("the course wrote no generated.jsonl")
        elif (self.root / "commit-filter.jsonl").exists():
            self.check()
        guard()

    def cleanup(self) -> None:
        if self.pull:
            self.resolve_threads()
            run("gh", "pr", "close", "-R", REPOSITORY, str(self.pull), check=False)
        for branch in (self.head_branch, self.base):
            run("git", "push", "-q", "origin", "--delete", branch, cwd=self.repo, check=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--cleanup", action="store_true")
    parser.add_argument("--generated", action="store_true", help="BPR-1692: commit linguist-generated rules in the base and run the generated course")
    parser.add_argument("--force-push", action="store_true", help="RFC-BPR-0089 M3: hide C3, force-push it reworded, and check the rematch offer")
    parser.add_argument("--comment", action="store_true", help="RFC-BPR-0089 M4: place comments on filtered diffs, publish them and check GitHub and the markers")
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
