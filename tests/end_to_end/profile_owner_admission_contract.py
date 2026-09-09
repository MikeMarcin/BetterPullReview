"""Competing real desktops hand off or refuse without another registry writer."""
import argparse
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile
import time


def windows(root, profile):
    with sqlite3.connect(root / "state/v1/profile-registry.sqlite3") as connection:
        return connection.execute(
            "SELECT window_id,generation,ordinal FROM profile_registry_windows "
            "WHERE profile_id=? ORDER BY ordinal", (str(profile),)
        ).fetchall()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--application", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="own-", dir=os.environ.get("BPR_TEST_SOCKET_ROOT", "/tmp" if os.name == "posix" else None)) as temporary:
        root = Path(temporary).resolve()
        os.chmod(root, 0o700)
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
