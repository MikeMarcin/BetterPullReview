"""Kill a live native logger, then admit a fresh session without touching its tail."""
import json
from pathlib import Path
import queue
import subprocess
import sys
import threading

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def wait_line(child, expected):
    lines = queue.Queue()
    reader = threading.Thread(target=lambda: lines.put(child.stdout.readline()), daemon=True)
    reader.start()
    if lines.get(timeout=10).strip() != expected:
        raise RuntimeError("native recovery child did not reach " + expected)
    reader.join(timeout=1)


def start_child(executable, root):
    return subprocess.Popen(
        [executable, "--recovery-child", str(root)],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )


def stop_child(child):
    if child.poll() is None:
        child.kill()
    child.wait(timeout=5)


def crash_recovery(executable):
    with contained_temporary_directory(prefix="bpr-log-crash-") as temporary:
        root = Path(temporary).resolve() / "data"
        child = start_child(executable, root)
        try:
            wait_line(child, "flushed")
            before = {file: file.read_bytes() for file in (root / "logs").glob("session-*.jsonl")}
            if len(before) != 1 or not all(before.values()):
                raise RuntimeError("flushed pre-crash record missing")
            child.kill()
            child.wait(timeout=5)
            if child.returncode == 0:
                raise RuntimeError("logger was not abruptly terminated")
        finally:
            stop_child(child)
        for file, content in before.items():
            if file.read_bytes() != content:
                raise RuntimeError("unexpected post-termination file mutation")
            records = [json.loads(line) for line in content.splitlines()]
            if any(record["outcome"] == "shutdown" for record in records):
                raise RuntimeError("abrupt termination invented a shutdown event")
        child = start_child(executable, root)
        try:
            wait_line(child, "flushed")
            child.communicate("finish\n", timeout=10)
            if child.returncode:
                raise RuntimeError("restart failed to release native session ownership")
        finally:
            stop_child(child)
        if len(list((root / "logs").glob("session-*.jsonl"))) != 2:
            raise RuntimeError("restart did not create its own session")
        if any(file.read_bytes() != content for file, content in before.items()):
            raise RuntimeError("restart appended into the terminated session")
    print("logging-crash-recovery:pass (flushed records only; no unflushed durability claim)")


if __name__ == "__main__":
    crash_recovery(sys.argv[1])
