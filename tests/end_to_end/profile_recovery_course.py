"""[[BPR-1602]]. A profile that cannot open, through the shipped entry point.

Runs the real application offscreen on the github-com fixture with a
persistent private profile root (`--epoch-fixture-root`), as an attended look
does, and answers the recovery dialog with `--profile-recovery-answer`:

1. A healthy profile opens and is offered nothing (the negative control).
2. Its comparison-history database is damaged: start-up fails and the dialog
   is offered; Quit leaves every file as it was.
3. Offered again and answered "caches": the caches are cleared before any
   store opens and the pull request opens; sign-in stores, logs and review
   data are the same files.
4. `--reset-profile-data` offers the dialog before anything loads; answered
   "everything", review data is replaced and sign-in stores are kept.
5. `--reset-profile-data-copy` without a scope is refused.

Usage: profile_recovery_course.py APPLICATION SOURCE_ROOT
Prints PROFILE_RECOVERY_COURSE_OK on success. Stops only the processes it
started, by process group.
"""

import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

OPENED = "ATTENDED_PULL_REQUEST_OPENED"


def fail(message, log=""):
    print(f"PROFILE_RECOVERY_COURSE_FAILED: {message}")
    if log:
        print(log[-4000:])
    sys.exit(1)


def private(path):
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(path, 0o700)
    return path


def run(application, source, root, log_path, *extra, wait=90):
    """Starts the application and returns its output once the pull request
    opened or it exited, stopping it if it is still running."""
    environment = dict(os.environ)
    environment["QT_QPA_PLATFORM"] = "offscreen"
    command = [application, "--github-fixture", "github-com", "--epoch-fixture-root", str(root),
               "--epoch-fixture-phase", "single", "--attended-launch", "--attended-open-pull-request", *extra]
    with open(log_path, "w", encoding="utf-8") as log:
        process = subprocess.Popen(command, cwd=source, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=True)
    deadline = time.monotonic() + wait
    while time.monotonic() < deadline:
        text = Path(log_path).read_text(encoding="utf-8", errors="replace")
        if OPENED in text or process.poll() is not None:
            break
        time.sleep(0.25)
    if process.poll() is None:
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
    return Path(log_path).read_text(encoding="utf-8", errors="replace")


def identity(path):
    status = path.stat()
    return (status.st_ino, status.st_size, path.read_bytes())


def damage(path):
    for sidecar in ("-wal", "-shm"):
        candidate = path.with_name(path.name + sidecar)
        if candidate.exists():
            candidate.unlink()
    data = bytearray(path.read_bytes())
    data[: min(4096, len(data))] = b"\x5a" * min(4096, len(data))
    path.write_bytes(bytes(data))


def main():
    if len(sys.argv) != 3:
        fail("usage: profile_recovery_course.py APPLICATION SOURCE_ROOT")
    application, source = sys.argv[1], sys.argv[2]
    scratch = Path(tempfile.mkdtemp(prefix="bpr-profile-recovery-")).resolve()
    try:
        os.chmod(scratch, 0o700)
        root = private(scratch / "profile")
        state = root / "state" / "v1"

        first = run(application, source, root, scratch / "first.log", "--profile-recovery-answer", "caches")
        if OPENED not in first or "PROFILE_RECOVERY_OFFERED" in first:
            fail("a healthy profile opens and is offered nothing", first)

        catalog = state / "epoch-catalog.sqlite3"
        if not catalog.exists():
            fail("the fixture made its comparison-history database")
        damage(catalog)
        damaged = catalog.read_bytes()
        kept = [path for path in (state / "remembered-connections.sqlite3", state / "review-ledger.sqlite3") if path.exists()]
        if not kept:
            fail("the fixture made stores a cache reset keeps")
        before = {path: identity(path) for path in kept}
        logs_before = sorted(path.name for path in (root / "logs").iterdir())

        quit_run = run(application, source, root, scratch / "quit.log", "--profile-recovery-answer", "quit")
        if "PROFILE_RECOVERY_OFFERED:startup-failure" not in quit_run or "PROFILE_RECOVERY_CHOICE:quit" not in quit_run or OPENED in quit_run:
            fail("a damaged profile is offered recovery and Quit opens nothing", quit_run)
        # The failed start may open other stores before it fails; Quit itself
        # removes nothing, so every file is still the same file.
        if catalog.read_bytes() != damaged or any(path.stat().st_ino != before[path][0] for path in kept):
            fail("Quit removes nothing")

        recovered = run(application, source, root, scratch / "recovered.log", "--profile-recovery-answer", "caches")
        if "PROFILE_RECOVERY_OFFERED:startup-failure" not in recovered or "PROFILE_RECOVERY_CHOICE:caches" not in recovered:
            fail("the damaged profile is offered Clear caches", recovered)
        if OPENED not in recovered:
            fail("after clearing caches the pull request opens", recovered)
        if catalog.exists() and catalog.read_bytes() == damaged:
            fail("the damaged catalog was removed")
        for path in kept:
            if path.stat().st_ino != before[path][0]:
                fail(f"Clear caches keeps {path.name}")
        missing = [name for name in logs_before if not (root / "logs" / name).exists()]
        if missing:
            fail(f"Clear caches keeps the logs: {missing}")

        ledger = state / "review-ledger.sqlite3"
        connections = state / "remembered-connections.sqlite3"
        ledger_before = ledger.stat().st_ino if ledger.exists() else None
        connections_before = identity(connections) if connections.exists() else None
        requested = run(application, source, root, scratch / "requested.log", "--reset-profile-data", "--profile-recovery-answer", "everything")
        if "PROFILE_RECOVERY_OFFERED:requested" not in requested or "PROFILE_RECOVERY_CHOICE:everything" not in requested or OPENED not in requested:
            fail("--reset-profile-data offers the dialog and Reset everything opens the profile", requested)
        if ledger_before is not None and ledger.exists() and ledger.stat().st_ino == ledger_before:
            fail("Reset everything replaced the review data")
        if connections_before is not None and (not connections.exists() or identity(connections) != connections_before):
            fail("Reset everything keeps remembered connections")

        refused = run(application, source, root, scratch / "refused.log", "--reset-profile-data-copy", wait=30)
        if "RESET_PROFILE_DATA_CONFIGURATION_FAILED" not in refused or OPENED in refused:
            fail("a copy without a reset is refused", refused)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    print("PROFILE_RECOVERY_COURSE_OK")


if __name__ == "__main__":
    main()
