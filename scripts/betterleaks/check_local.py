#!/usr/bin/env python3
"""Explicit local integration course; deliberately outside CI/test discovery.

Install with ../betterleaks.py setup first. No test downloads anything. On macOS
the entire course reruns under Seatbelt with network access denied.
"""

from __future__ import annotations

import importlib.util
import io
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import tomllib
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "betterleaks.py"
ROOT = SCRIPT.parents[1]
spec = importlib.util.spec_from_file_location("local_betterleaks", SCRIPT)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)
# Assemble synthetic canaries so this test's own source is not a secret fixture.
CANARY = "ghp_" + "8xYt3QmP" + "7vRs9WkL" + "2nBd6FcH" + "5jAz4UeG" + "0iOp"
OTHER = "ghp_" + "9aZs2WnQ" + "6uRt8VkM" + "3mCe7GbJ" + "4hBy5TfH" + "1jPq"
REVIEW_CANARY = "w" * 43


def require(condition, message):
    if not condition:
        # Never include captured output in an assertion: broken redaction is
        # precisely one of the failures this course must be safe to report.
        raise AssertionError(message)


def call(command, env, expected=0):
    result = subprocess.run([str(a) for a in command], env=env, capture_output=True)
    require(all(v.encode() not in result.stdout + result.stderr
                for v in (CANARY, OTHER, REVIEW_CANARY)),
            "Secret appeared in child output (output withheld)")
    if expected is not None:
        require(result.returncode == expected,
                f"Unexpected exit {result.returncode}, expected {expected}; child output withheld")
    return result


class Repository:
    def __init__(self, parent, name="repo"):
        self.path = parent / name
        self.path.mkdir()
        self.env = tool.environment(parent)
        call(["git", "init", "--quiet", "--template=", self.path], self.env)
        self.git("config", "user.name", "Local scanner test")
        self.git("config", "user.email", "scanner@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.write(".gitignore", "/.cache/betterleaks/\n")
        self.git("add", ".gitignore")
        self.install_cache()

    def install_cache(self):
        source = tool.verify_binary(ROOT)
        target = tool.release(self.path)[3]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    def git(self, *args, expected=0, env=None):
        return call(["git", "-C", self.path, *args], env or self.env, expected)

    def scan(self, command, expected=0, env=None):
        return call([sys.executable, SCRIPT, "--repo", self.path, command], env or self.env, expected)

    def write(self, name, content):
        path = self.path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)


def staged_and_commits(parent):
    r = Repository(parent)
    r.scan("hook-install")
    r.scan("hook-install")
    r.scan("hook-status")
    r.write("src/one.cpp", "clean initial\n")
    r.git("add", "src")
    r.scan("staged")
    r.git("commit", "-qm", "clean initial")
    initial = r.git("rev-parse", "HEAD").stdout
    r.write("src/one.cpp", CANARY + "\n")
    r.git("add", "src")
    r.write("src/one.cpp", "clean initial\n")
    r.scan("staged", 1)
    r.scan("tracked")
    r.git("commit", "-qm", "must fail", expected=1)
    require(r.git("rev-parse", "HEAD").stdout == initial, "Rejected commit changed HEAD")
    r.write("src/one.cpp", "clean staged\n")
    r.git("add", "src")
    r.write("src/one.cpp", OTHER + "\n")
    r.scan("staged")
    r.scan("tracked", 1)
    r.git("commit", "-qm", "only staged content")
    clean = r.git("rev-parse", "HEAD").stdout
    # A real Git commit, not a manually supplied substitute index.
    r.git("commit", "-qam", "commit-a must fail", expected=1)
    require(r.git("rev-parse", "HEAD").stdout == clean, "commit -a bypassed index scan")
    r.write("src/one.cpp", "clean tracked\n")
    r.git("commit", "-qam", "clean commit-a")
    r.scan("history")
    r.scan("hook-remove")
    r.scan("hook-remove")
    require(not (r.path / ".git/hooks/pre-commit").exists(), "Hook removal failed")


def initial_and_paths(parent):
    r = Repository(parent)
    r.scan("hook-install")
    for name in ("src/a.cpp", "tests/test.cpp", "docs/guide.md", "odd \t\nname.txt", "package-lock.json"):
        r.write(name, CANARY + "\n")
    r.git("add", ".")
    output = r.scan("staged", 1).stdout
    for name in ("src/a.cpp", "tests/test.cpp", "docs/guide.md", "package-lock.json"):
        require(name.encode() in output, "Source/test/doc/default-prefilter coverage missing")
    r.git("commit", "-qm", "unborn secret", expected=1)
    r.scan("hook-remove")
    r.git("commit", "-qm", "synthetic history")
    r.git("rm", "-r", "--quiet", "src", "tests", "docs", "odd \t\nname.txt", "package-lock.json")
    r.scan("staged")
    r.git("commit", "-qm", "deletion")
    r.scan("tracked")
    r.scan("history", 1)


