#!/usr/bin/env python3
"""Loopback-only TLS proof for the exact prelaunch public observer."""

from __future__ import annotations

from pathlib import Path
import sys
import argparse
import os
import pathlib
import socket
import ssl
import subprocess
import threading


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

HOST = "api.github.com"
PATH = "/repos/MikeMarcin/BPR-Test/pulls/2"
BODY = (
    b'{"id":4650736621,"node_id":"PR_kwDOTyvof88AAAABFTSb7Q","number":2,'
    b'"state":"open","head":{"sha":"61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a",'
    b'"ref":"codex/rfc0022-live-comments-20260926"},"base":{'
    b'"sha":"a939a85a5525994fe9e9cb4c973ad9362ed427c7","ref":"codex/rfc0022-fixture-base-20260926",'
    b'"repo":{"id":1328277631,"node_id":"R_kgDOTyvofw",'
    b'"full_name":"MikeMarcin/BPR-Test"}}}'
)


class FixtureFailure(RuntimeError):
    pass


def command(arguments: list[str], cwd: pathlib.Path) -> None:
    completed = subprocess.run(
        arguments,
        cwd=cwd,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=15,
        check=False,
        env={"PATH": "/usr/bin:/bin:/usr/sbin:/sbin"},
    )
    if completed.returncode != 0:
        raise FixtureFailure("certificate command failed")


def certificates(openssl: pathlib.Path, directory: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path, pathlib.Path]:
    ca_key = directory / "ca.key"
    ca_cert = directory / "ca.pem"
    leaf_key = directory / "leaf.key"
    leaf_csr = directory / "leaf.csr"
    leaf_cert = directory / "leaf.pem"
    extension = directory / "leaf.ext"
    extension.write_text(
        "basicConstraints=critical,CA:FALSE\n"
        "keyUsage=critical,digitalSignature,keyEncipherment\n"
        "extendedKeyUsage=serverAuth\n"
        f"subjectAltName=DNS:{HOST}\n",
        encoding="ascii",
    )
    command(
        [
            str(openssl), "req", "-new", "-x509", "-newkey", "rsa:2048",
            "-sha256", "-nodes", "-days", "1", "-subj", "/CN=BPR fixture CA",
            "-keyout", str(ca_key), "-out", str(ca_cert),
        ],
        directory,
    )
    command(
        [
            str(openssl), "req", "-new", "-newkey", "rsa:2048", "-sha256",
            "-nodes", "-subj", f"/CN={HOST}", "-keyout", str(leaf_key),
            "-out", str(leaf_csr),
        ],
        directory,
    )
    command(
        [
            str(openssl), "x509", "-req", "-sha256", "-days", "1",
            "-in", str(leaf_csr), "-CA", str(ca_cert), "-CAkey", str(ca_key),
            "-CAcreateserial", "-extfile", str(extension), "-out", str(leaf_cert),
        ],
        directory,
    )
    ca_key.chmod(0o600)
    leaf_key.chmod(0o600)
    return ca_cert, leaf_cert, leaf_key


