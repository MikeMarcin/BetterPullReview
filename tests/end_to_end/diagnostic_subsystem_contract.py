"""Run owning public host/adapter courses with content canaries and capture both terminals."""
from pathlib import Path
import argparse
import subprocess
import sys
import time


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--course", nargs=2, action="append", required=True,
                        metavar=("TIMEOUT_SECONDS", "BINARY"))
    options = parser.parse_args(arguments)
    courses = []
    for seconds, binary in options.course:
        try:
            timeout = int(seconds)
        except ValueError:
            parser.error("course timeout must be a positive integer")
        if timeout <= 0:
            parser.error("course timeout must be a positive integer")
        courses.append((timeout, binary))
    for timeout, binary in courses:
        start = time.monotonic()
        try:
            result = subprocess.run([binary], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=timeout, check=False)
        except subprocess.TimeoutExpired as error:
            # Do not echo partial terminals: the child did not reach the
            # canary check, so only its structural failure is safe to print.
            raise AssertionError(
                f"{Path(binary).name}: PROCESS_TIMEOUT after {timeout}s") from error
        assert result.returncode == 0, (Path(binary).name, result.returncode, result.stdout[-1024:], result.stderr[-1024:])
        assert b"BPR_DIAGNOSTIC_PRIVATE_CANARY" not in result.stdout + result.stderr, Path(binary).name
        print(f"diagnostic-subsystem={Path(binary).name} terminal_bytes={len(result.stdout) + len(result.stderr)} elapsed={time.monotonic() - start:.3f} timeout={timeout}")
    print("diagnostic-subsystem-contract:pass")


if __name__ == "__main__":
    main(sys.argv[1:])
