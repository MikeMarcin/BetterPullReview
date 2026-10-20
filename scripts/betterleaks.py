#!/usr/bin/env python3
"""Explicit, offline-after-setup BetterLeaks tooling. Never called by CI."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile


SUPPORT = Path(__file__).resolve().with_suffix("")
CACHE = ".cache/betterleaks"
LEAK_EXIT = 10
PATH_PREFIX = "__BPR_PATH_PREFIX__"
# Do not serialize Match, Line, Secret, captures, messages or author metadata.
REPORT = ('{{range .}}{{dict "file" .File "rule" .RuleID "line" .StartLine '
          '"commit" .Commit | toJSON}}{{"\\n"}}{{end}}')
RECOVERY = "Run python3 scripts/betterleaks.py setup explicitly (setup alone uses the network)."


class ToolError(Exception):
    """A diagnostic safe to show without child output or source contents."""


def run(command, *, cwd=None, env=None, data=None, allowed=(0,)):
    try:
        result = subprocess.run(command, cwd=cwd, env=env, input=data,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except OSError:
        raise ToolError("Cannot launch a required executable; check Git/Python installation and permissions.") from None
    if result.returncode not in allowed:
        raise ToolError("A local command failed; raw diagnostics withheld to protect secrets. "
                        "Check repository/object availability and permissions; no scan pass is recorded.")
    return result


def environment(home=None, *, index=False):
    # In particular, do not inherit scanner config, validation credentials,
    # Git command injection, tracing, lazy-fetch settings or dynamic loaders.
    env = {k: os.environ[k] for k in ("PATH", "SYSTEMROOT", "WINDIR", "COMSPEC", "PATHEXT")
           if k in os.environ}
    env.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_SYSTEM=os.devnull,
               GIT_CONFIG_GLOBAL=os.devnull, GIT_TERMINAL_PROMPT="0",
               GIT_NO_LAZY_FETCH="1", GIT_NO_REPLACE_OBJECTS="1", GIT_OPTIONAL_LOCKS="0",
               LC_ALL="C", NO_COLOR="1")
    if home:
        env.update(HOME=str(home), XDG_CONFIG_HOME=str(home), TMPDIR=str(home),
                   TMP=str(home), TEMP=str(home))
    if index and os.environ.get("GIT_INDEX_FILE"):
        # Git commit -a supplies its prospective index here. Do not replace it
        # with .git/index or use working-tree content for a staged scan.
        env["GIT_INDEX_FILE"] = str(Path(os.environ["GIT_INDEX_FILE"]).absolute())
    return env


def git_command(repo, *args):
    executable = shutil.which("git")
    if not executable:
        raise ToolError("Git is required. Install Git, then rerun this command; nothing was downloaded.")
    return [executable, "-C", str(repo), "-c", "protocol.allow=never",
            "-c", "core.fsmonitor=false", "-c", "core.hooksPath=" + os.devnull,
            "-c", "credential.helper=", *args]


def git(repo, *args, data=None, allowed=(0,)):
    return run(git_command(repo, *args), env=environment(index=True),
               data=data, allowed=allowed)


def repository(path):
    result = git(path, "rev-parse", "--show-toplevel")
    return Path(os.fsdecode(result.stdout.rstrip(b"\n"))).resolve()


def release(repo):
    manifest = json.loads((SUPPORT / "release.json").read_text())
    system = platform.system().lower()
    machine = {"aarch64": "arm64", "amd64": "x64", "x86_64": "x64"}.get(
        platform.machine().lower(), platform.machine().lower())
    key = f"{system}_{machine}"
    if key not in manifest["platforms"]:
        raise ToolError("No pinned BetterLeaks binary for this platform; see docs/development/betterleaks.md.")
    entry = manifest["platforms"][key]
    destination = repo / CACHE / manifest["version"] / key
    binary = destination / ("betterleaks.exe" if system == "windows" else "betterleaks")
    return manifest, entry, destination, binary


def no_symlink_parents(path):
    if any(p.is_symlink() for p in (path, *path.parents)):
        raise ToolError("Refusing a symlink in a tooling destination; restore a regular local cache/hook path.")


def verify_binary(repo):
    manifest, entry, destination, binary = release(repo)
    no_symlink_parents(binary)
    if not binary.is_file():
        raise ToolError("Pinned BetterLeaks is missing. " + RECOVERY)
    if hashlib.sha256(binary.read_bytes()).hexdigest() != entry["binary_sha256"]:
        raise ToolError("BetterLeaks binary is incompatible or changed. Remove only the conflicting "
                        f"{CACHE}/{manifest['version']}/{destination.name} directory, then: " + RECOVERY)
    with tempfile.TemporaryDirectory(prefix="bpr-betterleaks-version-") as scratch:
        result = run([str(binary), "version"], cwd=scratch, env=environment(scratch), allowed=(0, 1))
    if result.returncode or result.stdout.strip() != manifest["version"].encode():
        raise ToolError("Pinned BetterLeaks cannot run or has an incompatible CLI. " + RECOVERY)
    return binary


def setup(repo):
    manifest, entry, destination, binary = release(repo)
    no_symlink_parents(destination)
    if destination.exists():
        try:
            verify_binary(repo)
        except ToolError:
            raise ToolError("Existing cache conflicts with the pinned release; nothing replaced. "
                            f"Remove only {CACHE}/{manifest['version']}/{destination.name}, then rerun setup.") from None
        print("BetterLeaks " + manifest["version"] + " is already installed and verified; no download.")
        return
    if git(repo, "check-ignore", "--quiet", "--no-index", str(destination), allowed=(0, 1)).returncode:
        raise ToolError(f"Cache is not ignored. Add /{CACHE}/ to this checkout's ignore rules before setup.")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".install-", dir=destination.parent) as scratch:
        staging = Path(scratch) / "payload"
        staging.mkdir(mode=0o700)
        url = ("https://github.com/betterleaks/betterleaks/releases/download/v"
               + manifest["version"] + "/" + entry["asset"])
        try:
            with urllib.request.urlopen(url, timeout=90) as response:
                archive = response.read()
        except (OSError, ValueError):
            raise ToolError("BetterLeaks download failed. Check HTTPS/proxy access and rerun setup explicitly.") from None
        if hashlib.sha256(archive).hexdigest() != entry["sha256"]:
            raise ToolError("BetterLeaks archive SHA-256 mismatch; refused installation. No existing cache changed.")
        # Read exactly one regular member; never extract archive paths or links.
        if entry["asset"].endswith(".zip"):
            with zipfile.ZipFile(io.BytesIO(archive)) as package:
                content = package.read(binary.name)
        else:
            with tarfile.open(fileobj=io.BytesIO(archive)) as package:
                member = package.getmember(binary.name)
                if not member.isfile():
                    raise ToolError("Release executable is not a regular archive member.")
                content = package.extractfile(member).read()
        if hashlib.sha256(content).hexdigest() != entry["binary_sha256"]:
            raise ToolError("BetterLeaks executable SHA-256 mismatch; refused installation.")
        target = staging / binary.name
        target.write_bytes(content)
        target.chmod(0o700)
        # A concurrent setup must never be overwritten, including an empty dir.
        try:
            destination.mkdir()
        except FileExistsError:
            raise ToolError("Installation destination appeared concurrently; rerun setup to verify it.") from None
        target.rename(binary)
    verify_binary(repo)
    print("Installed verified BetterLeaks " + manifest["version"] + " in " + CACHE + ".")


def index_entries(repo):
    entries = []
    for record in git(repo, "ls-files", "--stage", "-z").stdout.split(b"\0"):
        if not record:
            continue
        metadata, name = record.split(b"\t", 1)
        mode, oid, stage = metadata.split()
        if stage != b"0":
            raise ToolError("Unmerged index entries: resolve the conflict and stage the result before scanning.")
        entries.append((mode, oid, name))
    return entries


def checked_path(root, name):
    parts = os.fsdecode(name).split("/")
    if any(p in ("", ".", "..") for p in parts) or Path(os.fsdecode(name)).is_absolute():
        raise ToolError("Unsupported Git path; scan refused.")
    return root.joinpath(*parts)


def copy_index(repo, entries, destination):
    process = subprocess.Popen(git_command(repo, "cat-file", "--batch"),
                               env=environment(index=True), stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    try:
        for _, oid, name in entries:
            process.stdin.write(oid + b"\n")
            process.stdin.flush()
            header = process.stdout.readline().split()
            if len(header) != 3 or header[:2] != [oid, b"blob"]:
                raise ToolError("An index blob is unavailable locally; scan refused without fetching.")
            size = int(header[2])
            path = checked_path(destination, name)
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("xb") as output:
                remaining = size
                while remaining:
                    block = process.stdout.read(min(remaining, 1024 * 1024))
                    if not block:
                        raise ToolError("Incomplete local Git blob; scan refused.")
                    output.write(block)
                    remaining -= len(block)
            if process.stdout.read(1) != b"\n":
                raise ToolError("Invalid local Git object stream; scan refused.")
        process.stdin.close()
        if process.wait() != 0:
            raise ToolError("Could not read the local index objects; scan refused.")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait()
        process.stdout.close()
        if not process.stdin.closed:
            process.stdin.close()


def copy_working(repo, entries, destination):
    missing = 0
    for _, _, name in entries:
        source = checked_path(repo, name)
        no_symlink_parents(source.parent)
        target = checked_path(destination, name)
        try:
            mode = source.lstat().st_mode
        except FileNotFoundError:
            missing += 1
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        if stat.S_ISLNK(mode):
            target.write_bytes(os.fsencode(os.readlink(source)))
        elif stat.S_ISREG(mode):
            descriptor = os.open(source, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
            with os.fdopen(descriptor, "rb") as incoming, target.open("xb") as outgoing:
                shutil.copyfileobj(incoming, outgoing)
        else:
            raise ToolError("A tracked path is not a regular file or symlink; scan refused.")
    return missing


def history_view(repo, destination):
    # The scanner never gets the real repo's config, hooks, attributes or
    # ignore files. This bare view borrows local objects but has no remote.
    object_format = git(repo, "rev-parse", "--show-object-format").stdout.strip().decode("ascii")
    git(destination.parent, "init", "--quiet", "--bare", "--template=",
        "--object-format=" + object_format, str(destination))
    objects = git(repo, "rev-parse", "--path-format=absolute", "--git-path", "objects").stdout.rstrip(b"\n")
    if b"\n" in objects:
        raise ToolError("Newlines in the object-directory path are unsupported.")
    (destination / "objects/info/alternates").write_bytes(objects + b"\n")
    references = git(repo, "for-each-ref", "--format=%(objectname) %(refname)").stdout.splitlines()
    updates = []
    for reference in references:
        oid, ref = reference.split(b" ", 1)
        updates.append(b"update " + ref + b" " + oid + b"\n")
    head = git(repo, "rev-parse", "--verify", "HEAD", allowed=(0, 128))
    if head.returncode == 0:
        # HEAD may be detached, or outside every branch/tag.
        updates.append(b"update refs/bpr-scan-head " + head.stdout.strip() + b"\n")
    shallow = Path(os.fsdecode(git(repo, "rev-parse", "--path-format=absolute",
                                  "--git-path", "shallow").stdout.rstrip(b"\n")))
    if shallow.is_file():
        (destination / "shallow").write_bytes(shallow.read_bytes())
        print("Coverage: shallow history; only the locally recorded boundary is available.")
    if updates:
        git(destination, "update-ref", "--stdin", data=b"".join(updates))
    return bool(updates)


def scan(repo, mode):
    binary = verify_binary(repo)
    with tempfile.TemporaryDirectory(prefix="bpr-betterleaks-") as scratch_name:
        scratch = Path(scratch_name)
        policy = (SUPPORT / "config.toml").read_text()
        # Each mode uses one exact path namespace. An optional prefix would
        # also exempt a different historical file named input/files/<path>.
        policy = policy.replace(PATH_PREFIX, "" if mode == "history" else "input/files/")
        (scratch / "config.toml").write_text(policy)
        (scratch / "empty.ignore").touch()
        (scratch / "report.tmpl").write_text(REPORT)
        source = scratch / "input"
        if mode == "history":
            if not history_view(repo, source):
                print("BetterLeaks history: no local commits or refs to scan.")
                return 0
            command = ["git", str(source), "--log-opts=--all --full-history --root -m --text "
                       "--no-renames --no-ext-diff --no-textconv --no-color"]
        else:
            entries = index_entries(repo)
            if mode == "staged":
                changed = set(git(repo, "diff", "--cached", "--name-only", "--no-renames",
                                  "--no-ext-diff", "--no-textconv", "--diff-filter=ACMT", "-z").stdout.split(b"\0"))
                entries = [entry for entry in entries if entry[2] in changed]
            submodules = sum(file_mode == b"160000" for file_mode, _, _ in entries)
            entries = [entry for entry in entries if entry[0] != b"160000"]
            if submodules:
                print(f"Coverage: {submodules} submodule entries excluded; scan those repositories separately.")
            # Ignore/config files are scanned as content beneath files/, never
            # interpreted at the scanner's source root (v1.9.0 loads that root
            # even when --gitleaks-ignore-path points somewhere else).
            files = source / "files"
            files.mkdir(parents=True)
            if mode == "staged":
                copy_index(repo, entries, files)
            else:
                missing = copy_working(repo, entries, files)
                if missing:
                    print(f"Coverage: {missing} tracked paths absent on disk (deleted or sparse); use staged/history for stored blobs.")
            print(f"BetterLeaks {mode}: {len(entries)} selected tracked files.")
            command = ["dir", "input"]
        args = [str(binary), *command, "--config", str(scratch / "config.toml"),
                "--gitleaks-ignore-path", str(scratch / "empty.ignore"),
                "--ignore-gitleaks-allow", "--validation=false", "--redact=100",
                "--max-decode-depth=5", "--max-archive-depth=8", "--max-target-megabytes=0",
                "--report-format=template", "--report-template", str(scratch / "report.tmpl"),
                "--report-path=-", "--exit-code=" + str(LEAK_EXIT),
                "--no-banner", "--no-color", "--log-level=error"]
        result = run(args, cwd=scratch, env=environment(scratch), allowed=(0, 1, LEAK_EXIT))
        if result.returncode not in (0, LEAK_EXIT) or result.stderr:
            raise ToolError("BetterLeaks could not complete the scan. Raw diagnostics withheld. "
                            "Check local Git objects and file permissions, then verify the installation with setup.")
        findings = [json.loads(line) for line in result.stdout.splitlines()]
        if bool(findings) != (result.returncode == LEAK_EXIT):
            raise ToolError("BetterLeaks returned an inconsistent report; no pass recorded. " + RECOVERY)
        for finding in sorted(findings, key=lambda f: (f["file"], f["line"], f["rule"], f["commit"])):
            path = finding["file"]
            prefix = "input/files/"
            if os.name == "nt":
                path = path.replace("\\", "/")
            if mode != "history" and path.startswith(prefix):
                path = path[len(prefix):]
            print(json.dumps({"file": path, "line": finding["line"], "rule": finding["rule"],
                              "commit": finding["commit"], "secret": "REDACTED"}, ensure_ascii=True))
        print(f"BetterLeaks {mode}: {len(findings)} findings.")
        return 1 if findings else 0


def config(repo, *args):
    # Hook management must see the real effective config, unlike scan plumbing.
    executable = shutil.which("git")
    if not executable:
        raise ToolError("Git is required for hook management.")
    # Honor effective hooksPath (including user config), but do not let a
    # caller's Git routing variables redirect a write into another checkout.
    routing = {"GIT_DIR", "GIT_COMMON_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE",
               "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES", "GIT_CONFIG"}
    env = {k: v for k, v in os.environ.items() if k not in routing and not k.startswith("GIT_TRACE")}
    return run([executable, "-C", str(repo), "config", *args], env=env, allowed=(0, 1, 5))


def hook_location(repo):
    git_dir = Path(os.fsdecode(git(repo, "rev-parse", "--absolute-git-dir").stdout.rstrip(b"\n")))
    common = Path(os.fsdecode(git(repo, "rev-parse", "--path-format=absolute", "--git-common-dir").stdout.rstrip(b"\n")))
    enabled = config(repo, "--local", "--bool", "--get", "extensions.worktreeConfig")
    isolated = git_dir != common or enabled.stdout.strip() == b"true"
    hook_dir = git_dir / "betterleaks-hooks" if isolated else common / "hooks"
    current = config(repo, "--get-all", "core.hooksPath")
    setting = (str(hook_dir) + "\n").encode()
    owned_path = (isolated and current.stdout == setting
                  and config(repo, "--worktree", "--get-all", "core.hooksPath").stdout == setting)
    if current.returncode == 0 and not owned_path:
        raise ToolError("Custom core.hooksPath is configured; preserved unchanged. "
                        "Add the staged command to your existing hook manually; see the local tooling guide.")
    if isolated:
        if enabled.stdout.strip() != b"true":
            raise ToolError("Linked worktree needs existing extensions.worktreeConfig=true for isolated hook installation. "
                            "Shared hooks/config were preserved; see the local tooling guide.")
    elif git(repo, "worktree", "list", "--porcelain").stdout.count(b"worktree ") > 1:
        raise ToolError("Multiple worktrees share these hooks; refusing a shared installation. "
                        "Use manual integration or existing worktree-scoped configuration.")
    return hook_dir, common / "hooks", isolated, owned_path


def hook_text(repo):
    invocation = " ".join(shlex.quote(str(p)) for p in
                          (Path(__file__).resolve(), "--repo", repo, "staged"))
    return ("#!/bin/sh\n# BetterPullReview BetterLeaks managed hook v1\n"
            "if ! command -v python3 >/dev/null 2>&1; then\n"
            "  echo 'BetterLeaks hook: Python 3.10+ is required; install Python or remove the opt-in hook.' >&2\n"
            "  exit 2\nfi\nexec python3 " + invocation + "\n").encode()


def manage_hook(repo, action):
    hook_dir, shared_hooks, isolated, owned_path = hook_location(repo)
    hook = hook_dir / "pre-commit"
    no_symlink_parents(hook)
    expected = hook_text(repo)
    exists = hook.exists()
    if exists and (not hook.is_file() or hook.read_bytes() != expected):
        raise ToolError("Existing pre-commit hook differs; refusing replacement/removal. "
                        "It was preserved. See docs/development/betterleaks.md for manual integration.")
    if action == "hook-status":
        active = exists and os.access(hook, os.X_OK) and (not isolated or owned_path)
        print("BetterLeaks hook: " + ("active in this checkout." if active else "inactive in this checkout."))
        return
    if action == "hook-remove":
        if isolated and owned_path:
            result = config(repo, "--worktree", "--unset-all", "core.hooksPath")
            if result.returncode:
                raise ToolError("Could not remove the owned worktree hook setting; hook file preserved.")
        if exists:
            hook.unlink()
        if isolated and hook_dir.exists() and not any(hook_dir.iterdir()):
            hook_dir.rmdir()
        print("BetterLeaks hook removed from this checkout; existing hooks preserved.")
        return
    verify_binary(repo)
    if isolated and not owned_path and shared_hooks.is_dir():
        if any(not p.name.endswith(".sample") for p in shared_hooks.iterdir()):
            raise ToolError("Shared hooks already exist; refusing to hide them with a worktree hooksPath. "
                            "Use manual integration in your existing hook manager.")
    hook_dir.mkdir(parents=True, exist_ok=True)
    if not exists:
        with hook.open("xb") as output:
            output.write(expected)
    hook.chmod(0o700)
    if isolated and not owned_path:
        result = config(repo, "--worktree", "core.hooksPath", str(hook_dir))
        if result.returncode:
            raise ToolError("Could not activate the hook; retry hook-install or use hook-remove.")
    print("BetterLeaks hook active in this checkout.")


def main():
    if sys.version_info < (3, 10):
        raise ToolError("Python 3.10+ is required. Run this command with a newer python3.")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1],
                        help="checkout to scan/manage (default: this script's checkout)")
    parser.add_argument("command", choices=("setup", "staged", "tracked", "history",
                                            "hook-install", "hook-remove", "hook-status"))
    args = parser.parse_args()
    if os.environ.get("GITHUB_ACTIONS"):
        raise ToolError("This tooling is local-only and must not run in GitHub Actions.")
    repo = repository(args.repo)
    if args.command == "setup":
        setup(repo)
    elif args.command.startswith("hook-"):
        manage_hook(repo, args.command)
    else:
        return scan(repo, args.command)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ToolError as error:
        print("BetterLeaks: " + str(error), file=sys.stderr)
        sys.exit(2)
    except (OSError, ValueError, KeyError, tarfile.TarError, zipfile.BadZipFile):
        print("BetterLeaks: local input/configuration error; details withheld to protect secrets. "
              "Check file permissions and the tracked tooling files. " + RECOVERY, file=sys.stderr)
        sys.exit(2)
    except KeyboardInterrupt:
        print("BetterLeaks: interrupted; no scan pass recorded.", file=sys.stderr)
        sys.exit(130)