class Server:
    def __init__(self, port: int, certificate: pathlib.Path, key: pathlib.Path, scenario: str) -> None:
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", port))
        self.listener.listen(1)
        self.listener.settimeout(10)
        self.port = int(self.listener.getsockname()[1])
        self.scenario = scenario
        self.failure: BaseException | None = None
        self.sni = ""
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.set_alpn_protocols(["http/1.1"])
        context.load_cert_chain(certificate, key)

        def observe_sni(sock: ssl.SSLSocket, name: str | None, selected: ssl.SSLContext) -> None:
            del sock, selected
            self.sni = name or ""

        context.set_servername_callback(observe_sni)
        self.context = context
        self.thread = threading.Thread(target=self.serve, daemon=True)

    def __enter__(self) -> Server:
        self.thread.start()
        return self

    def __exit__(self, *_: object) -> None:
        self.listener.close()
        self.thread.join(timeout=10)
        if self.thread.is_alive():
            raise FixtureFailure("fixture did not stop")
        if self.failure is not None:
            raise self.failure
        if self.sni != HOST:
            raise FixtureFailure("observer did not use exact SNI")

    def serve(self) -> None:
        try:
            connection, address = self.listener.accept()
            if address[0] != "127.0.0.1":
                raise FixtureFailure("non-loopback peer")
            with connection, self.context.wrap_socket(connection, server_side=True) as tls:
                tls.settimeout(5)
                request = bytearray()
                while b"\r\n\r\n" not in request and len(request) <= 32 * 1024:
                    block = tls.recv(4096)
                    if not block:
                        break
                    request.extend(block)
                header = bytes(request)
                first = header.split(b"\r\n", 1)[0]
                lowered = header.lower()
                if first != f"GET {PATH} HTTP/1.1".encode("ascii"):
                    raise FixtureFailure("observer request target differs")
                for forbidden in (b"\r\nauthorization:", b"\r\ncookie:", b"\r\nproxy-authorization:"):
                    if forbidden in lowered:
                        raise FixtureFailure("observer sent ambient authority")
                for required in (
                    b"accept: application/vnd.github+json",
                    b"x-github-api-version: 2022-11-28",
                    b"accept-encoding: identity",
                ):
                    if required not in lowered:
                        raise FixtureFailure("observer header profile differs")
                if self.scenario == "redirect":
                    response = (
                        b"HTTP/1.1 302 Found\r\nLocation: https://example.invalid/\r\n"
                        b"Content-Length: 0\r\nConnection: close\r\n\r\n"
                    )
                else:
                    body = BODY
                    if self.scenario == "identity":
                        body = body.replace(b"61e8dbcd", b"71e8dbcd", 1)
                    elif self.scenario == "limit":
                        body = b"x" * (256 * 1024 + 1)
                    response = (
                        b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                        + f"Content-Length: {len(body)}\r\n".encode("ascii")
                        + b"Connection: close\r\n\r\n"
                        + body
                    )
                try:
                    tls.sendall(response)
                except (BrokenPipeError, ssl.SSLError):
                    if self.scenario != "limit":
                        raise
        except BaseException as error:
            self.failure = error


def configured_port() -> int:
    raw = os.environ.get("BPR_TEST_LOOPBACK_PORTS", "")
    if not raw:
        return 0
    value = int(raw.split(",", 1)[0])
    if value < 1 or value > 65535:
        raise FixtureFailure("invalid loopback port")
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=pathlib.Path, required=True)
    parser.add_argument("--openssl", type=pathlib.Path, required=True)
    arguments = parser.parse_args()
    executable = arguments.executable.resolve(strict=True)
    openssl = arguments.openssl.resolve(strict=True)
    with contained_temporary_directory() as directory_text:
        directory = pathlib.Path(directory_text)
        cancelled = subprocess.run(
            [str(executable), "--cancelled"],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=5,
            check=False,
            env={},
        )
        if cancelled.returncode != 0 or cancelled.stdout != b"DESIGNATED_PUBLIC_OBSERVER_OK\n":
            raise FixtureFailure("observer cancellation contract failed")
        (directory / ".curlrc").write_text(
            'header = "Authorization: ambient-config-must-not-load"\n'
            'proxy = "http://127.0.0.1:1"\n',
            encoding="ascii",
        )
        ca, certificate, key = certificates(openssl, directory)
        for scenario in ("success", "identity", "redirect", "limit"):
            with Server(configured_port(), certificate, key, scenario) as server:
                completed = subprocess.run(
                    [
                        str(executable), "--port", str(server.port), "--ca", str(ca),
                        "response-limit" if scenario == "limit" else scenario,
                    ],
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=20,
                    check=False,
                    env={
                        "HOME": str(directory),
                        "CURL_HOME": str(directory),
                        "HTTPS_PROXY": "http://127.0.0.1:1",
                        "ALL_PROXY": "http://127.0.0.1:1",
                        "CURL_CA_BUNDLE": str(directory / "missing-ca.pem"),
                    },
                )
            if completed.returncode != 0 or completed.stdout != b"DESIGNATED_PUBLIC_OBSERVER_OK\n":
                raise FixtureFailure(
                    f"observer scenario failed: {scenario}:"
                    f"{completed.returncode}:{completed.stdout.decode('ascii', 'replace').strip()}"
                )
    print("designated-live-public-observer:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