def suppressions_and_refs(parent):
    r = Repository(parent)
    r.write("tests/fixture.md", CANARY + " # gitleaks:allow betterleaks:allow\n")
    r.write(".betterleaks.toml", 'prefilter = "true"\n')
    r.write(".gitleaks.toml", 'prefilter = "true"\n')
    r.write(".gitignore", "/.cache/betterleaks/\ntests/\n")
    r.write(".gitattributes", "* -diff export-ignore\n")
    r.git("add", ".")
    r.git("add", "-f", "tests/fixture.md")
    ambient = dict(r.env, BETTERLEAKS_CONFIG=str(r.path / ".betterleaks.toml"),
                   GITLEAKS_CONFIG=str(r.path / ".gitleaks.toml"),
                   BETTERLEAKS_CONFIG_TOML='prefilter="true"',
                   GITLEAKS_CONFIG_TOML='prefilter="true"',
                   BETTERLEAKS_VALIDATION="true", GITLEAKS_VALIDATION="true")
    r.scan("staged", 1, ambient)
    r.scan("tracked", 1, ambient)
    r.git("commit", "-qm", "synthetic ignored fixture")
    oid = r.git("rev-parse", "HEAD").stdout.strip().decode()
    fingerprint = f"{oid}:tests/fixture.md:github-pat:1\n"
    r.write(".betterleaksignore", fingerprint)
    r.write(".gitleaksignore", fingerprint)
    r.scan("history", 1, ambient)
    # Pin the upstream trap: even -i an empty file does NOT disable the
    # source-root ignore file. The wrapper's isolated view must defeat this.
    empty = parent / "empty.ignore"
    empty.touch()
    result = call([tool.verify_binary(ROOT), "git", r.path, "--config", tool.SUPPORT / "config.toml",
                   "--ignore-gitleaks-allow", "--gitleaks-ignore-path", empty,
                   "--redact=100", "--validation=false", "--no-banner", "--log-level=error"], r.env)
    require(result.returncode == 0, "Upstream ignore-root behavior changed; review isolation")
    r.git("tag", "only-tag", oid)
    r.git("update-ref", "refs/remotes/offline/topic", oid)
    r.git("checkout", "--orphan", "clean-branch")
    r.git("rm", "-rf", "--quiet", ".")
    r.git("commit", "--allow-empty", "-qm", "clean root")
    r.git("branch", "-D", "master", expected=None)
    r.git("branch", "-D", "main", expected=None)
    output = r.scan("history", 1).stdout
    require(oid.encode() in output, "All-ref history missed local tag/remote tracking refs")
    r.git("tag", "-d", "only-tag")
    r.git("update-ref", "-d", "refs/remotes/offline/topic")
    r.scan("history")
    r.git("checkout", "--detach", oid)
    r.scan("history", 1)


def missing_and_conflicts(parent):
    r = Repository(parent)
    r.scan("setup")  # Existing verified cache must be usable offline.
    binary = tool.release(r.path)[3]
    binary.unlink()
    require(b"setup" in r.scan("staged", 2).stderr, "Missing scanner has no recovery command")
    r.install_cache()
    binary.write_bytes(b"incompatible")
    r.scan("setup", 2)
    r.scan("staged", 2)
    r.install_cache()
    hooks = r.path / ".git/hooks"
    hooks.mkdir(exist_ok=True)
    hook = hooks / "pre-commit"
    original = b"#!/bin/sh\nexit 0\n"
    hook.write_bytes(original)
    r.scan("hook-install", 2)
    r.scan("hook-remove", 2)
    require(hook.read_bytes() == original, "Conflicting hook changed")
    hook.unlink()
    custom = parent / "custom-hooks"
    custom.mkdir()
    r.git("config", "core.hooksPath", str(custom))
    r.scan("hook-install", 2)
    require(r.git("config", "--get", "core.hooksPath").stdout.strip() == str(custom).encode(),
            "Custom hooksPath changed")
    r.git("config", "--unset", "core.hooksPath")
    r.scan("hook-install")
    no_python = parent / "only-git"
    no_python.mkdir()
    (no_python / "git").symlink_to(shutil.which("git"))
    result = r.git("commit", "-qm", "missing Python blocks commit", expected=1,
                   env=dict(r.env, PATH=str(no_python)))
    require(b"Python 3.10+" in result.stderr, "Missing Python recovery diagnostic absent")
    binary.unlink()
    r.git("commit", "-qm", "missing scanner blocks commit", expected=1)
    # Missing Git: Python is selected by absolute path and therefore still runs.
    no_path = dict(r.env, PATH=str(parent / "empty-path"))
    require(b"Git is required" in r.scan("staged", 2, no_path).stderr, "Missing Git diagnostic absent")


