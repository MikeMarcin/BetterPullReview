"""[[BPR-0644]]. A remembered device grant survives an ordinary relaunch.

An orderly quit and a termination signal are each followed by a relaunch that
must restore the session. These are regression courses: both pass with the
BPR-0644 product change reverted. A last launch whose vault is locked asserts
BPR-0541's fail-closed rule, under which the grant is tombstoned and erased.

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
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from scripts.contained_temporary_support import (  # noqa: E402
    contained_temporary_directory,
    inherits_parent_permissions,
)

TIMEOUT_SECONDS = 120


def binding_rows(root):
    stores = list(root.rglob("device-bindings.sqlite3"))
    if len(stores) != 1:
        raise SystemExit(f"expected one device binding store, found {len(stores)}")
    with sqlite3.connect(f"file:{stores[0]}?mode=ro", uri=True) as connection:
        return connection.execute("SELECT profile,tombstone FROM device_binding").fetchall()


def expect_tombstoned_binding(root, after):
    rows = binding_rows(root)
    if len(rows) != 1 or rows[0][1] != 1:
        raise SystemExit(f"after {after}: the binding row is not tombstoned ({rows and rows[0][1]}, {len(rows)} rows)")


def expect_live_binding(root, after):
    rows = binding_rows(root)
    if len(rows) != 1 or rows[0][1] != 0:
        raise SystemExit(f"after {after}: the binding row is {'tombstoned' if rows and rows[0][1] else 'missing'} ({len(rows)} rows)")


def run(application, phase, root, vault=b"", exports=True):
    result = subprocess.run([application, phase, str(root)], input=vault,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=TIMEOUT_SECONDS, check=False)
    diagnostic = result.stderr.decode("utf-8", errors="replace")
    if result.returncode != 0 or f"device-binding-relaunch:{phase}:pass" not in diagnostic:
        # stdout carries vault material; never report it, including on failure.
        raise SystemExit(f"{phase} failed ({result.returncode})\n{diagnostic}")
    exported = result.stdout.strip()
    if not exports:
        if exported:
            raise SystemExit(f"{phase} exported a vault it should have erased")
        return b""
    if not exported or len(exported) > 2048:
        raise SystemExit(f"{phase} exported no bounded vault")
    return exported


def terminate(application, root, vault):
    # The contained support has no temporary-file helper, so the stderr sink is
    # a real file inside a contained temporary directory. The child still gets
    # an operating-system file handle, and leaving the directory removes it.
    with contained_temporary_directory(prefix="bpr-0644-relaunch-stderr-") as scratch, \
            open(Path(scratch) / "terminate.stderr", "w+b") as diagnostics:
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
    with contained_temporary_directory(prefix="bpr-0644-relaunch-") as temporary:
        root = Path(temporary).resolve() / "application"
        # [[BPR-0876]]. The mode is POSIX's way of saying "only me". On Windows
        # `mkdir(mode=0o700)` stamps CPython's protected owner-only descriptor,
        # which leaves out the AppContainer this course runs in, so the
        # directory it just made is one it can neither narrow with `os.chmod`
        # nor remove afterwards — both refuse with WinError 5. There the
        # directory inherits the private entries of the temporary the support
        # module made, which is the same guarantee expressed the platform's way.
        # `inherits_parent_permissions` is this tree's existing way to ask which
        # of the two applies.
        if inherits_parent_permissions():
            root.mkdir()
        else:
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
        # A relaunch whose vault will not answer without a prompt fails
        # closed under BPR-0541: the grant is tombstoned and erased.
        run(arguments.application, "locked", root, vault, exports=False)
        expect_tombstoned_binding(root, "a relaunch with a locked vault")
    print("DEVICE_BINDING_RELAUNCH_OK")


if __name__ == "__main__":
    main()
