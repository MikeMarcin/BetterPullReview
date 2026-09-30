"""Exercise real ENOSPC in an owned 32 MiB macOS image, never the host volume."""
import errno
import os
from pathlib import Path
import platform
import subprocess
import sys

from verify_logging_recovery import start_child, stop_child, wait_line

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def disk_full(executable):
    if platform.system() != "Darwin":
        raise RuntimeError("this native disk-image course requires macOS")
    with contained_temporary_directory(prefix="bpr-log-full-") as temporary:
        scratch = Path(temporary).resolve()
        image = scratch / "volume.dmg"
        mount = scratch / "mount"
        mount.mkdir()
        subprocess.run(["hdiutil", "create", "-size", "32m", "-fs", "HFS+",
                        "-volname", "BPR diagnostic test", str(image)], check=True, capture_output=True)
        subprocess.run(["hdiutil", "attach", "-nobrowse", "-owners", "on", "-mountpoint",
                        str(mount), str(image)], check=True, capture_output=True)
        try:
            if mount.stat().st_dev == scratch.stat().st_dev:
                raise RuntimeError("refusing to fill a directory on the host volume")
            volume = os.statvfs(mount)
            if volume.f_blocks * volume.f_frsize > 32 * 1024 * 1024:
                raise RuntimeError("refusing an unexpectedly large mounted volume")
            root = mount / "data"
            child = start_child(executable, root)
            try:
                wait_line(child, "flushed")
                old_files = list((root / "logs").glob("session-*.jsonl"))
                written = 0
                with (mount / "filler").open("xb", buffering=0) as filler:
                    try:
                        while written <= 32 * 1024 * 1024:
                            written += filler.write(b"x" * 4096)
                    except OSError as error:
                        if error.errno != errno.ENOSPC:
                            raise
                    else:
                        raise RuntimeError("mounted volume did not reach ENOSPC within its bound")
                child.stdin.write("failure\n")
                child.stdin.flush()
                wait_line(child, "unavailable")
                child.wait(timeout=10)
                if child.returncode:
                    raise RuntimeError("native logger did not report disk failure")
            finally:
                stop_child(child)
            before = {file: file.read_bytes() for file in old_files}
            (mount / "filler").unlink()
            child = start_child(executable, root)
            try:
                wait_line(child, "flushed")
                child.communicate("finish\n", timeout=10)
                if child.returncode:
                    raise RuntimeError("logger did not recover after space was freed")
            finally:
                stop_child(child)
            if any(file.read_bytes() != content for file, content in before.items()):
                raise RuntimeError("recovery altered the failed session")
            if len(list((root / "logs").glob("session-*.jsonl"))) != 2:
                raise RuntimeError("recovery did not create a separate session")
            print(f"logging-disk-full:pass errno=ENOSPC isolated_volume=32MiB filler_bytes={written}")
        finally:
            subprocess.run(["hdiutil", "detach", str(mount)], check=True, capture_output=True)


if __name__ == "__main__":
    disk_full(str(Path(sys.argv[1]).resolve()))
