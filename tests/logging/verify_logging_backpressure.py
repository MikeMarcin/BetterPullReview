"""A full inherited stderr pipe must not block sink failure or shutdown."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="bpr-log-pipe-") as temporary:
    root = Path(temporary).resolve()
    reader, writer = os.pipe()
    try:
        os.set_blocking(writer, False)
        try:
            while True:
                os.write(writer, b"x" * 4096)
        except BlockingIOError:
            pass
        os.set_blocking(writer, True)
        for scenario in ("full", "unavailable"):
            completed = subprocess.run(
                [sys.argv[1], "--failure-child", str(root), scenario],
                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=writer,
                timeout=5, check=False,
            )
            if completed.returncode != 0:
                raise RuntimeError("bounded logging failure child failed")
    finally:
        os.close(writer)
        os.close(reader)
print("logging-backpressure:pass")
