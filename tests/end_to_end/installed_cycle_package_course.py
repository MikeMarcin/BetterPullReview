#!/usr/bin/env python3
"""[[BPR-1182]]. The installed macOS packages, started natively, and swapped over one profile.

Each package is a tests-disabled universal candidate from `tools/release.sh
--prepare-only`, so nothing inside it can be driven; this course reaches it only
from outside. For the full, agent-free and agent-free, automation-off packages
in turn it installs the `.app` into one place, starts it on the Cocoa platform
with a clean profile of its own (`HOME` and `CFFIXED_USER_HOME`; macOS keeps
Application Support under the account's home whatever `HOME` says), and
watches what the started application does:

- it composes the profile (the registry appears) and stays up;
- it starts no child process (sampled every 50 ms), holds no network socket,
  and runs none of the executables named like agent CLIs placed first on PATH;
- it binds its activation endpoint, and binds the owner command endpoint `bpr`
  uses exactly when the package was built with local automation.

Between launches the packages replace one another in place, as an install over
an install does, on the same profile. Before the first swap the course's `bpr`
(a tests-enabled agent-free build, used only to make and read the state)
opens a cycle on a Git checkout with work in progress, adds a note and
freezes F1; after each package has run, the same cycle, note and round read
back byte for byte. A package never needs `bpr`, and nothing here contacts a
network or a model.

The root is made under the account's home, not the system temporary
directory: the default data root is resolved through Foundation, which
standardises `/private/var/...` to `/var/...`, and the store refuses a root
whose canonical path differs. Arguments: --package-root DIR holding
bpr1182-full, bpr1182-agent-free and bpr1182-agent-free-automation-off; --bpr
PATH. Prints INSTALLED_CYCLE_PACKAGE_COURSE_OK.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.contained_temporary_support import make_contained_temporary_directory  # noqa: E402

AGENT_NAMES = ("claude", "codex", "gemini", "aider", "copilot", "cursor-agent", "ollama", "llm", "openai")
VARIANTS = (
    ("full", True, True),
    ("agent-free", False, True),
    ("agent-free-automation-off", False, False),
    ("full", True, True),
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"INSTALLED_CYCLE_PACKAGE_COURSE_FAILED: {message}")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def children(pid: int) -> list[str]:
    listed = subprocess.run(["pgrep", "-l", "-P", str(pid)], capture_output=True, text=True)
    return [line for line in listed.stdout.splitlines() if line.strip()]


def network_sockets(pid: int) -> list[str]:
    listed = subprocess.run(["lsof", "-nP", "-a", "-p", str(pid), "-i"], capture_output=True, text=True)
    return [line for line in listed.stdout.splitlines()[1:] if line.strip()]


def bound_endpoints(pid: int, run: Path) -> list[str]:
    """The endpoints the process itself holds bound in `run`: a socket file a
    previous, terminated instance left behind is not one of them."""
    listed = subprocess.run(["lsof", "-nP", "-a", "-p", str(pid), "-U", "-Fn"], capture_output=True, text=True)
    names = [Path(line[1:]) for line in listed.stdout.splitlines() if line.startswith("n") and line[1:].startswith(str(run))]
    return sorted(name.name.split("-")[1] for name in names if name.name.startswith("bpr-"))


def launch(app: Path, environment: dict[str, str], cwd: Path, data: Path, run: Path, automation: bool, label: str) -> dict:
    """One native start: composed, quiet, the right endpoints; then a polite quit."""
    executable = app / "Contents/MacOS/BetterPullReview"
    log_path = cwd / f"launch-{label.replace(':', '-')}.log"
    log = log_path.open("wb")
    child = subprocess.Popen([str(executable)], env=environment, cwd=cwd, stdout=log, stderr=subprocess.STDOUT)
    registry = data / "state/v1/profile-registry.sqlite3"
    seen_children: list[str] = []
    deadline = time.monotonic() + 30
    try:
        while not registry.exists() or not list(run.glob("bpr-activate-*.sock")):
            if child.poll() is not None:
                require(False, f"{label}: exited {child.returncode} before composing: {log_path.read_text(errors='replace')[-800:]}")
            require(time.monotonic() < deadline, f"{label}: no registry or activation endpoint within 30 s")
            seen_children += children(child.pid)
            time.sleep(0.05)
        settle = time.monotonic() + 5
        while time.monotonic() < settle:
            require(child.poll() is None, f"{label}: exited {child.returncode} while idle")
            seen_children += children(child.pid)
            time.sleep(0.05)
        sockets = network_sockets(child.pid)
        endpoints = bound_endpoints(child.pid, run)
    finally:
        if child.poll() is None:
            child.send_signal(signal.SIGTERM)
        try:
            child.wait(timeout=20)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait(timeout=10)
        log.close()
    output = log_path.read_text(errors="replace")
    require(seen_children == [], f"{label}: started children {sorted(set(seen_children))}")
    require(sockets == [], f"{label}: network sockets {sockets}")
    owner = "tourcmd" in endpoints
    require("activate" in endpoints, f"{label}: no activation endpoint among {endpoints}")
    require(owner == automation, f"{label}: owner command endpoint {'absent' if automation else 'bound'} with local automation {'built' if automation else 'absent'}")
    return {"variant": label, "executable_sha256": digest(executable), "children": 0, "network_sockets": 0,
            "endpoints": endpoints, "exit": child.returncode, "output_tail": output.strip().splitlines()[-1:] if output else []}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--bpr", type=Path, required=True)
    args = parser.parse_args()
    args.package_root, args.bpr = args.package_root.resolve(), args.bpr.resolve()
    packages = {name: args.package_root / f"bpr1182-{name}" / "BetterPullReview.app" for name, _, _ in VARIANTS}
    for name, app in packages.items():
        require((app / "Contents/MacOS/BetterPullReview").is_file(), f"{name}: no package at {app}")
    root = make_contained_temporary_directory(prefix="bpr1182-installed-", parent=Path.home()).resolve()
    try:
        os.chmod(root, 0o700)
        home, run, installed, canaries = root / "home", root / "run", root / "Applications", root / "agent-canaries"
        for directory in (home, run, installed, canaries):
            directory.mkdir(mode=0o700)
        marks = root / "agent-ran"
        for name in AGENT_NAMES:
            script = canaries / name
            script.write_text(f"#!/bin/sh\necho {name} >> '{marks}'\nexit 3\n", encoding="utf-8")
            script.chmod(0o755)
        environment = {key: value for key, value in os.environ.items() if not key.startswith(("GH_", "GITHUB_", "QT_", "DYLD_", "BPR_"))}
        environment.update(HOME=str(home), CFFIXED_USER_HOME=str(home), TMPDIR=str(run) + "/",
                           PATH=f"{canaries}:/usr/bin:/bin:/usr/sbin:/sbin")
        data = home / "Library/Application Support/BetterPullReview/BetterPullReview"

        # A Git checkout with work in progress: one CRLF file with tabs, one changed after the commit.
        project = root / "config-parser"
        (project / "src").mkdir(parents=True)
        (project / "src/parser.cpp").write_bytes(b"int parse_value(const char* aText)\r\n{\r\n\treturn aText == nullptr ? -1 : 0;\r\n}\r\n")
        (project / "README.md").write_bytes(b"# Config parser\n")
        git = ["git", "-c", "core.autocrlf=false", "-c", "user.name=Dana", "-c", "user.email=dana@example.invalid", "-c", "commit.gpgsign=false"]
        quiet = dict(environment, GIT_CONFIG_NOSYSTEM="1")
        for command in (["init", "-q", "-b", "main"], ["add", "-A"], ["commit", "-q", "-m", "Start"]):
            subprocess.run(git + command, cwd=project, check=True, capture_output=True, env=quiet)
        (project / "README.md").write_bytes(b"# Config parser\n\nReads one value per line.\n")

        def bpr(*arguments: str, stdin: bytes = b"") -> dict:
            result = subprocess.run([str(args.bpr), "cycle", *arguments, "--json", "--profile", "1", "--data-root", str(data)],
                                    input=stdin, capture_output=True, timeout=120, cwd=project, env=environment)
            value = json.loads(result.stdout)
            require(result.returncode == 0 and value["outcome"] == "ok", f"bpr {arguments}: {result.returncode} {value}")
            return value["data"]

        def history() -> str:
            shown = bpr("show", "RC-1")
            exported = subprocess.run([str(args.bpr), "cycle", "export", "RC-1", "--round", "F1", "--output", "-", "--profile", "1", "--data-root", str(data)],
                                      capture_output=True, timeout=120, cwd=project, env=environment)
            require(exported.returncode == 0, f"export F1: {exported.returncode}")
            require([note["item"] for note in shown["notes"]] == ["I1"] and len(shown["rounds"]) == 1, f"show: {sorted(shown)}")
            # Each read-back's own export is recorded as a delivery; nothing else may be.
            deliveries = shown.pop("deliveries")
            require(all(item["channel"] == "terminal_output" and item["round"] == "F1" for item in deliveries), f"deliveries: {deliveries}")
            return json.dumps({"show": shown, "f1_sha256": hashlib.sha256(exported.stdout).hexdigest()}, sort_keys=True)

        results = []
        expected = None
        for index, (name, agents, automation) in enumerate(VARIANTS):
            target = installed / "BetterPullReview.app"
            if target.exists():
                shutil.rmtree(target)
            subprocess.run(["ditto", str(packages[name]), str(target)], check=True)
            mcp = (target / "Contents/MacOS/better_pull_review_mcp_context_helper").exists()
            require(mcp == agents, f"{name}: MCP helper {'missing' if agents else 'present'}")
            results.append(launch(target, environment, root, data, run, automation, f"{index + 1}:{name}"))
            if expected is None:
                opened = bpr("open", str(project))
                require(opened["cycle"] == "RC-1", f"open: {opened}")
                bpr("note", "RC-1", "--file", "src/parser.cpp", "--lines", "3:3", "--body-file", "-", stdin=b"Name the sentinel.\n")
                draft = bpr("review", "RC-1")
                bpr("freeze", "RC-1", "--expected-revision", draft["draft_revision"])
                expected = history()
            else:
                now = history()
                if now != expected:
                    import difflib
                    lines = difflib.unified_diff(json.dumps(json.loads(expected), indent=1, sort_keys=True).splitlines(), json.dumps(json.loads(now), indent=1, sort_keys=True).splitlines(), lineterm="")
                    require(False, f"{name}: the cycle no longer reads back as it was:\n" + "\n".join(lines))
        require(not marks.exists(), f"agent canaries ran: {marks.read_text() if marks.exists() else ''}")
        print(json.dumps({"launches": results, "cycle": json.loads(expected)}, indent=1))
        print("PATH_CANARIES not run: " + ", ".join(AGENT_NAMES))
        print("INSTALLED_CYCLE_PACKAGE_COURSE_OK")
    finally:
        shutil.rmtree(root, ignore_errors=True)


if __name__ == "__main__":
    main()
