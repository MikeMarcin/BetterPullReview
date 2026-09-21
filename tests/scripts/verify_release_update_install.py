#!/usr/bin/env python3
"""Qualify signed packages only inside an explicitly guarded disposable root."""

import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import plistlib
import subprocess
import time


def wait_for(condition, description, seconds=120):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = condition()
        if value:
            return value
        time.sleep(0.1)
    raise RuntimeError("Timed out: " + description)


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def bundle_version(bundle):
    with (bundle / "Contents/Info.plist").open("rb") as source:
        return plistlib.load(source)["CFBundleShortVersionString"]


def sandbox_arguments(root):
    live_data = Path.home() / "Library/Application Support/BetterPullReview"
    profile = root / "native.sb"
    rules = [
        "(version 1)",
        "(allow default)",
        "(deny network*)",
        "(deny file-write*)",
        f'(allow file-write* (subpath {json.dumps(str(root))}) (literal "/dev/null"))',
        f"(deny file-read* (subpath {json.dumps(str(live_data))}))",
    ]
    profile.write_text("\n".join(rules) + "\n")
    return ["/usr/bin/sandbox-exec", "-f", str(profile)]


def processes(executable):
    rows = subprocess.check_output(["/bin/ps", "-axo", "pid=,command="], text=True)
    result = []
    for row in rows.splitlines():
        pid, _, command = row.strip().partition(" ")
        if command == str(executable) or command.startswith(str(executable) + " "):
            result.append(int(pid))
    return result


