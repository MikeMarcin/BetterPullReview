#!/usr/bin/env python3
"""Opt-in BPR-0971 counterexample probe; not an expected-passing CTest row.

Only synthetic environment names/values are supplied. Output reports fixed facts,
never child text. A full pipe is always drained or its exact child is reaped.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def blocked_stderr(helper: Path) -> dict:
    read_fd, write_fd = os.pipe()
    child = None
    try:
        os.set_blocking(write_fd, False)
        filled = 0
        while True:
            try:
                filled += os.write(write_fd, b"x" * 4096)
            except BlockingIOError:
                break
        os.set_blocking(write_fd, True)
        started = time.monotonic()
        child = subprocess.Popen(
            [str(helper)], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
            stderr=write_fd, env={}, close_fds=True,
        )
        os.close(write_fd)
        write_fd = -1
        blocked = False
        try:
            child.wait(timeout=1)
        except subprocess.TimeoutExpired:
            blocked = True
        elapsed = time.monotonic() - started
        # Drain only this test's fill. The small refusal now fits, so the child
        # can finish normally rather than needing a kill to end its write.
        remaining = filled
        while remaining:
            remaining -= len(os.read(read_fd, min(remaining, 4096)))
        stdout, _ = child.communicate(timeout=5)
        return {
            "pipe_bytes": filled,
            "blocked_after_seconds": round(elapsed, 3),
            "blocked": blocked,
            "exit_after_drain": child.returncode,
            "stdout_bytes": len(stdout),
        }
    finally:
        if child is not None:
            if child.poll() is None:
                child.kill()
            child.communicate(timeout=5)
        os.close(read_fd)
        if write_fd >= 0:
            os.close(write_fd)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "posix":
        parser.error("this observation requires a POSIX pipe; other platforms remain pending")
    helper = args.helper.resolve(strict=True)
    content = "BPR_SYNTHETIC_PRIVATE_CONTENT_CANARY"
    value = "BPR_SYNTHETIC_CREDENTIAL_VALUE"
    name = content + "x" * 32768 + "\nmcp-forged-line"
    observed = subprocess.run(
        [str(helper)], input=b"", stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env={name: value}, timeout=5, check=False,
    )
    blocked = blocked_stderr(helper)
    result = {
        "schema": "bpr.terminal-diagnostic-audit.v1",
        "helper_sha256": hashlib.sha256(helper.read_bytes()).hexdigest(),
        "exit": observed.returncode,
        "stderr_bytes": len(observed.stderr),
        "name_content_retained": content.encode() in observed.stderr,
        "value_retained": value.encode() in observed.stderr,
        "injected_line_retained": b"\nmcp-forged-line\n" in observed.stderr,
        "blocked_pipe": blocked,
    }
    print(json.dumps(result, sort_keys=True))
    return int(result["name_content_retained"] or result["injected_line_retained"] or blocked["blocked"])


if __name__ == "__main__":
    raise SystemExit(main())
