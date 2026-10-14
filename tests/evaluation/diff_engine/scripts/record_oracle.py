#!/usr/bin/env python3
"""Record GitHub's diff of the evaluation corpus ([[RFC-BPR-0082]] AC03).

Pushes every default-option corpus pair to MikeMarcin/BPR-Test, the owner's
designated live test repository. One base branch holds every left side. GitHub
stops producing patches once a pull request's diff passes its size budget, so
the right sides go to several head branches, each a small group of files, and
each group gets its own pull request. For every group the script reads the
REST file list and the raw `.diff` of the pull request, and writes
tests/fixtures/github/diff-oracle-v1/oracle.json with each file's blob
identities and GitHub's hunk headers (from the file list's patch, or from the
raw diff where the file list omits it). No patch text is kept: only
`@@ -a,b +c,d @@` numbers, so no corpus content is committed. Each pull request
is closed after it is read; the branches stay so the recording can be checked
again.

Rate limit: a handful of REST calls. The X-RateLimit-Remaining header of every
call is printed, and the script stops when fewer than 500 remain. It never
runs `gh auth switch`; it uses the active account.

Usage (prints the plan; --execute performs it):
  record_oracle.py --corpus BUILD/diff-evaluation-corpus --repo ROOT \
      --git /absolute/git --stage BUILD/_bpr/diff-evaluation [--execute]
"""

from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[4]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

REPOSITORY = "MikeMarcin/BPR-Test"
BASE_BRANCH = "bpr-diff-eval-v1-base"
HEAD_PREFIX = "bpr-diff-eval-v1-group-"
GROUP_LINES = 6_000
GROUP_FILES = 25
MINIMUM_REMAINING = 500
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", re.M)


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(command, check=True, capture_output=True, text=True, **kwargs)


def gh_get(path: str, accept: str | None = None) -> tuple[str | None, int]:
    """One REST GET through gh: the body (None on an HTTP error) and
    X-RateLimit-Remaining."""
    command = ["gh", "api", "-i", path]
    if accept:
        command[2:2] = ["-H", f"Accept: {accept}"]
    completed = subprocess.run(command, capture_output=True, text=True)
    text = completed.stdout
    # The headers end at the first blank line; a raw diff's body may itself
    # hold CRLF blank lines.
    ends = [index for index in (text.find("\r\n\r\n"), text.find("\n\n")) if index >= 0]
    cut = min(ends) if ends else len(text)
    head = text[:cut]
    body = text[cut + (4 if text.startswith("\r\n\r\n", cut) else 2):]
    remaining = -1
    status = head.splitlines()[0] if head else ""
    for line in head.splitlines():
        if line.lower().startswith("x-ratelimit-remaining:"):
            remaining = int(line.split(":", 1)[1].strip())
    print(f"GET {path} {status.strip()} X-RateLimit-Remaining {remaining}")
    if 0 <= remaining < MINIMUM_REMAINING:
        raise SystemExit("rate limit margin reached; stopping")
    if completed.returncode != 0:
        return None, remaining
    return body, remaining


def merge_like_file_list(raw: list[list[int]]) -> list[list[int]]:
    """GitHub's REST file list joins hunks that one unchanged line separates
    (as `git diff --inter-hunk-context=1` does); its raw `.diff` does not.
    The recording checked this on every file that had both."""
    result: list[list[int]] = []
    for hunk in raw:
        if result:
            previous = result[-1]
            if hunk[0] - (previous[0] + previous[1]) <= 1 and hunk[2] - (previous[2] + previous[3]) <= 1:
                result[-1] = [previous[0], hunk[0] + hunk[1] - previous[0], previous[2], hunk[2] + hunk[3] - previous[2]]
                continue
        result.append(list(hunk))
    return result


DIFF_FILE = re.compile(r"^diff --git a/(.+?) b/(.+)$", re.M)


def raw_diff_hunks(text: str) -> dict[str, list[list[int]]]:
    """Each file's hunk headers in a raw unified diff."""
    result: dict[str, list[list[int]]] = {}
    matches = list(DIFF_FILE.finditer(text))
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        result[match.group(2)] = hunks(text[match.end():end]) or []
    return result


def hunks(patch: str | None) -> list[list[int]] | None:
    if patch is None:
        return None
    result = []
    for match in HUNK.finditer(patch):
        a, b, c, d = match.groups()
        result.append([int(a), int(b) if b is not None else 1, int(c), int(d) if d is not None else 1])
    return result


