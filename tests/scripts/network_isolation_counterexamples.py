#!/usr/bin/env python3
"""Falsifiable process-boundary probes for the automated-test network lane."""

from __future__ import annotations

import errno
import os
import pathlib
import socket
import subprocess
import sys
import tempfile
import threading


def external_attempt() -> int:
    candidate = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    candidate.settimeout(2.0)
    result = candidate.connect_ex(("1.1.1.1", 443))
    candidate.close()
    if result not in (errno.EPERM, errno.EACCES):
        print(f"NETWORK_ISOLATION_EXTERNAL_NOT_DENIED:{result}", flush=True)
        return 1
    print(f"NETWORK_ISOLATION_EXTERNAL_DENIED:{result}", flush=True)
    return 0


def fixture_ports(required: int) -> list[int]:
    raw = os.environ.get("BPR_TEST_LOOPBACK_PORTS", "")
    if not raw:
        return [0] * required
    try:
        ports = [int(value) for value in raw.split(",") if value]
    except ValueError:
        return []
    if len(ports) < required or any(port < 1 or port > 65535 for port in ports):
        return []
    return ports


def fixture_token() -> bytes:
    return os.environ.get("BPR_TEST_SOCKET_TOKEN", "bpr-loopback").encode("ascii")


def one_loopback(
    family: socket.AddressFamily,
    address: str,
    port: int,
) -> bool:
    server = socket.socket(family, socket.SOCK_STREAM)
    server.bind((address, port))
    server.listen(1)
    selected_port = server.getsockname()[1]
    token = fixture_token()

    def serve() -> None:
        connection, _ = server.accept()
        with connection:
            connection.sendall(token)

    worker = threading.Thread(target=serve)
    worker.start()
    with socket.create_connection((address, selected_port), timeout=2.0) as client:
        received = client.recv(len(token) + 1)
    worker.join(timeout=2.0)
    server.close()
    return received == token and not worker.is_alive()


def unix_loopback() -> bool:
    configured_root = os.environ.get("BPR_TEST_SOCKET_ROOT")
    context = (
        tempfile.TemporaryDirectory(dir=configured_root)
        if configured_root
        else tempfile.TemporaryDirectory()
    )
    with context as directory:
        path = pathlib.Path(directory) / "fixture.sock"
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(str(path))
        server.listen(1)
        token = fixture_token()

        def serve() -> None:
            connection, _ = server.accept()
            with connection:
                connection.sendall(token)

        worker = threading.Thread(target=serve)
        worker.start()
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.connect(str(path))
        received = client.recv(len(token) + 1)
        client.close()
        worker.join(timeout=2.0)
        server.close()
        return received == token and not worker.is_alive()


def loopback_attempt() -> int:
    ports = fixture_ports(1)
    if len(ports) != 1:
        print("NETWORK_ISOLATION_LOOPBACK_FAILED:ports", flush=True)
        return 1
    ipv6_available = socket.has_ipv6
    ipv4_ok = one_loopback(socket.AF_INET, "127.0.0.1", ports[0])
    ipv6_ok = not ipv6_available or one_loopback(socket.AF_INET6, "::1", ports[0])
    unix_ok = unix_loopback()
    if not (ipv4_ok and ipv6_ok and unix_ok):
        print(
            f"NETWORK_ISOLATION_LOOPBACK_FAILED:{ipv4_ok}:{ipv6_ok}:{unix_ok}",
            flush=True,
        )
        return 1
    print("NETWORK_ISOLATION_LOOPBACK_OK", flush=True)
    return 0


def denied_connect(family: socket.AddressFamily, endpoint: object) -> bool:
    candidate = socket.socket(family, socket.SOCK_STREAM)
    candidate.settimeout(2.0)
    try:
        result = candidate.connect_ex(endpoint)
    finally:
        candidate.close()
    return result in (errno.EPERM, errno.EACCES)


def unrelated_attempt() -> int:
    port = os.environ.get("BPR_UNRELATED_LOOPBACK_PORT", "")
    unix_path = os.environ.get("BPR_UNRELATED_UNIX_SOCKET", "")
    if not port.isdigit() or not unix_path:
        print("NETWORK_ISOLATION_UNRELATED_FAILED:missing-endpoints", flush=True)
        return 1
    allowed_ok = loopback_attempt() == 0
    tcp_denied = denied_connect(socket.AF_INET, ("127.0.0.1", int(port)))
    unix_denied = denied_connect(socket.AF_UNIX, unix_path)
    if not (allowed_ok and tcp_denied and unix_denied):
        print(
            "NETWORK_ISOLATION_UNRELATED_FAILED:"
            f"{allowed_ok}:{tcp_denied}:{unix_denied}",
            flush=True,
        )
        return 1
    print("NETWORK_ISOLATION_UNRELATED_DENIED", flush=True)
    return 0


def child_attempt() -> int:
    child = subprocess.run(
        [sys.executable, __file__, "external"],
        capture_output=True,
        text=True,
        check=False,
        timeout=10,
    )
    if child.returncode != 0 or "NETWORK_ISOLATION_EXTERNAL_DENIED" not in child.stdout:
        print("NETWORK_ISOLATION_CHILD_NOT_DENIED", flush=True)
        print(child.stdout, end="")
        print(child.stderr, end="", file=sys.stderr)
        return 1
    print("NETWORK_ISOLATION_CHILD_DENIED", flush=True)
    return 0


def main() -> int:
    if len(sys.argv) != 2:
        return 2
    if sys.argv[1] == "external":
        return external_attempt()
    if sys.argv[1] == "loopback":
        return loopback_attempt()
    if sys.argv[1] == "child":
        return child_attempt()
    if sys.argv[1] == "unrelated":
        return unrelated_attempt()
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
