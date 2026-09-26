"""Run owning public host/adapter courses with content canaries and capture both terminals."""
from pathlib import Path
import subprocess
import sys
import time

for binary in sys.argv[1:]:
    start = time.monotonic()
    result = subprocess.run([binary], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=120, check=False)
    assert result.returncode == 0, (Path(binary).name, result.returncode, result.stdout[-1024:], result.stderr[-1024:])
    assert b"BPR_DIAGNOSTIC_PRIVATE_CANARY" not in result.stdout + result.stderr, Path(binary).name
    print(f"diagnostic-subsystem={Path(binary).name} terminal_bytes={len(result.stdout) + len(result.stderr)} elapsed={time.monotonic() - start:.3f}")
print("diagnostic-subsystem-contract:pass")
