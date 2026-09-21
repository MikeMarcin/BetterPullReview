#!/usr/bin/env python3
"""Tests-disabled macOS IPC and bounded crash-report canary observations.

Use qualification_guard and a disposable child HOME. No credential or real
repository is opened. Crash canaries are synthetic process environment data;
this does not certify crashes with actual resident provider/source material.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import sqlite3
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from contained_temporary_support import ContainedTemporaryDirectory
import verify_runtime_surface as surface


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def wait_for(predicate, description, seconds=10):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.05)
    raise RuntimeError(description)


def window_count(registry):
    with sqlite3.connect(f"file:{registry}?mode=ro", uri=True) as connection:
        return connection.execute("SELECT COUNT(*) FROM profile_registry_windows").fetchone()[0]


def run(app: Path, scratch: Path, report_root: Path | None) -> dict:
    home = scratch / "home"
    home.mkdir(mode=0o700)
    runtime = scratch / "runtime"
    runtime.mkdir(mode=0o700)
    canaries = ["BPR_CANARY_" + kind + "_" + os.urandom(16).hex() for kind in ("SECRET", "SOURCE", "COMMENT")]
    env = {key: value for key, value in os.environ.items() if not key.startswith(("GH_", "GITHUB_", "QT_", "DYLD_"))}
    env.update(HOME=str(home), CFFIXED_USER_HOME=str(home), TMPDIR=str(runtime), PATH="/usr/bin:/bin:/usr/sbin:/sbin")
    env.update({f"BPR_SECURITY_CANARY_{index}": value for index, value in enumerate(canaries)})
    env = surface.launch_environment(True, env)
    process = subprocess.Popen([str(app)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    endpoint = None
    evidence = {}
    try:
        registry = wait_for(lambda: next(home.rglob("profile-registry.sqlite3"), None), "profile-not-created")
        wait_for(lambda: window_count(registry) == 1, "initial-window-not-created")
        endpoints = wait_for(lambda: list(runtime.glob("bpr-activate-*.sock")), "activation-endpoint-not-created")
        require(len(endpoints) == 1, "ambiguous-profile-endpoint")
        endpoint = endpoints[0]
        surface.check_profile_socket(process.pid, endpoint)
        require(not surface.network_sockets(process.pid) and not surface.child_processes(process.pid), "unexpected-idle-network-or-child")
        frame = bytearray(b"BPRA\x01\x00\x00\x00" + (1).to_bytes(8, "little"))
        for data in (b"GET / HTTP/1.1\r\n\r\n", bytes(frame[:15]), bytes(frame + b"x"),
                     bytes(frame[:4] + b"\x02" + frame[5:]), bytes(frame[:8] + (2).to_bytes(8, "little"))):
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(4)
                client.connect(str(endpoint))
                client.sendall(data)
                client.shutdown(socket.SHUT_WR)
                try:
                    reply = client.recv(8)
                except ConnectionResetError:
                    reply = b""
                require(reply != b"\x01\x01", "hostile-ipc-frame-acknowledged")
        require(window_count(registry) == 1 and process.poll() is None, "hostile-frame-changed-owner")
        sender = subprocess.run([str(app), "--bpr-profile", "1", "--new-window"], env=env,
                                capture_output=True, timeout=15)
        require(sender.returncode == 0, "production-new-window-handoff-failed")
        wait_for(lambda: window_count(registry) == 2, "handoff-did-not-reach-window-policy")
        require(not surface.network_sockets(process.pid), "activation-created-network-endpoint")
        evidence["ipc"] = "private named AF_UNIX; five hostile frames refused; actual second launch creates second window in original owner; no TCP/UDP"
        evidence["crash"] = "not-requested"
        if report_root is not None:
            existing = set(report_root.glob("BetterPullReview*.ips"))
            process.send_signal(signal.SIGABRT)
            process.wait(timeout=10)
            reports = []
            deadline = time.monotonic() + 25
            while time.monotonic() < deadline and not reports:
                for path in set(report_root.glob("BetterPullReview*.ips")) - existing:
                    payload = path.read_bytes()
                    if re.search(rb'"pid"\s*:\s*' + str(process.pid).encode() + rb'\b', payload):
                        require(not any(value.encode() in payload for value in canaries), "crash-canary-leak")
                        reports.append({"sha256": hashlib.sha256(payload).hexdigest(), "bytes": len(payload)})
                if not reports:
                    time.sleep(0.25)
            require(bool(reports), "native-crash-report-unavailable")
            evidence["crash"] = {"reports": reports, "environment_canaries_absent": bool(reports),
                                 "resident_provider_source_course": "pending; environment memory is narrower evidence"}
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            stdout, stderr = process.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            stdout, stderr = process.communicate(timeout=5)
        require(not any(value.encode() in stdout + stderr for value in canaries), "output-canary-leak")
    evidence["executable_sha256"] = hashlib.sha256(app.read_bytes()).hexdigest()
    evidence["child_reaped"] = process.poll() is not None
    evidence["trust_vault_native_matrix"] = "pending"
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--application", required=True, type=Path)
    parser.add_argument("--crash-report-root", type=Path)
    args = parser.parse_args()
    require(sys.platform == "darwin", "course-requires-macos")
    outer = Path(os.environ["QUALIFICATION_TARGET_ROOT"]).resolve(strict=True)
    with ContainedTemporaryDirectory(prefix="security-", parent=outer) as temporary:
        result = run(args.application.resolve(strict=True), Path(temporary), args.crash_report_root)
    print(json.dumps({"result": "RELEASE_SECURITY_COURSE_OK", **result}, sort_keys=True))


if __name__ == "__main__":
    main()
