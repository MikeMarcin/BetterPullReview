"""Separate native loggers share one bounded profile directory without append races."""
import json
import select
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

with contained_temporary_directory(prefix="bpr-session-log-") as temporary:
    root = Path(temporary).resolve()
    children = []
    try:
        for _ in range(4):
            child = subprocess.Popen(
                [sys.argv[1], "--concurrent-child", str(root)],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                text=True,
            )
            children.append(child)
            if not select.select([child.stdout], [], [], 8)[0] or child.stdout.readline().strip() != "ready":
                raise RuntimeError("concurrent session admission failed")
        initial = list((root / "logs").glob("session-*.jsonl"))
        if len(initial) != 4:
            raise RuntimeError("each active process must own a separate file")
        for child in children:
            child.stdin.write("run\n")
            child.stdin.flush()
        for child in children:
            _, error = child.communicate(timeout=10)
            if child.returncode:
                raise RuntimeError(f"concurrent writer failed: {error}")
        files = list((root / "logs").glob("session-*.jsonl"))
        if len(files) > 8 or sum(file.stat().st_size for file in files) > 8400:
            raise RuntimeError("aggregate or file-count bound violated")
        for file in files:
            if file.stat().st_size > 1400:
                raise RuntimeError("segment bound violated")
            previous = 0
            for line in file.read_text().splitlines():
                record = json.loads(line)
                if record["session"] not in file.name or record["sequence"] <= previous:
                    raise RuntimeError("cross-session append or order violation")
                previous = record["sequence"]
    finally:
        for child in children:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=5)
print("session-concurrency:pass")
