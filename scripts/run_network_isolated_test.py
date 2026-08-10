#!/usr/bin/env python3
"""Launch one automated test with only runner-owned local socket authority."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import secrets
import signal
import socket
import subprocess
import sys
import tempfile
from typing import NoReturn


def fail(message: str) -> NoReturn:
    print(f"TEST_NETWORK_ISOLATION_RUNNER_FAILED:{message}", file=sys.stderr)
    raise SystemExit(125)


def reserve_loopback_port() -> tuple[int, list[socket.socket]]:
    for _ in range(64):
        ipv4 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            ipv4.bind(("127.0.0.1", 0))
            port = int(ipv4.getsockname()[1])
            reservations = [ipv4]
            if socket.has_ipv6:
                ipv6 = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
                try:
                    ipv6.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
                    ipv6.bind(("::1", port))
                except OSError:
                    ipv6.close()
                    ipv4.close()
                    continue
                reservations.append(ipv6)
            return port, reservations
        except OSError:
            ipv4.close()
    fail("unable-to-reserve-loopback-port")


def reserve_loopback_ports(count: int) -> tuple[list[int], list[socket.socket]]:
    ports: list[int] = []
    reservations: list[socket.socket] = []
    try:
        for _ in range(count):
            port, sockets = reserve_loopback_port()
            ports.append(port)
            reservations.extend(sockets)
    except BaseException:
        for reservation in reservations:
            reservation.close()
        raise
    return ports, reservations


def seatbelt_string(value: Path) -> str:
    raw = str(value)
    if not raw.startswith("/") or any(character in raw for character in '"\\\n\r'):
        fail("unsafe-fixture-root")
    return f'"{raw}"'


def write_profile(
    template: Path,
    destination: Path,
    fixture_root: Path,
    ports: list[int],
) -> None:
    try:
        base = template.read_text(encoding="utf-8", errors="strict")
    except OSError as error:
        fail(f"profile-template:{error}")
    if "(allow network-" in base or "localhost:" in base or "(subpath" in base:
        fail("profile-template-contains-network-allow")
    rules = [
        "",
        ";; The trusted runner owns this mode-0700 directory for one test.",
        "(allow network-outbound network-inbound network-bind",
        f"    (subpath {seatbelt_string(fixture_root)}))",
    ]
    for port in ports:
        rules.extend(
            [
                "(allow network-outbound",
                f'    (remote ip "localhost:{port}"))',
                "(allow network-inbound network-bind",
                f'    (local ip "localhost:{port}"))',
            ]
        )
    destination.write_text(base.rstrip() + "\n" + "\n".join(rules) + "\n", encoding="utf-8")
    destination.chmod(0o600)


def unrelated_endpoints(root: Path) -> tuple[socket.socket, socket.socket, Path]:
    tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp.bind(("127.0.0.1", 0))
    tcp.listen(1)
    tcp.setblocking(False)
    unix_path = root / "unrelated.sock"
    unix = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    unix.bind(str(unix_path))
    unix.listen(1)
    unix.setblocking(False)
    return tcp, unix, unix_path


def accepted(listener: socket.socket) -> bool:
    try:
        connection, _ = listener.accept()
    except BlockingIOError:
        return False
    connection.close()
    return True


def run_child(command: list[str], environment: dict[str, str]) -> int:
    try:
        child = subprocess.Popen(command, env=environment)
    except OSError as error:
        fail(f"launch:{error}")

    forwarded_signal = 0

    def forward(signum: int, _frame: object) -> None:
        nonlocal forwarded_signal
        forwarded_signal = signum
        if child.poll() is None:
            child.send_signal(signum)

    previous: dict[int, object] = {}
    for signum in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        previous[signum] = signal.signal(signum, forward)
    try:
        return_code = child.wait()
    finally:
        for signum, handler in previous.items():
            signal.signal(signum, handler)
    if forwarded_signal and return_code == 0:
        return 128 + forwarded_signal
    return return_code


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sandbox-exec", required=True, type=Path)
    parser.add_argument("--profile-template", required=True, type=Path)
    parser.add_argument("--loopback-port-count", required=True, type=int)
    parser.add_argument("--unrelated-endpoint-probe", action="store_true")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    arguments = parser.parse_args()
    if arguments.command[:1] != ["--"] or len(arguments.command) < 2:
        fail("missing-command")
    arguments.command = arguments.command[1:]
    if arguments.loopback_port_count < 0 or arguments.loopback_port_count > 8:
        fail("invalid-loopback-port-count")
    if arguments.unrelated_endpoint_probe and arguments.loopback_port_count < 1:
        fail("unrelated-probe-requires-fixture-port")
    return arguments


def main() -> int:
    arguments = parse_arguments()
    sandbox_exec = arguments.sandbox_exec.resolve(strict=True)
    template = arguments.profile_template.resolve(strict=True)
    if sandbox_exec != Path("/usr/bin/sandbox-exec"):
        fail("unexpected-sandbox-exec")

    temporary_parent = Path("/tmp")
    if not temporary_parent.is_dir():
        fail("missing-short-temporary-root")
    with tempfile.TemporaryDirectory(
        prefix="bpr-net-",
        dir=temporary_parent,
    ) as raw_root:
        root = Path(raw_root).resolve(strict=True)
        root.chmod(0o700)
        fixture_root = root / "fixture"
        fixture_root.mkdir(mode=0o700)
        profile = root / "profile.sb"
        ports, reservations = reserve_loopback_ports(arguments.loopback_port_count)
        tcp_unrelated: socket.socket | None = None
        unix_unrelated: socket.socket | None = None
        unix_unrelated_path: Path | None = None
        try:
            if arguments.unrelated_endpoint_probe:
                tcp_unrelated, unix_unrelated, unix_unrelated_path = unrelated_endpoints(root)
            write_profile(template, profile, fixture_root, ports)
            environment = os.environ.copy()
            environment["BPR_TEST_LOOPBACK_PORTS"] = ",".join(str(port) for port in ports)
            environment["BPR_TEST_SOCKET_ROOT"] = str(fixture_root)
            environment["BPR_TEST_SOCKET_TOKEN"] = secrets.token_urlsafe(32)
            environment["TMPDIR"] = str(fixture_root)
            if tcp_unrelated is not None and unix_unrelated_path is not None:
                environment["BPR_UNRELATED_LOOPBACK_PORT"] = str(
                    int(tcp_unrelated.getsockname()[1])
                )
                environment["BPR_UNRELATED_UNIX_SOCKET"] = str(unix_unrelated_path)
            for reservation in reservations:
                reservation.close()
            reservations.clear()
            command = [
                str(sandbox_exec),
                "-f",
                str(profile),
                *arguments.command,
            ]
            result = run_child(command, environment)
            if tcp_unrelated is not None and unix_unrelated is not None:
                if accepted(tcp_unrelated) or accepted(unix_unrelated):
                    fail("unrelated-endpoint-received-connection")
            return result
        finally:
            for reservation in reservations:
                reservation.close()
            if tcp_unrelated is not None:
                tcp_unrelated.close()
            if unix_unrelated is not None:
                unix_unrelated.close()
            if unix_unrelated_path is not None:
                unix_unrelated_path.unlink(missing_ok=True)


if __name__ == "__main__":
    raise SystemExit(main())
