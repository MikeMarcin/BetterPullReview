#!/usr/bin/env python3
"""[[BPR-1572]] Criss-cross merge bases on MikeMarcin/BPR-Test.

Git can name several merge bases for a base and a head; GitHub's compare names
one (`merge_base_commit`). This builds a pull request whose recorded base and
head have two, X1 and Y1, whose comparisons with the head differ:

    M0 --- X1 ------ Mm      base branch: X1 changes c.txt line 5, Mm merges Y1
      \\        \\  /
       \\         \\/
        \\        /\\
         Y1 ------ Fm --- F2  head: Y1 changes c.txt line 25, Fm merges X1,
                                  F2 changes d.txt line 3

From X1 the head differs in c.txt (line 25) and d.txt; from Y1 in c.txt (line
5) and d.txt; from a merge of both (Git's recursive virtual base) in d.txt
only. It records what GitHub's Files changed lists, which merge base compare
names, and which candidate's comparison is GitHub's, then runs the shipped
composition's parity course (`--build`) and `bpr1560_live_parity.py`, which
checks the app shows exactly GitHub's file set and lines with the left side
read at the merge base compare names.

    python3 tests/end_to_end/bpr1572_criss_cross_probe.py --build build/dev \\
        --work DIR --prefix bpr1572-cc [--record FILE] [--cleanup]
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bpr1572_stacked_live_course import REPOSITORY, gh_json, remaining, run, web_diff_files  # noqa: E402

HERE = Path(__file__).resolve().parent


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--record")
    parser.add_argument("--cleanup", action="store_true")
    arguments = parser.parse_args()
    if remaining() < 500:
        raise SystemExit("X-RateLimit-Remaining below 500; stopping")
    work = Path(arguments.work).resolve()
    repo, root = work / "repo", work / "app-root"
    work.mkdir(parents=True, exist_ok=True)
    root.mkdir(exist_ok=True)
    p = arguments.prefix
    main_branch, feature = f"{p}-main", f"{p}-feature"
    commits: dict[str, str] = {}

    def git(*command: str) -> str:
        return run("git", "-c", "user.name=Michael Marcin", "-c", "user.email=mike.marcin@gmail.com", *command, cwd=repo).strip()

    def change(label: str, name: str, line: int, text: str) -> str:
        target = repo / p / name
        lines = target.read_text().splitlines()
        lines[line - 1] = text
        target.write_text("\n".join(lines) + "\n")
        git("add", "-A")
        git("commit", "-q", "-m", f"{p}: {label}")
        commits[label] = git("rev-parse", "HEAD")
        return commits[label]

    def merge(label: str, into: str, other: str) -> str:
        git("switch", "-q", "--detach", into)
        git("merge", "-q", "--no-ff", "-m", f"{p}: {label}", other)
        commits[label] = git("rev-parse", "HEAD")
        return commits[label]

    pull = 0
    try:
        if not repo.exists():
            run("git", "clone", "-q", "--filter=blob:none", f"https://github.com/{REPOSITORY}.git", str(repo))
        git("fetch", "-q", "origin")
        git("switch", "-q", "--detach", "origin/main")
        (repo / p).mkdir(exist_ok=True)
        for name in ("c", "d"):
            (repo / p / f"{name}.txt").write_text("".join(f"{name} line {n}\n" for n in range(1, 31)))
        git("add", "-A")
        git("commit", "-q", "-m", f"{p}: fork point")
        commits["M0"] = git("rev-parse", "HEAD")
        x1 = change("X1", "c.txt", 5, "c line 5 X")
        git("switch", "-q", "--detach", commits["M0"])
        y1 = change("Y1", "c.txt", 25, "c line 25 Y")
        mm = merge("Mm", x1, y1)
        fm = merge("Fm", y1, x1)
        git("switch", "-q", "--detach", fm)
        f2 = change("F2", "d.txt", 3, "d line 3 F")
        candidates = sorted(git("merge-base", "--all", mm, f2).split())
        git("push", "-q", "origin", f"{mm}:refs/heads/{main_branch}", f"{f2}:refs/heads/{feature}")
        url = run("gh", "pr", "create", "-R", REPOSITORY, "-B", main_branch, "-H", feature, "-t", f"BPR-1572 criss-cross probe ({p})",
                  "-b", "Live probe for BPR-1572: a criss-cross merge base. Safe to close.").strip()
        pull = int(url.rsplit("/", 1)[1])
        names = {value: label for label, value in commits.items()}
        for _ in range(30):
            if gh_json(f"repos/{REPOSITORY}/pulls/{pull}")["head"]["sha"] == f2:
                break
            time.sleep(1)
        time.sleep(5)
        record = gh_json(f"repos/{REPOSITORY}/pulls/{pull}")
        files = gh_json(f"repos/{REPOSITORY}/pulls/{pull}/files?per_page=100")
        recorded = gh_json(f"repos/{REPOSITORY}/compare/{record['base']['sha']}...{f2}?per_page=1&page=2")
        branch = gh_json(f"repos/{REPOSITORY}/compare/{main_branch}...{f2}?per_page=1&page=2")
        github_files = sorted([f["filename"].split("/")[-1], f["additions"], f["deletions"]] for f in files)
        # The patches decide: from X1 and from Y1 the file set and counts are
        # alike, the changed lines are not.
        github_patches = {f["filename"].split("/")[-1]: f.get("patch", "") for f in files}
        local_patches = {}
        local = {}
        for label, base in [*((names.get(c, c[:10]), c) for c in candidates), ("virtual (merge-tree of both)", None)]:
            if base is None:
                tree = git("merge-tree", "--write-tree", *candidates).split()[0] if len(candidates) == 2 else None
                if tree is None:
                    continue
                numstat = git("diff", "--numstat", tree, f2)
            else:
                numstat = git("diff", "--numstat", base, f2)
            local[label] = sorted([line.split("\t")[2].split("/")[-1], int(line.split("\t")[0]), int(line.split("\t")[1])] for line in numstat.splitlines())
            patches = {}
            for name in ("c.txt", "d.txt"):
                text = git("diff", "--unified=3", tree if base is None else base, f2, "--", f"{p}/{name}")
                patches[name] = text[text.index("@@"):].rstrip("\n") if "@@" in text else ""
            local_patches[label] = {name: patch for name, patch in patches.items() if patch}
        named = names.get(recorded["merge_base_commit"]["sha"], recorded["merge_base_commit"]["sha"][:10])
        matches = [label for label, patches in local_patches.items() if patches == {name: patch for name, patch in github_patches.items() if patch}]
        print(f"merge bases by git: {[names.get(c, c[:10]) for c in candidates]}")
        print(f"base.sha={names.get(record['base']['sha'])} compare(base.sha...head) names {named}; compare(branch...head) names "
              f"{names.get(branch['merge_base_commit']['sha'], branch['merge_base_commit']['sha'][:10])}")
        print(f"GitHub Files changed: {github_files}; web .diff: {web_diff_files(pull)}")
        for label, numstat in local.items():
            print(f"  from {label}: {numstat}{'  <- GitHub (patches equal)' if label in matches else ''}")
        course = subprocess.run([str(Path(arguments.build) / "tests/end_to_end/live_second_account_course"), "--course", "parity",
                                 "--root", str(root), "--pull", str(pull)], capture_output=True, text=True,
                                env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
        (work / "course.log").write_text(course.stdout + course.stderr)
        app = [line[:300] for line in course.stdout.splitlines() if line.startswith(("parity:comparing", "parity:files", "parity:waiting", "live-second-account-course"))]
        verdict = subprocess.run(["python3", str(HERE / "bpr1560_live_parity.py"), "--root", str(root), "--pull", str(pull)], capture_output=True, text=True)
        print("\n".join("  " + line for line in [*app, *verdict.stdout.splitlines()]))
        passed = course.returncode == 0 and verdict.returncode == 0
        if arguments.record:
            with Path(arguments.record).open("a") as out:
                out.write(json.dumps({"prefix": p, "pull": pull, "git_merge_bases": [names.get(c, c[:10]) for c in candidates], "compare_recorded": named,
                                      "compare_branch": names.get(branch["merge_base_commit"]["sha"]), "github_files": github_files,
                                      "web_diff_files": web_diff_files(pull), "local": local, "github_matches": matches, "app": app,
                                      "parity": verdict.stdout.splitlines()[-1:], "passed": passed}) + "\n")
        print("summary:", "ok" if passed else "FAILED")
        return 0 if passed else 1
    finally:
        if arguments.cleanup:
            if pull:
                run("gh", "pr", "close", "-R", REPOSITORY, str(pull), check=False)
            for branch_name in (feature, main_branch):
                run("git", "push", "-q", "origin", "--delete", branch_name, cwd=repo, check=False)


if __name__ == "__main__":
    sys.exit(main())
