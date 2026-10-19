#!/usr/bin/env python3
"""Exercise the shipped MCP helper's POSIX terminal privacy and backpressure.

Only synthetic names, values and protocol inputs are supplied. Output reports
fixed facts, never child text. Every owned child is reaped and pipe closed.
Windows delivery and external native crash reporting remain separate courses.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import signal

if os.name == "posix":
    import fcntl
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


MAXIMUM_STDERR_BYTES = 512
CONTENT = "BPR_SYNTHETIC_PRIVATE_CONTENT_CANARY"
VALUE = "BPR_SYNTHETIC_CREDENTIAL_VALUE"
REQUEST = b'{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}\n'


def blocked_stderr(helper: Path, environment: dict, request: bytes) -> tuple[dict, bytes]:
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
        original_flags = fcntl.fcntl(write_fd, fcntl.F_GETFL)
        started = time.monotonic()
        child = subprocess.Popen(
            [str(helper)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=write_fd, env=environment, close_fds=True,
        )
        child.stdin.write(request)
        child.stdin.close()
        child.stdin = None
        blocked = False
        try:
            child.wait(timeout=1)
        except subprocess.TimeoutExpired:
            blocked = True
        elapsed = time.monotonic() - started
        same_flags = fcntl.fcntl(write_fd, fcntl.F_GETFL) == original_flags
        # A failing baseline is allowed to finish only after its blockage has
        # been measured; this drain can never turn that observation into a pass.
        remaining = filled
        while remaining:
            remaining -= len(os.read(read_fd, min(remaining, 4096)))
        stdout, _ = child.communicate(timeout=5)
        os.close(write_fd)
        write_fd = -1
        remainder = os.read(read_fd, MAXIMUM_STDERR_BYTES + 1)
        return {
            "pipe_bytes": filled,
            "elapsed_seconds": round(elapsed, 3),
            "blocked": blocked,
            "exit": child.returncode,
            "stdout_bytes": len(stdout),
            "diagnostic_bytes_after_fill": len(remainder),
            "inherited_flags_unchanged": same_flags,
        }, stdout
    finally:
        if child is not None:
            if child.poll() is None:
                child.kill()
            child.communicate(timeout=5)
        os.close(read_fd)
        if write_fd >= 0:
            os.close(write_fd)


def closed_stderr(helper: Path, environment: dict, request: bytes) -> subprocess.CompletedProcess:
    read_fd, write_fd = os.pipe()
    os.close(read_fd)
    try:
        return subprocess.run(
            [str(helper)], input=request, stdout=subprocess.PIPE, stderr=write_fd,
            env=environment, close_fds=True, timeout=5, check=False,
        )
    finally:
        os.close(write_fd)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "posix":
        parser.error("this course requires POSIX pipes; other platforms remain pending")
    helper = args.helper.resolve(strict=True)
    name = CONTENT + "x" * 32768 + "\nmcp-forged-line\r\x1b"
    # CoreFoundation inserts this name at startup on macOS when it is absent.
    # Supply a synthetic value so the helper's complete received count is
    # exactly the environment we constructed on every platform.
    hostile_environment = {name: VALUE, "__CF_USER_TEXT_ENCODING": "0x0:0:0"}
    hostile_environment.update({f"{CONTENT}_{index}": VALUE for index in range(256)})
    observed = subprocess.run(
        [str(helper)], input=b"", stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env=hostile_environment, timeout=5, check=False,
    )
    arguments = subprocess.run(
        [str(helper), CONTENT + "\nmcp-forged-line"], input=b"",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, env={}, timeout=5, check=False,
    )
    blocked_refusal, refusal_output = blocked_stderr(helper, {}, b"")
    closed_refusal = closed_stderr(helper, {}, b"")
    with contained_temporary_directory(prefix="bpr-mcp-terminal-") as scratch:
        pin = "1/2/3/" + "a" * 40 + "/" + "b" * 40 + "/4/5"
        environment = {"BPR_MCP_SNAPSHOT_ROOT": scratch, "BPR_MCP_PINNED_BINDING": pin}
        healthy = subprocess.run(
            [str(helper)], input=REQUEST, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=environment, timeout=5, check=False,
        )
        blocked_protocol, protocol_output = blocked_stderr(helper, environment, REQUEST)
        closed_protocol = closed_stderr(helper, environment, REQUEST)
        ready_private_absent = scratch.encode() not in healthy.stderr and pin.encode() not in healthy.stderr
        read_fd, write_fd = os.pipe()
        os.close(read_fd)
        try:
            closed_stdout = subprocess.run(
                [str(helper)], input=REQUEST, stdout=write_fd, stderr=subprocess.PIPE,
                env=environment, timeout=5, check=False,
            )
        finally:
            os.close(write_fd)
    expected_counts = f"mcp-environment-counts\t{len(hostile_environment)}\t0\t0\n".encode("ascii")
    checks = {
        "refusal_exit_unchanged": observed.returncode == 1 and observed.stdout == b"",
        "complete_received_and_retained_counts": expected_counts in observed.stderr,
        "bounded_record": 0 < len(observed.stderr) <= MAXIMUM_STDERR_BYTES,
        "name_content_absent": CONTENT.encode() not in observed.stderr,
        "value_absent": VALUE.encode() not in observed.stderr,
        "line_injection_absent": b"mcp-forged-line" not in observed.stderr,
        "argument_content_absent": CONTENT.encode() not in arguments.stderr and arguments.stdout == b"",
        "argument_refusal_useful": b"mcp-startup-refusal\tunexpected_argument\t1\n" in arguments.stderr and arguments.returncode == 1,
        "missing_environment_useful": b"mcp-startup-refusal\tmissing_environment\n" in observed.stderr,
        "full_pipe_refusal_exits": not blocked_refusal["blocked"] and blocked_refusal["exit"] == 1 and refusal_output == b"",
        "full_pipe_protocol_exits": not blocked_protocol["blocked"] and blocked_protocol["exit"] == 0,
        "full_pipe_protocol_unchanged": protocol_output == healthy.stdout,
        "closed_pipe_refusal_unchanged": closed_refusal.returncode == 1 and closed_refusal.stdout == b"",
        "closed_pipe_protocol_unchanged": closed_protocol.returncode == 0 and closed_protocol.stdout == healthy.stdout,
        "inherited_flags_unchanged": blocked_refusal["inherited_flags_unchanged"] and blocked_protocol["inherited_flags_unchanged"],
        "stalled_records_dropped": blocked_refusal["diagnostic_bytes_after_fill"] == 0 and blocked_protocol["diagnostic_bytes_after_fill"] == 0,
        "healthy_protocol_valid": healthy.returncode == 0 and json.loads(healthy.stdout).get("id") == 1 and len(json.loads(healthy.stdout)["result"]["tools"]) == 4,
        "healthy_record_bounded": 0 < len(healthy.stderr) <= MAXIMUM_STDERR_BYTES,
        "ready_contains_no_path_or_binding": ready_private_absent,
        "stdout_signal_disposition_restored": closed_stdout.returncode == -signal.SIGPIPE,
    }
    result = {
        "schema": "bpr.terminal-diagnostic-audit.v2",
        "helper_sha256": hashlib.sha256(helper.read_bytes()).hexdigest(),
        "checks": checks,
        "stderr_bytes": len(observed.stderr),
        "healthy_stderr_bytes": len(healthy.stderr),
        "blocked_refusal": blocked_refusal,
        "blocked_protocol": blocked_protocol,
    }
    print(json.dumps(result, sort_keys=True))
    return int(not all(checks.values()))


if __name__ == "__main__":
    raise SystemExit(main())