def compact_json(oracle: dict) -> str:
    """One line per file, so a file with thousands of hunks stays one line."""
    head = {key: value for key, value in oracle.items() if key != "files"}
    lines = ["{"] + [f" {json.dumps(key)}: {json.dumps(head[key], sort_keys=True)}," for key in sorted(head)] + [' "files": [']
    for index, entry in enumerate(oracle["files"]):
        lines.append("  " + json.dumps(entry, sort_keys=True, separators=(",", ":")) + ("," if index + 1 < len(oracle["files"]) else ""))
    return "\n".join(lines + [" ]", "}"]) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--corpus", type=pathlib.Path, required=True)
    parser.add_argument("--repo", type=pathlib.Path, required=True)
    parser.add_argument("--git", required=True)
    parser.add_argument("--stage", type=pathlib.Path, required=True)
    parser.add_argument("--execute", action="store_true")
    arguments = parser.parse_args()
    manifest = json.loads((arguments.corpus / "manifest.json").read_text())
    pairs = [pair for pair in manifest["pairs"] if pair["options"] == "default" and not pair["id"].startswith("uiforetw74-")]
    files = {}
    for pair in pairs:
        name = pathlib.PurePosixPath(pair["path"]).name or (pair["id"] + ".txt")
        files[f"corpus/{pair['id']}/{name}"] = pair

    git = arguments.git
    environment = {"GIT_CONFIG_NOSYSTEM": "1", "HOME": str(pathlib.Path.home()), "PATH": "/usr/bin:/bin:/opt/homebrew/bin",
                   "GIT_AUTHOR_NAME": "BPR diff evaluation", "GIT_AUTHOR_EMAIL": "noreply@example.invalid",
                   "GIT_COMMITTER_NAME": "BPR diff evaluation", "GIT_COMMITTER_EMAIL": "noreply@example.invalid",
                   "GIT_AUTHOR_DATE": "2026-10-13T00:00:00Z", "GIT_COMMITTER_DATE": "2026-10-13T00:00:00Z"}
    # Changed lines per file, to keep each pull request inside GitHub's budget.
    changed = {}
    for path, pair in files.items():
        numstat = subprocess.run([git, "diff", "--no-index", "--numstat", str(arguments.corpus / pair["left"]), str(arguments.corpus / pair["right"])],
                                 capture_output=True, text=True, env=environment).stdout.split()
        changed[path] = int(numstat[0]) + int(numstat[1]) if len(numstat) >= 2 and numstat[0].isdigit() else 0
    groups: list[list[str]] = []
    current: list[str] = []
    total = 0
    for path in sorted(files):
        if current and (total + changed[path] > GROUP_LINES or len(current) >= GROUP_FILES):
            groups.append(current)
            current, total = [], 0
        current.append(path)
        total += changed[path]
    if current:
        groups.append(current)
    print(f"{len(files)} files in {len(groups)} pull requests on {REPOSITORY}")
    if not arguments.execute:
        for index, group in enumerate(groups):
            print(index, len(group), sum(changed[path] for path in group))
        return 0

    entries = []
    sources = []
    with contained_temporary_directory(prefix="bpr-diff-oracle-") as temporary:
        work = pathlib.Path(temporary) / "work"
        run([git, "init", "--quiet", "--initial-branch", BASE_BRANCH, str(work)], env=environment)
        run([git, "-C", str(work), "config", "core.autocrlf", "false"], env=environment)
        readme = ("Diff-engine evaluation corpus for BetterPullReview RFC-BPR-0082. The base branch holds each "
                  "comparison's left side, the head branch its right side. Files under corpus/git-* come from "
                  "Git 2.39.5 and 2.56.0 and are GPL-2.0 (see COPYING); the rest are generated.\n")
        (work / "README.md").write_text(readme)
        (work / "COPYING").write_bytes((arguments.stage / "git-2.56.0/git-2.56.0/COPYING").read_bytes())
        for path, pair in files.items():
            target = work / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((arguments.corpus / pair["left"]).read_bytes())
        run([git, "-C", str(work), "add", "--all"], env=environment)
        run([git, "-C", str(work), "commit", "--quiet", "-m", "Diff evaluation corpus: left sides"], env=environment)
        base_commit = run([git, "-C", str(work), "rev-parse", BASE_BRANCH], env=environment).stdout.strip()
        branches = []
        for index, group in enumerate(groups):
            branch = f"{HEAD_PREFIX}{index:02d}"
            run([git, "-C", str(work), "checkout", "--quiet", "-B", branch, BASE_BRANCH], env=environment)
            for path in group:
                (work / path).write_bytes((arguments.corpus / files[path]["right"]).read_bytes())
            run([git, "-C", str(work), "add", "--all"], env=environment)
            run([git, "-C", str(work), "commit", "--quiet", "-m", f"Diff evaluation corpus: right sides, group {index}"], env=environment)
            branches.append(branch)
        url = f"https://github.com/{REPOSITORY}.git"
        push = subprocess.run([git, "-C", str(work), "-c", "credential.helper=", "-c", "credential.helper=!gh auth git-credential",
                               "push", "--force", url, f"{BASE_BRANCH}:{BASE_BRANCH}"] + [f"{branch}:{branch}" for branch in branches],
                              capture_output=True, text=True, env=environment)
        if push.returncode != 0:
            print(push.stderr)
            raise SystemExit("push failed")
    for index, group in enumerate(groups):
        branch = f"{HEAD_PREFIX}{index:02d}"
        created = run(["gh", "pr", "create", "--repo", REPOSITORY, "--base", BASE_BRANCH, "--head", branch,
                       "--title", f"BPR diff-engine evaluation oracle, group {index} (RFC-BPR-0082)",
                       "--body", "Records GitHub's diff of part of the BetterPullReview diff-engine evaluation corpus. Closed after recording."])
        number = int(created.stdout.strip().rstrip("/").split("/")[-1])
        listed = []
        page = 1
        while True:
            body, _ = gh_get(f"repos/{REPOSITORY}/pulls/{number}/files?per_page=100&page={page}")
            items = json.loads(body) if body else []
            listed.extend(items)
            if len(items) < 100:
                break
            page += 1
        raw, _ = gh_get(f"repos/{REPOSITORY}/pulls/{number}", accept="application/vnd.github.v3.diff")
        raw_hunks = raw_diff_hunks(raw) if raw is not None else {}
        run(["gh", "pr", "close", str(number), "--repo", REPOSITORY, "--comment", "Recorded for RFC-BPR-0082; closing."])
        sources.append({"pull_request": number, "branch": branch, "files": len(group), "raw_diff": raw is not None})
        for item in listed:
            pair = files.get(item["filename"])
            if pair is None:
                continue
            listed_hunks = hunks(item.get("patch"))
            from_raw = raw_hunks.get(item["filename"])
            entry = {
                "id": pair["id"],
                "path": item["filename"],
                "pull_request": number,
                "status": item["status"],
                "base_sha": pair["left_sha"],
                "head_sha": item["sha"],
                "additions": item["additions"],
                "deletions": item["deletions"],
                "hunks": listed_hunks if listed_hunks is not None else (merge_like_file_list(from_raw) if from_raw is not None else None),
                "hunks_raw": from_raw,
                "hunk_source": "files-api" if listed_hunks is not None else ("raw-diff-merged" if from_raw is not None else "none"),
                "raw_diff_agrees": None if listed_hunks is None or from_raw is None else listed_hunks == merge_like_file_list(from_raw),
            }
            entries.append(entry)
    missing = sorted(set(pair["id"] for pair in files.values()) - set(entry["id"] for entry in entries))
    oracle = {
        "schema": "bpr.diff-evaluation.oracle.v1",
        "source": REPOSITORY,
        "recorded_on": datetime.date.today().isoformat(),
        "base_branch": BASE_BRANCH,
        "base_commit": base_commit,
        "pull_requests": sources,
        "note": ("GitHub REST pull-request file lists and raw .diff; only hunk header numbers are kept, never patch text. "
                 "`hunks` is the file list's patch, or the raw diff with hunks one unchanged line apart joined as the file list joins them."),
        "files": sorted(entries, key=lambda entry: entry["id"]),
        "not_listed": missing,
    }
    target = arguments.repo / "tests/fixtures/github/diff-oracle-v1/oracle.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(compact_json(oracle))
    print(f"ORACLE {target} files={len(entries)} without_hunks={sum(1 for entry in entries if entry['hunks'] is None)} not_listed={len(missing)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
