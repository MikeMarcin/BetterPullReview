"""[[BPR-0644]]. A remembered device grant survives an ordinary relaunch.

An orderly quit, a termination signal, and a launch whose vault is locked are
each followed by a relaunch that must restore the session.

Real processes over one application data root. The synthetic fixture vault
slots travel only through the child's standard input and standard output
pipes; they are never printed, logged, or written to disk by this driver. The
metadata row is read straight from the real SQLite store after every process
has exited.
"""
import argparse
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile

TIMEOUT_SECONDS = 120


def binding_rows(root):
    stores = list(root.rglob("device-bindings.sqlite3"))
    if len(stores) != 1:
        raise SystemExit(f"expected one device binding store, found {len(stores)}")
    with sqlite3.connect(f"file:{stores[0]}?mode=ro", uri=True) as connection:
        return connection.execute("SELECT profile,tombstone FROM device_binding").fetchall()


def expect_live_binding(root, after):
    rows = binding_rows(root)
    if len(rows) != 1 or rows[0][1] != 0:
        raise SystemExit(f"after {after}: the binding row is {'tombstoned' if rows and rows[0][1] else 'missing'} ({len(rows)} rows)")


def run(application, phase, root, vault=b""):
    result = subprocess.run([application, phase, str(root)], input=vault,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=TIMEOUT_SECONDS, check=False)
    diagnostic = result.stderr.decode("utf-8", errors="replace")
    if result.returncode != 0 or f"device-binding-relaunch:{phase}:pass" not in diagnostic:
        # stdout carries vault material; never report it, including on failure.
        raise SystemExit(f"{phase} failed ({result.returncode})\n{diagnostic}")
    exported = result.stdout.strip()
    if not exported or len(exported) > 2048:
        raise SystemExit(f"{phase} exported no bounded vault")
    return exported


def terminate(application, root, vault):
    with tempfile.TemporaryFile() as diagnostics:
        child = subprocess.Popen([application, "terminate", str(root)], stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=diagnostics)
        try:
            child.stdin.write(vault)
            child.stdin.close()
            exported = child.stdout.readline().strip()
            ready = child.stdout.readline().strip()
            if ready != b"device-binding-relaunch:terminate:ready" or not exported or len(exported) > 2048:
                child.kill()
                child.wait(timeout=TIMEOUT_SECONDS)
                diagnostics.seek(0)
                raise SystemExit("terminate phase did not reach readiness\n"
                                 + diagnostics.read().decode("utf-8", errors="replace"))
            # SIGTERM on POSIX, TerminateProcess on Windows: no shutdown runs.
            child.terminate()
            child.wait(timeout=TIMEOUT_SECONDS)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=TIMEOUT_SECONDS)
            child.stdout.close()
    return exported


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="bpr-0644-relaunch-") as temporary:
        root = Path(temporary).resolve() / "application"
        root.mkdir(mode=0o700)
        os.chmod(root, 0o700)
        vault = run(arguments.application, "seed", root)
        expect_live_binding(root, "the seeding lifetime")
        # An orderly quit after a restored session, then a relaunch.
        vault = run(arguments.application, "quit", root, vault)
        expect_live_binding(root, "an orderly quit of a restored session")
        vault = run(arguments.application, "quit", root, vault)
        expect_live_binding(root, "the relaunch after an orderly quit")
        # A termination signal after a restored session, then a relaunch.
        vault = terminate(arguments.application, root, vault)
        expect_live_binding(root, "a terminated restored session")
        vault = run(arguments.application, "verify", root, vault)
        expect_live_binding(root, "the relaunch after termination")
        # A relaunch whose vault will not answer without a prompt, then one
        # that answers: the first must not cost the second its session.
        vault = run(arguments.application, "locked", root, vault)
        expect_live_binding(root, "a relaunch with a locked vault")
        run(arguments.application, "verify", root, vault)
        expect_live_binding(root, "the relaunch after a locked vault")
    print("DEVICE_BINDING_RELAUNCH_OK")


if __name__ == "__main__":
    main()