def held_participants(control):
    count = 0
    for path in control.glob("process-*"):
        with path.open("rb") as handle:
            try:
                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--target", required=True, type=Path)
    parser.add_argument("--public-driver", required=True, type=Path)
    parser.add_argument("--transaction-driver", required=True, type=Path)
    parser.add_argument("--scenario", choices=("success", "timeout", "missing-receipt", "rollback"), required=True)
    parser.add_argument("--observe-ui", action="store_true", help="Pause for agent UI observations; no product test hook.")
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    if str(root) != os.environ.get("QUALIFICATION_TARGET_ROOT") or any(root.iterdir()):
        raise RuntimeError("An empty, exact guarded root is required")
    for path in (args.baseline, args.target, args.public_driver, args.transaction_driver):
        if not path.is_absolute() or not path.is_file():
            raise RuntimeError("Every artifact/executable path must be absolute and exist")
    root.chmod(0o700)
    home = root / "home"
    state = home / "Library/Application Support/BetterPullReview/BetterPullReview"
    state.mkdir(parents=True, mode=0o700)
    directories = (home, home / "Library", home / "Library/Application Support",
                   state.parent, state, root / "tmp", root / "installed", root / "staged")
    for directory in directories:
        directory.mkdir(exist_ok=True)
        directory.chmod(0o700)
    for archive, target in ((args.baseline, root / "installed"), (args.target, root / "staged")):
        subprocess.run(["/usr/bin/ditto", "-x", "-k", str(archive), str(target)], check=True)
    installed = root / "installed/BetterPullReview.app"
    executable = installed / "Contents/MacOS/BetterPullReview"
    helper = installed / "Contents/MacOS/bpr_update_helper"
    old_version = bundle_version(installed)
    new_version = bundle_version(root / "staged/BetterPullReview.app")
    control = installed.parent / (".bpr-update-" + hashlib.sha256(str(installed).encode()).hexdigest()[:24])
    environment = {
        "PATH": "/usr/bin:/bin:/usr/sbin:/sbin",
        "CFFIXED_USER_HOME": str(home),
        "TMPDIR": str(root / "tmp"),
        "QUALIFICATION_TARGET_ROOT": str(root),
    }
    driver_environment = dict(environment, QT_QPA_PLATFORM="offscreen")
    sandbox = sandbox_arguments(root)
    logs = []
    owned = []

    def launch(arguments, name, native=False, stdin=None):
        log = (root / (name + ".log")).open("wb")
        logs.append(log)
        child = subprocess.Popen(
            sandbox + [str(value) for value in arguments],
            env=environment if native else driver_environment,
            cwd=root, stdin=stdin, stdout=log, stderr=subprocess.STDOUT,
        )
        owned.append(child)
        return child

    def run(arguments, name, native=False, expected=0, timeout=120):
        child = launch(arguments, name, native)
        code = child.wait(timeout=timeout)
        if code != expected:
            raise RuntimeError(f"{name} returned {code}, expected {expected}; inspect {root / (name + '.log')}")

    summary = {"scenario": args.scenario, "root": str(root), "baseline_version": old_version, "target_version": new_version,
               "baseline_sha256": digest(args.baseline), "target_sha256": digest(args.target), "baseline_executable_sha256": digest(executable),
               "target_executable_sha256": digest(root / "staged/BetterPullReview.app/Contents/MacOS/BetterPullReview")}
    try:
        run([args.public_driver, "--native-state", root, state, "seed"], "state-seed")
        if args.scenario == "rollback":
            run([args.transaction_driver, "--native-prepare", root, args.target, new_version, state, "after-old-rename"], "prepare")
            journal = json.loads((control / "active.json").read_text())
            transaction = control / ("transaction-" + journal["transaction"])
            if installed.exists() or not (transaction / "backup.app").is_dir():
                raise RuntimeError("Expected exact interrupted old rename")
            recovery_helper = transaction / "backup.app/Contents/MacOS/bpr_update_helper"
            run([recovery_helper, "--recover", installed, journal["transaction"]], "native-recover", native=True)
            if digest(executable) != summary["baseline_executable_sha256"]:
                raise RuntimeError("Native rollback did not restore exact old executable")
        else:
            old = launch([executable], "old-application", native=True)
            wait_for(lambda: held_participants(control) == 1 or old.poll() is not None, "signed baseline startup", 40)
            if old.poll() is not None:
                raise RuntimeError("Signed baseline refused startup")
            time.sleep(3)
            if old.poll() is not None:
                raise RuntimeError("Signed baseline exited before state-store readiness")
            print(json.dumps({"phase": "baseline-running", "pid": old.pid, "application": str(installed), "state": str(state)}), flush=True)
            if args.observe_ui:
                wait_for(lambda: (root / "continue-install").is_file(), "external baseline UI observation", 600)
            peer = None
            if args.scenario == "timeout":
                peer = launch([args.transaction_driver, "--peer", installed], "uncooperative-peer", stdin=subprocess.PIPE)
                wait_for(lambda: held_participants(control) == 2, "actual peer joins installation")
            recovery_database = state / "state/v1/update-draft-recovery.sqlite3"
            if args.scenario == "missing-receipt":
                # Obstruct a required new-startup store only after the old process
                # has durably drained; the supervisor waits for its actual exit.
                run([args.transaction_driver, "--native-prepare", root, args.target, new_version, state, "prepared"], "prepare")
                old.wait(timeout=40)
                recovery_database.rename(state / "saved-recovery.sqlite3")
                recovery_database.mkdir(mode=0o700)
                journal = json.loads((control / "active.json").read_text())
                started = time.monotonic()
                run([helper, "--install", installed, journal["transaction"]], "missing-receipt-helper", native=True, expected=1, timeout=140)
                summary["helper_seconds"] = round(time.monotonic() - started, 3)
                transaction = control / ("transaction-" + journal["transaction"])
                if (transaction / "receipt.json").exists() or digest(executable) != summary["baseline_executable_sha256"]:
                    raise RuntimeError("Missing receipt did not restore the exact old application")
                recovery_database.rmdir()
                (state / "saved-recovery.sqlite3").rename(recovery_database)
            else:
                started = time.monotonic()
                expected = "installed" if args.scenario == "success" else "recovered"
                run([args.public_driver, "--native-handoff", root, args.target, old_version, new_version, state, expected], "public-handoff", timeout=360)
                summary["handoff_seconds"] = round(time.monotonic() - started, 3)
                old.wait(timeout=15)
                if old.returncode != 0:
                    raise RuntimeError("Old signed application did not drain successfully")
                if peer:
                    if peer.poll() is not None:
                        raise RuntimeError("Helper must leave an uncooperative participant running")
                    peer.stdin.write(b"unclean\n")
                    peer.stdin.flush()
                    peer.wait(timeout=10)
                    if digest(executable) != summary["baseline_executable_sha256"] or summary["handoff_seconds"] < 30:
                        raise RuntimeError("Native timeout must preserve the old executable for the full wait")
                else:
                    journal = json.loads((control / "last-result.json").read_text())
                    transaction = control / ("transaction-" + journal["transaction"])
                    receipt = json.loads((transaction / "receipt.json").read_text())
                    if receipt["transaction"] != journal["transaction"] or receipt["executable"] != summary["target_executable_sha256"] or receipt["version"] != "v" + new_version or (transaction / "backup.app").exists():
                        raise RuntimeError("Exact native startup receipt/cleanup mismatch")
                    summary["receipt"] = receipt
                    print(json.dumps({"phase": "replacement-running", "pids": processes(executable), "application": str(installed), "receipt": receipt}), flush=True)
                    if args.observe_ui:
                        wait_for(lambda: (root / "continue-cleanup").is_file(), "external replacement UI observation", 600)
        run([args.public_driver, "--native-state", root, state, "check"], "state-retained")
        summary["result"] = json.loads((control / "last-result.json").read_text())
        summary["passed"] = True
    finally:
        if processes(executable):
            print(json.dumps({"phase": "quit-through-native-ui", "pids": processes(executable),
                              "application": str(installed)}), flush=True)
        wait_for(lambda: not processes(executable), "task-owned native applications to quit through their UI", 600)
        for child in owned:
            if child.poll() is None:
                if child.stdin:
                    child.stdin.write(b"unclean\n")
                    child.stdin.flush()
                child.wait(timeout=180)
        for log in logs:
            log.close()
        summary["applications_quiescent"] = not processes(executable)
        (root / "result.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({"scenario": args.scenario, "passed": summary.get("passed", False), "record": str(root / "result.json")}), flush=True)


if __name__ == "__main__":
    main()