def setup_checksum_refusal(parent):
    r = Repository(parent)
    destination = tool.release(r.path)[2]
    shutil.rmtree(destination)
    with mock.patch.object(tool.urllib.request, "urlopen", return_value=io.BytesIO(b"invalid archive")) as download:
        try:
            tool.setup(r.path)
        except tool.ToolError as error:
            require("SHA-256 mismatch" in str(error), "Corrupt archive had wrong failure")
        else:
            raise AssertionError("Setup accepted an invalid archive")
        require("/download/v1.9.0/" in download.call_args.args[0], "Setup did not use a pinned URL")
    require(not destination.exists(), "Checksum failure installed a destination")


def linked_worktree(parent):
    r = Repository(parent)
    r.git("commit", "-qm", "base")
    linked = parent / "linked"
    r.git("worktree", "add", "--detach", linked)
    # No extension: refuse shared mutation.
    call([sys.executable, SCRIPT, "--repo", linked, "hook-install"], r.env, 2)
    r.git("config", "extensions.worktreeConfig", "true")
    target = tool.release(linked)[3]
    target.parent.mkdir(parents=True)
    shutil.copy2(tool.verify_binary(ROOT), target)
    before = (r.path / ".git/config").read_bytes()
    for action in ("hook-install", "hook-install", "hook-status"):
        call([sys.executable, SCRIPT, "--repo", linked, action], r.env)
    require((r.path / ".git/config").read_bytes() == before, "Shared Git config changed")
    require(r.git("config", "--get", "core.hooksPath", expected=1).returncode == 1,
            "Sibling worktree hook configuration changed")
    for action in ("hook-remove", "hook-remove"):
        call([sys.executable, SCRIPT, "--repo", linked, action], r.env)
    r.git("worktree", "remove", "--force", linked)


def fixture_exceptions(parent):
    """Prove each exception requires its exact rule AND file AND fake value."""
    r = Repository(parent)
    policy = tomllib.loads((tool.SUPPORT / "config.toml").read_text())
    entries = []
    for rule in policy.get("rules", []):
        for allow in rule.get("allowlists", []):
            require(allow["condition"] == "AND" and allow["regexTarget"] == "secret",
                    "Exception lost its conjunctive scope")
            pattern = allow["paths"][0]
            prefix = "^" + tool.PATH_PREFIX
            require(pattern.startswith(prefix) and pattern.endswith("$"),
                    "Exception path must be anchored and exact")
            name = re.sub(r"\\(.)", r"\1", pattern[len(prefix):-1])
            values = []
            for regex in allow["regexes"]:
                require(regex.startswith("^\\x") and regex.endswith("$"), "Exception value must be exact")
                value = re.sub(r"\\x([0-9a-f]{2})", lambda m: chr(int(m[1], 16)), regex[1:-1])
                values.append(re.sub(r"\\(.)", r"\1", value))
            entries.append((name, rule["id"], values))

    def content(rule, value):
        if rule == "generic-api-key":
            return 'api_key = "' + value + '"\n'
        if rule == "generic-credential-uri":
            return "https://user:" + value + "@example.invalid\n"
        return value + "\n"

    for name, rule, values in entries:
        r.write(name, "".join(content(rule, v) for v in values))
    r.git("add", ".")
    r.scan("staged")
    r.scan("tracked")
    r.git("commit", "-qm", "reviewed synthetic fixtures")
    r.scan("history")
    for name, rule, values in entries:
        r.write("negative/" + name, "".join(content(rule, v) for v in values))
        r.write("input/files/" + name, "".join(content(rule, v) for v in values))
        # Same rule, different value in the allowed file must still fail.
        value = "gho_" + OTHER[4:] if rule == "github-oauth" else OTHER[4:]
        r.write(name, content(rule, value) + CANARY + "\n")
    r.git("add", ".")
    result = r.scan("staged", 1)
    findings = [json.loads(line) for line in result.stdout.splitlines() if line.startswith(b"{")]
    for name, rule, _ in entries:
        require(any(f["file"] == name and f["rule"] == rule for f in findings),
                "Exception hid another value of the same rule")
        require(any(f["file"] == "negative/" + name and f["rule"] == rule for f in findings),
                "Exception hid the fake value in another file")
        require(any(f["file"] == name and f["rule"] == "github-pat" for f in findings),
                "Exception hid another rule in the allowed file")
    r.git("commit", "-qm", "synthetic exception negative controls")
    result = r.scan("history", 1)
    history = [json.loads(line) for line in result.stdout.splitlines() if line.startswith(b"{")]
    for name, rule, _ in entries:
        require(any(f["file"] == "input/files/" + name and f["rule"] == rule for f in history),
                "Snapshot prefix exempted a different historical path")
    print(f"Checked {len(entries)} rule/file exceptions with value, path and rule negative controls.")


