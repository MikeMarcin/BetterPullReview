#!/usr/bin/env python3
"""Run RC59-10's live publication course on github.com/MikeMarcin/BPR-Test.

[[BPR-1176]]. The owner designated MikeMarcin/BPR-Test as the disposable
target on 2026-09-30. This is the only repository this driver touches, and it
is a constant, not an argument. It is not a test row: a person runs it, with
the `gh` sign-in of an account that can push to the repository.

The driver:

1. clones the repository into a fresh owner-private directory;
2. pushes a base branch with three files, and a second branch at the same
   commit that stands for a branch another client made;
3. runs `tool_review_cycle_live_publication` (built with
   `-DBPR_BUILD_LIVE_PUBLICATION_COURSE=ON`), handing it the credential on
   standard input, never on the command line or in the environment;
4. checks what the tool reports against GitHub through a second client,
   `git fetch` and `gh api`: the raw commit keeps both non-zero offsets, the
   deleted file is gone from the tree, and each pull request's head, base,
   draft flag and author are what the plan named.

Every branch and pull request it makes is left in place as evidence and is
listed at the end. Nothing on GitHub is deleted, closed or force-updated; the
local clone is removed when every check passes.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

if sys.version_info < (3, 10):
    sys.exit(f"Python 3.10 or newer is required; found {sys.version.split()[0]} at {sys.executable}")

OWNER = "MikeMarcin"
REPOSITORY = "BPR-Test"
REMOTE = f"https://github.com/{OWNER}/{REPOSITORY}.git"


def run(arguments: list[str], cwd: pathlib.Path | None = None, stdin: bytes | None = None) -> str:
    completed = subprocess.run(arguments, cwd=cwd, input=stdin, capture_output=True, check=False)
    if completed.returncode != 0:
        raise SystemExit(
            f"{arguments[0]} {' '.join(arguments[1:3])} failed ({completed.returncode}):\n"
            + completed.stderr.decode(errors="replace")
        )
    return completed.stdout.decode()


def git(checkout: pathlib.Path, *arguments: str) -> str:
    return run(
        [
            "git",
            "-c", "user.name=BPR live course",
            "-c", "user.email=bpr-live-course@example.invalid",
            "-c", "commit.gpgsign=false",
            "-c", "core.autocrlf=false",
            "-c", "core.hooksPath=" + str(checkout / "no-hooks-here"),
            *arguments,
        ],
        cwd=checkout,
    )


def gh_api(path: str) -> dict:
    return json.loads(run(["gh", "api", f"repos/{OWNER}/{REPOSITORY}/{path}"]))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tool", required=True, type=pathlib.Path, help="tool_review_cycle_live_publication")
    parser.add_argument("--git", default="git", help="the Git the product's publication catalog runs")
    options = parser.parse_args()

    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    prefix = f"bpr-rc59-10/{stamp}"
    viewer = json.loads(run(["gh", "api", "user"]))
    login, account = viewer["login"], viewer["id"]
    git_executable = run(["which", options.git]).strip() if os.sep not in options.git else options.git

    work = pathlib.Path(tempfile.mkdtemp(prefix="bpr-rc59-10-")).resolve()
    checkout = work / REPOSITORY
    run(["git", "clone", "--quiet", REMOTE, str(checkout)])
    base = f"{prefix}/base"
    foreign = f"{prefix}/foreign"
    git(checkout, "checkout", "--quiet", "-b", base, "origin/main")
    (checkout / "src").mkdir()
    (checkout / "README.md").write_text("# RC59-10 live course fixture\n", newline="\n")
    (checkout / "src" / "parse.cpp").write_text("int parse() { return 0; }\n", newline="\n")
    (checkout / "src" / "old.cpp").write_text("int old() { return 0; }\n", newline="\n")
    git(checkout, "add", "README.md", "src")
    git(checkout, "commit", "--quiet", "-m", f"RC59-10 live course base {stamp}")
    parent = git(checkout, "rev-parse", "HEAD").strip()
    git(checkout, "push", "--quiet", "origin", f"HEAD:refs/heads/{base}", f"HEAD:refs/heads/{foreign}")
    print(f"DRIVER base {base} at {parent}; foreign branch {foreign} pushed by git")

    token = run(["gh", "auth", "token"]).strip().encode()
    completed = subprocess.run(
        [
            str(options.tool),
            "--git", git_executable,
            "--checkout", str(checkout),
            "--base", base,
            "--stamp", stamp,
            "--login", login,
            "--account", str(account),
            "--foreign", foreign,
        ],
        input=token + b"\n",
        capture_output=True,
        check=False,
    )
    token = b""
    output = completed.stdout.decode(errors="replace")
    sys.stdout.write(output)
    sys.stderr.write(completed.stderr.decode(errors="replace"))
    if completed.returncode != 0:
        print(f"DRIVER the tool failed ({completed.returncode}); branches under {prefix}/ and the clone {checkout} are left for inspection")
        return 1

    # A second client's view of the ready publication.
    ready = re.search(r"^LIVE ready: branch (\S+) commit ([0-9a-f]{40}) tree ([0-9a-f]{40}) .* pull #(\d+) ", output, re.MULTILINE)
    draft = re.search(r"^LIVE draft: .* pull #(\d+) ", output, re.MULTILINE)
    if ready is None or draft is None:
        print("DRIVER the tool's report is missing the ready or draft publication")
        return 1
    branch, commit, tree, number = ready.group(1), ready.group(2), ready.group(3), int(ready.group(4))
    git(checkout, "fetch", "--quiet", "origin", f"refs/heads/{branch}")
    fetched = git(checkout, "rev-parse", "FETCH_HEAD").strip()
    raw = git(checkout, "cat-file", "-p", fetched)
    files = git(checkout, "ls-tree", "-r", "--name-only", fetched).split()
    author = next(line for line in raw.splitlines() if line.startswith("author "))
    committer = next(line for line in raw.splitlines() if line.startswith("committer "))
    checks = {
        "the branch head fetched by Git is the tool's commit": fetched == commit,
        "the fetched commit's tree is the tool's tree": git(checkout, "rev-parse", f"{fetched}^{{tree}}").strip() == tree,
        "one parent, the pushed base": git(checkout, "rev-parse", f"{fetched}^@").split() == [parent],
        "the author keeps +0530": author.endswith(" 1790000000 +0530"),
        "the committer keeps -0700": committer.endswith(" 1790000060 -0700"),
        "src/old.cpp is deleted": "src/old.cpp" not in files,
        "src/validate.cpp is added": "src/validate.cpp" in files,
    }
    pull = gh_api(f"pulls/{number}")
    checks["the ready pull request is not a draft"] = pull["draft"] is False
    checks["its head is the commit"] = pull["head"]["sha"] == commit and pull["head"]["ref"] == branch
    checks["its base is the base branch at the parent"] = pull["base"]["ref"] == base and pull["base"]["sha"] == parent
    checks["its author is the signed-in login"] = pull["user"]["login"] == login
    drafted = gh_api(f"pulls/{draft.group(1)}")
    checks["the draft pull request is a draft"] = drafted["draft"] is True
    for name, passed in checks.items():
        print(f"DRIVER {'pass' if passed else 'FAIL'}: {name}")
    print(f"DRIVER author line: {author}")
    print(f"DRIVER committer line: {committer}")

    listed = json.loads(run(["gh", "api", f"repos/{OWNER}/{REPOSITORY}/pulls?state=all&per_page=100"]))
    print("DRIVER pull requests left as evidence:")
    for item in listed:
        if item["head"]["ref"].startswith(prefix + "/"):
            print(f"  #{item['number']} {item['head']['ref']} draft={item['draft']} {item['html_url']}")
    refs = json.loads(run(["gh", "api", f"repos/{OWNER}/{REPOSITORY}/git/matching-refs/heads/{prefix}/"]))
    print("DRIVER branches left as evidence:")
    for ref in refs:
        print(f"  {ref['ref']} {ref['object']['sha']}")
    if all(checks.values()):
        shutil.rmtree(work)
        return 0
    print(f"DRIVER local clone kept for inspection: {checkout}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
