"""Real desktop activation preserves owner identity and durable window routing."""
import argparse
import contextlib
import os
from pathlib import Path
import sqlite3
import stat
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


@contextlib.contextmanager
def private_fixture_parent():
    """[[BPR-0847]]. The directory the course's data root is created in.

    An owner creates its agent workspace neighbourhood beside its data root, and
    only under a parent this user owns and nobody else can write. Otherwise
    workspace recovery refuses, and the orderly drain exits 1. The macOS runner's
    `BPR_TEST_SOCKET_ROOT` is such a directory. Linux's network sandbox sets
    none, and shared `/tmp` is not one, so the course makes its own there. `/tmp`
    keeps the socket path short. On POSIX the contained support delegates to
    `tempfile.mkdtemp`, so the directory is still created mode 0700.
    """
    configured = os.environ.get("BPR_TEST_SOCKET_ROOT")
    if configured is not None:
        yield configured
        return
    with contained_temporary_directory(prefix="bpr-activation-course-", parent="/tmp") as private:
        yield private


def windows(root, profile):
    with sqlite3.connect(root / "state/v1/profile-registry.sqlite3") as connection:
        return connection.execute(
            "SELECT window_id,generation,ordinal FROM profile_registry_windows "
            "WHERE profile_id=? ORDER BY ordinal", (str(profile),)
        ).fetchall()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    parser.add_argument("--desktop-application", required=True)
    args = parser.parse_args()
    malformed = [
        ["--bpr-profile"], ["--bpr-profile", "0"],
        ["--bpr-profile", "-1"], ["--bpr-profile", "+1"],
        ["--bpr-profile", "1x"], ["--bpr-profile", " 1"],
        ["--bpr-profile", "18446744073709551616"],
        ["--bpr-profile", "1", "--bpr-profile", "2"],
        ["--new-window", "--new-window"], ["--new-window=yes"],
        ["--bpr-profile=1"], ["--bpr-profile", "--new-window"],
    ]
    for arguments in malformed:
        result = subprocess.run([args.desktop_application, *arguments],
                                capture_output=True, text=True, timeout=15)
        assert result.returncode != 0 and "PROFILE_LAUNCH_ARGUMENTS_INVALID" in result.stdout, result
    for identity in ("1", "18446744073709551615"):
        result = subprocess.run([args.desktop_application, "--bpr-profile", identity,
                                 "--new-window", "--application-icon-probe"],
                                capture_output=True, text=True, timeout=15)
        assert result.returncode == 0 and "APPLICATION_ICON_OK" in result.stdout, result
    with private_fixture_parent() as parent, contained_temporary_directory(prefix="act-", parent=parent) as temporary:
        root = Path(temporary).resolve()
        os.chmod(root, 0o700)
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
            sender = start("activate", 1, "activate")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_QUEUED")
            assert windows(root, 1) == before, "ordinary activation changed durable windows"
            sender = start("new", 1, "new")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_QUEUED")
            deadline = time.monotonic() + 30
            while len(windows(root, 1)) != 2 and time.monotonic() < deadline:
                time.sleep(0.02)
            assert len(windows(root, 1)) == 2, "owner did not admit new window"
            (root / "first.quit.focus").touch()
            wait_marker(owner, "MRU_FOCUSED")
            before = windows(root, 1)
            sender = start("activate", 1, "mru")
            assert sender[0].wait(timeout=35) == 0
            (root / "first.quit.check").touch()
            wait_marker(owner, "MRU_ACTIVATED")
            assert windows(root, 1) == before, "MRU activation changed windows"
            other = start("owner", 2, "other")
            wait_marker(other, "OWNER_READY")
            other_before = windows(root, 2)
            sender = start("activate", 2, "other-activate")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_QUEUED")
            assert windows(root, 2) == other_before
            assert windows(root, 1) == before, "other profile activation crossed identity"
            endpoints = list(root.rglob("activate.sock"))
            assert endpoints, "owner activation endpoint was not created"
            # Only remove the fixture socket for the default owner's root.
            endpoint = next(path for path in endpoints if "profile-2" not in path.parts)
            endpoint.unlink()
            sender = start("unavailable", 1, "unavailable")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_UNAVAILABLE")
            assert owner[0].poll() is None and windows(root, 1) == before
            quit_owner(other)
            quit_owner(owner)
            restarted = start("owner", 1, "restarted")
            wait_marker(restarted, "OWNER_READY")
            restarted[0].kill()
            restarted[0].wait(timeout=10)
            recovered = start("owner", 1, "recovered")
            wait_marker(recovered, "OWNER_READY")
            quit_owner(recovered)
            survivor = start("owner", 1, "survivor")
            wait_marker(survivor, "OWNER_READY")
            survivor_before = windows(root, 1)
            closing = start("closing", 2, "closing")
            wait_marker(closing, "CLOSING_READY")
            closing_before = windows(root, 2)
            sender = start("refused", 2, "closing-sender")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_REFUSED")
            assert windows(root, 2) == closing_before
            sender = start("activate", 1, "survivor-activate")
            assert sender[0].wait(timeout=35) == 0
            wait_marker(sender, "ACTIVATION_QUEUED")
            assert windows(root, 1) == survivor_before
            quit_owner(closing)
            quit_owner(survivor)
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                process.wait(timeout=10)
            for log in logs:
                log.close()
    print("PROFILE_ACTIVATION_OK")


if __name__ == "__main__":
    main()