def missing_objects_and_index(parent):
    r = Repository(parent)
    r.write("tracked.txt", "clean\n")
    r.git("add", ".")
    r.git("commit", "-qm", "base")
    other_index = parent / "alternate-index"
    index_env = dict(r.env, GIT_INDEX_FILE=str(other_index))
    r.git("read-tree", "HEAD", env=index_env)
    r.write("tracked.txt", CANARY + "\n")
    r.git("add", "tracked.txt", env=index_env)
    r.write("tracked.txt", "clean\n")
    r.scan("staged")
    r.scan("staged", 1, index_env)
    oid = r.git("rev-parse", ":tracked.txt", env=index_env).stdout.strip().decode()
    (r.path / ".git/objects" / oid[:2] / oid[2:]).unlink()
    r.git("config", "remote.origin.url", "https://127.0.0.1:9/never-fetch")
    r.git("config", "remote.origin.promisor", "true")
    r.scan("staged", 2, index_env)
    # A cache conflict is refused without making a network request.
    tool.release(r.path)[3].unlink()
    r.scan("setup", 2)


def review_session_urls(parent):
    r = Repository(parent)
    r.scan("hook-install")
    r.git("commit", "-qm", "clean initial")
    initial = r.git("rev-parse", "HEAD").stdout
    for number, host in enumerate(("127.0.0.1", "localhost", "[::1]")):
        # Low entropy deliberately exercises the dedicated rule, independent
        # of generic API-key heuristics. No real session is contacted.
        url = f"http://{host}:12345/#" + "token=" + REVIEW_CANARY
        r.write(f"docs/handoff-{number}.txt", url + " # gitleaks:allow betterleaks:allow\n")
    r.git("add", ".")
    for mode in ("staged", "tracked"):
        result = r.scan(mode, 1)
        require(result.stdout.count(b'"rule": "bpr-review-session-url"') == 3,
                "Dedicated review URL coverage missing")
    r.git("commit", "-qm", "review URLs must fail", expected=1)
    require(r.git("rev-parse", "HEAD").stdout == initial, "Review URL commit changed HEAD")
    r.scan("hook-remove")
    r.git("commit", "-qm", "synthetic historical review URLs")
    result = r.scan("history", 1)
    require(result.stdout.count(b'"rule": "bpr-review-session-url"') == 3,
            "Historical review URL coverage missing")
    for number in range(3):
        r.write(f"docs/handoff-{number}.txt", "session: example; url: stopped\n")
    r.git("add", ".")
    r.scan("hook-install")
    r.git("commit", "-qm", "retain only session metadata")
    r.scan("tracked")


def main():
    if os.environ.get("GITHUB_ACTIONS"):
        print("This scanner course is local-only; it must not run in GitHub Actions.")
        return 2
    if sys.platform == "darwin" and "--network-denied" not in sys.argv:
        print("Running the entire local course with macOS network access denied.", flush=True)
        return subprocess.call(["/usr/bin/sandbox-exec", "-p",
                                "(version 1)(allow default)(deny network*)",
                                sys.executable, str(Path(__file__).resolve()), "--network-denied"])
    if "--network-denied" in sys.argv:
        try:
            with socket.socket() as probe:
                probe.connect(("127.0.0.1", 9))
        except PermissionError:
            pass
        else:
            raise AssertionError("Network denial was not enforced")
    else:
        print("OS network denial not exercised on this platform; commands still use offline-only paths.")
    tool.verify_binary(ROOT)
    courses = (staged_and_commits, initial_and_paths, suppressions_and_refs,
               missing_and_conflicts, linked_worktree, fixture_exceptions,
               missing_objects_and_index, setup_checksum_refusal, review_session_urls)
    for course in courses:
        with tempfile.TemporaryDirectory(prefix="bpr-betterleaks-course-") as directory:
            course(Path(directory).resolve())
        print("PASS " + course.__name__, flush=True)
    print(f"PASS {len(courses)} local integration courses using the real pinned binary.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, tool.ToolError) as error:
        print("FAIL: " + str(error), file=sys.stderr)
        sys.exit(1)
