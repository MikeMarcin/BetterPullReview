"""Competing real desktops hand off or refuse without another registry writer."""
import argparse
import contextlib
import os
from pathlib import Path
import stat
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from bpr_narrow_owner_private import make_root_private  # noqa: E402
from bpr_store_connection import store_connection  # noqa: E402
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


@contextlib.contextmanager
def private_fixture_parent():
    """[[BPR-0849]]. The directory the course's data root is created in.

    The same kind of parent [[BPR-0847]] gave the activation course. An owner
    creates its agent workspace neighbourhood beside its data root, and only
    under a parent this user owns and nobody else can write. Otherwise
    workspace recovery refuses, and the orderly quit exits 1. The macOS
    runner's `BPR_TEST_SOCKET_ROOT` is such a directory. Linux's network
    sandbox sets none, and shared `/tmp` is not one, so on POSIX the course
    makes its own there. `/tmp` keeps the socket path short. On POSIX the
    contained support delegates to `tempfile.mkdtemp`, so the directory is
    still created mode 0700. Windows keeps `None`, the user's own temporary
    folder.
    """
    configured = os.environ.get("BPR_TEST_SOCKET_ROOT")
    if configured is not None:
        yield configured
        return
    if os.name != "posix":
        yield None
        return
    with contained_temporary_directory(prefix="bpr-admission-course-", parent="/tmp") as private:
        yield private


def windows(root, profile):
    with store_connection(root / "state/v1/profile-registry.sqlite3") as connection:
        return connection.execute(
            "SELECT window_id,generation,ordinal FROM profile_registry_windows "
            "WHERE profile_id=? ORDER BY ordinal", (str(profile),)
        ).fetchall()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    args = parser.parse_args()
    with private_fixture_parent() as parent, contained_temporary_directory(prefix="own-", parent=parent) as temporary:
        # [[BPR-0875]]. `os.chmod(root, 0o700)` says "only me" on POSIX. On
        # Windows it only clears the read-only attribute, so the root kept the
        # entries CPython and the parent left on it and the profile registry
        # refused it. The shared helper writes the descriptor the stores define.
        root = Path(temporary).resolve() / "data"
        make_root_private(root)
        if os.name == "posix":
            # The product's own test of the parent, so a shared one fails here by name.
            shared = root.parent.lstat()
            owner_only = (stat.S_ISDIR(shared.st_mode) and shared.st_uid == os.geteuid()
                          and shared.st_mode & (stat.S_IWGRP | stat.S_IWOTH) == 0)
            assert owner_only, f"{root.parent} is not owner-only, so an owner's workspace recovery would refuse"
        processes = []
        logs = []

        def start(mode, profile, name):
            log = (root / f"{name}.log").open("w+")
            logs.append(log)
            process = subprocess.Popen(
                [args.application, mode, str(root), str(profile), str(root / f"{name}.quit")],
                stdout=log, stderr=subprocess.STDOUT, text=True,
            )
            processes.append(process)
            return process, log, name

        def wait_marker(item, marker):
            process, log, _ = item
            deadline = time.monotonic() + 35
            while time.monotonic() < deadline:
                log.seek(0)
                output = log.read()
                if marker in output:
                    return
                if process.poll() is not None:
                    raise AssertionError(f"process exited {process.returncode}: {output}")
                time.sleep(0.02)
            raise AssertionError(f"missing {marker}: {output}")

        def quit_owner(item):
            process, log, name = item
            (root / f"{name}.quit").touch()
            code = process.wait(timeout=35)
            log.seek(0)
            output = log.read()
            assert code == 0 and "OWNER_RELEASED" in output, output

        try:
            subprocess.run([args.application, "seed", str(root), "1", str(root / "unused")], check=True, timeout=30)
            owner = start("owner", 1, "first")
            wait_marker(owner, "OWNER_READY")
            before = windows(root, 1)
            assert len(before) == 1
            contender = start("contender", 1, "contender")
            contender_code = contender[0].wait(timeout=35)
            contender[1].seek(0)
            assert contender_code == 0, contender[1].read()
            wait_marker(contender, "OWNER_HANDOFF_OR_BUSY")
            assert windows(root, 1) == before, "duplicate startup changed owner's windows"
            other = start("other", 2, "other")
            wait_marker(other, "OTHER_REFUSED_DEFAULT")
            wait_marker(other, "OWNER_READY")
            assert windows(root, 1) == before, "Open Profile changed competing owner's windows"
            assert len(windows(root, 2)) == 1
            quit_owner(other)
            quit_owner(owner)
            restarted = start("owner", 1, "restarted")
            wait_marker(restarted, "OWNER_READY")
            restarted[0].kill()
            restarted[0].wait(timeout=10)
            recovered = start("owner", 1, "recovered")
            wait_marker(recovered, "OWNER_READY")
            quit_owner(recovered)
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                process.wait(timeout=10)
            for log in logs:
                log.close()
    print("PROFILE_OWNER_ADMISSION_OK")


if __name__ == "__main__":
    main()
