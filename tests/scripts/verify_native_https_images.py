#!/usr/bin/env python3
"""RFC-BPR-0084. Loopback courses for native HTTPS body-image fetches.

A CONNECT proxy on loopback terminates TLS for a fixed set of fixture
authorities with a disposable CA, answers each scenario's routes, and records
every request the transport makes: which authority, which path, and whether an
authorization header rode it. The C++ child prints the terminal state it
reached; this script judges both. Nothing leaves loopback: an authority
outside the route table is refused at CONNECT and counted.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import os
from pathlib import Path
import secrets
import shutil
import socket
import ssl
import struct
import subprocess
import sys
import threading

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

UUID = "25addea2-357a-42c0-a1c8-fba0163382cc"
GHES = "ghe.bpr.invalid"
MEDIA = "media.ghe.bpr.invalid"
STORAGE = "storage.bpr.invalid"
CAMO = "camo.githubusercontent.com"
DOTCOM = "github.com"
S3 = "github-production-user-asset-6210df.s3.amazonaws.com"
HOSTS = (GHES, MEDIA, STORAGE, CAMO, DOTCOM, S3)
PNG = b"\x89PNG\r\n\x1a\n" + b"\x00" * 56
TIMEOUT_SECONDS = 20.0
HEADER_LIMIT = 32 * 1024


class ProbeFailure(Exception):
    def __init__(self, code: str) -> None:
        super().__init__(code)
        self.code = code


@dataclass
class Response:
    status: int
    headers: tuple[tuple[str, str], ...] = ()
    body: bytes = b""
    stall: bool = False


@dataclass
class Seen:
    authority: str
    path: str
    authorization: str | None


@dataclass
class Scenario:
    name: str
    routes: dict[tuple[str, str], Response]
    expected_output: str
    expected_requests: list[tuple[str, str, bool]]
    refused_authorities: list[str] = field(default_factory=list)


def redirect(location: str) -> Response:
    return Response(302, (("Location", location), ("Content-Length", "0")))


def image(body: bytes = PNG, content_type: str = "image/png") -> Response:
    return Response(200, (("Content-Type", content_type), ("Content-Length", str(len(body)))), body)


def scenarios() -> list[Scenario]:
    camo_path = "/fixture/image"
    attachment = f"/user-attachments/assets/{UUID}"
    return [
        Scenario("dotcom-camo", {(CAMO, camo_path): image()}, f"fetched:{len(PNG)}:image/png", [(CAMO, camo_path, False)]),
        Scenario(
            "dotcom-attachment",
            {
                (DOTCOM, attachment): redirect(f"https://{S3}/1/2-{UUID}.png?X-Amz-Signature=s1&X-Amz-Expires=300"),
                (S3, f"/1/2-{UUID}.png?X-Amz-Signature=s1&X-Amz-Expires=300"): image(PNG, "image/gif"),
            },
            f"fetched:{len(PNG)}:image/gif",
            [(DOTCOM, attachment, False), (S3, f"/1/2-{UUID}.png?X-Amz-Signature=s1&X-Amz-Expires=300", False)],
        ),
        Scenario("dotcom-foreign-redirect", {(CAMO, camo_path): redirect(f"https://{STORAGE}/x.png")}, "blocked:0:", [(CAMO, camo_path, False)]),
        Scenario("dotcom-too-large", {(CAMO, camo_path): image(b"\x89PNG" + b"\x00" * 2044)}, "too_large:0:", [(CAMO, camo_path, False)]),
        Scenario("dotcom-not-found", {(CAMO, camo_path): Response(404, (("Content-Length", "0"),))}, "failed:0:", [(CAMO, camo_path, False)]),
        Scenario(
            "dotcom-redirect-loop",
            {
                (CAMO, camo_path): redirect(f"https://{CAMO}/a"),
                (CAMO, "/a"): redirect(f"https://{CAMO}/b"),
                (CAMO, "/b"): redirect(f"https://{CAMO}/c"),
                (CAMO, "/c"): redirect(f"https://{CAMO}/d"),
            },
            "failed:0:",
            [(CAMO, camo_path, False), (CAMO, "/a", False), (CAMO, "/b", False), (CAMO, "/c", False)],
        ),
        Scenario("dotcom-content-type", {(CAMO, camo_path): image(b"<html></html>", "text/html")}, "fetched:13:text/html", [(CAMO, camo_path, False)]),
        Scenario("dotcom-timeout", {(CAMO, camo_path): Response(200, stall=True)}, "failed:0:", [(CAMO, camo_path, False)]),
        Scenario(
            "ghes-credential",
            {
                (GHES, attachment): redirect(f"https://{MEDIA}/user/1/files/a.png"),
                (MEDIA, "/user/1/files/a.png"): redirect(f"https://{STORAGE}/blob?sig=1"),
                (STORAGE, "/blob?sig=1"): redirect(f"https://{GHES}/back.png"),
                (GHES, "/back.png"): image(),
            },
            f"fetched:{len(PNG)}:image/png",
            [(GHES, attachment, True), (MEDIA, "/user/1/files/a.png", True), (STORAGE, "/blob?sig=1", False), (GHES, "/back.png", False)],
        ),
        Scenario("ghes-credential-other", {(STORAGE, "/first.png"): image()}, f"fetched:{len(PNG)}:image/png", [(STORAGE, "/first.png", False)]),
        Scenario("ghes-media-binding", {(GHES, attachment): image()}, f"fetched:{len(PNG)}:image/png", [(GHES, attachment, False)]),
    ]


def executable_path(value: str) -> Path:
    candidate = Path(value)
    if not candidate.is_absolute():
        raise ProbeFailure("non-absolute-executable")
    resolved = candidate.resolve(strict=True)
    if not resolved.is_file() or not os.access(resolved, os.X_OK):
        raise ProbeFailure("unusable-executable")
    return resolved


def run_openssl(openssl: Path, arguments: list[str], directory: Path) -> None:
    completed = subprocess.run([str(openssl), *arguments], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, cwd=directory, timeout=TIMEOUT_SECONDS, check=False)
    if completed.returncode != 0:
        raise ProbeFailure("certificate-generation")


def generate_certificates(openssl: Path, directory: Path) -> tuple[Path, Path, Path]:
    authority_configuration = directory / "authority.cnf"
    leaf_configuration = directory / "leaf.cnf"
    authority_key, authority_certificate = directory / "authority-key.pem", directory / "authority.pem"
    leaf_key, leaf_request, leaf_certificate = directory / "leaf-key.pem", directory / "leaf.csr", directory / "leaf.pem"
    authority_configuration.write_text(
        "[req]\ndistinguished_name = n\nx509_extensions = e\nprompt = no\n[n]\nCN = BetterPullReview RFC0084 disposable image fixture authority\n"
        "[e]\nbasicConstraints = critical,CA:TRUE,pathlen:0\nkeyUsage = critical,keyCertSign,cRLSign\nsubjectKeyIdentifier = hash\n"
        "authorityKeyIdentifier = keyid:always\n", encoding="ascii")
    names = ", ".join(f"DNS:{host}" for host in HOSTS)
    leaf_configuration.write_text(
        f"[req]\ndistinguished_name = n\nreq_extensions = e\nprompt = no\n[n]\nCN = {GHES}\n"
        f"[e]\nbasicConstraints = critical,CA:FALSE\nkeyUsage = critical,digitalSignature,keyEncipherment\n"
        f"extendedKeyUsage = serverAuth\nsubjectAltName = {names}\nsubjectKeyIdentifier = hash\n", encoding="ascii")
    run_openssl(openssl, ["req", "-new", "-x509", "-newkey", "rsa:2048", "-sha256", "-nodes", "-days", "1", "-config",
                          str(authority_configuration), "-keyout", str(authority_key), "-out", str(authority_certificate)], directory)
    run_openssl(openssl, ["req", "-new", "-newkey", "rsa:2048", "-sha256", "-nodes", "-config", str(leaf_configuration),
                          "-keyout", str(leaf_key), "-out", str(leaf_request)], directory)
    run_openssl(openssl, ["x509", "-req", "-sha256", "-days", "1", "-in", str(leaf_request), "-CA", str(authority_certificate),
                          "-CAkey", str(authority_key), "-CAcreateserial", "-extfile", str(leaf_configuration), "-extensions", "e",
                          "-out", str(leaf_certificate)], directory)
    authority_key.chmod(0o600)
    leaf_key.chmod(0o600)
    return authority_certificate, leaf_certificate, leaf_key


def receive_head(connection: socket.socket) -> tuple[bytes, bytes]:
    received = bytearray()
    while b"\r\n\r\n" not in received:
        if len(received) > HEADER_LIMIT:
            raise ProbeFailure("header-limit")
        block = connection.recv(4096)
        if not block:
            raise ProbeFailure("truncated")
        received.extend(block)
    head, _, rest = bytes(received).partition(b"\r\n\r\n")
    return head, rest


class Proxy:
    def __init__(self, scenario: Scenario, context: ssl.SSLContext, port: int, token: str) -> None:
        self.scenario = scenario
        self.context = context
        self.token = token
        self.seen: list[Seen] = []
        self.refused: list[str] = []
        self.violations: list[str] = []
        self.lock = threading.Lock()
        self.listener = socket.create_server(("127.0.0.1", port))
        self.listener.settimeout(0.2)
        self.port = self.listener.getsockname()[1]
        self.stopping = threading.Event()
        self.threads: list[threading.Thread] = []
        self.acceptor = threading.Thread(target=self.accept, daemon=True)
        self.acceptor.start()

    def accept(self) -> None:
        while not self.stopping.is_set():
            try:
                connection, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            thread = threading.Thread(target=self.serve, args=(connection,), daemon=True)
            self.threads.append(thread)
            thread.start()

    def serve(self, connection: socket.socket) -> None:
        connection.settimeout(TIMEOUT_SECONDS)
        try:
            head, _ = receive_head(connection)
            request_line = head.split(b"\r\n", 1)[0].decode("ascii", "replace")
            parts = request_line.split(" ")
            if len(parts) != 3 or parts[0] != "CONNECT" or not parts[1].endswith(":443"):
                with self.lock:
                    self.violations.append(f"bad-connect:{request_line}")
                connection.sendall(b"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n")
                return
            authority = parts[1][:-4]
            if b"authorization" in head.lower():
                with self.lock:
                    self.violations.append("authorization-on-connect")
            if authority not in {host for host, _ in self.scenario.routes}:
                with self.lock:
                    self.refused.append(authority)
                connection.sendall(b"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n")
                return
            connection.sendall(b"HTTP/1.1 200 Connection Established\r\n\r\n")
            with self.context.wrap_socket(connection, server_side=True) as tls:
                request_head, _ = receive_head(tls)
                lines = request_head.decode("latin-1").split("\r\n")
                method, path, _ = lines[0].split(" ", 2)
                headers = {}
                for line in lines[1:]:
                    name, _, value = line.partition(":")
                    headers.setdefault(name.strip().lower(), []).append(value.strip())
                authorization = headers.get("authorization", [None])[0]
                with self.lock:
                    self.seen.append(Seen(authority, path, authorization))
                    if method != "GET":
                        self.violations.append(f"method:{method}")
                    if "cookie" in headers or "referer" in headers:
                        self.violations.append("cookie-or-referer")
                    if headers.get("accept-encoding") != ["identity"]:
                        self.violations.append("encoding")
                    if authorization is not None and authorization != f"Bearer {self.token}":
                        self.violations.append("foreign-authorization")
                response = self.scenario.routes.get((authority, path))
                if response is None:
                    tls.sendall(b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
                    return
                if response.stall:
                    self.stopping.wait(TIMEOUT_SECONDS)
                    return
                reason = {200: "OK", 302: "Found", 404: "Not Found"}.get(response.status, "Status")
                head_lines = [f"HTTP/1.1 {response.status} {reason}", "Connection: close", *[f"{n}: {v}" for n, v in response.headers]]
                tls.sendall(("\r\n".join(head_lines) + "\r\n\r\n").encode("latin-1") + response.body)
        except (OSError, ssl.SSLError, ProbeFailure, ValueError):
            return
        finally:
            try:
                connection.close()
            except OSError:
                pass

    def close(self) -> None:
        self.stopping.set()
        self.listener.close()
        self.acceptor.join(TIMEOUT_SECONDS)
        for thread in self.threads:
            thread.join(TIMEOUT_SECONDS)


def frame(value: bytes) -> bytes:
    return struct.pack(">I", len(value)) + value


def verify(executable: Path, scenario: Scenario, context: ssl.SSLContext, authority_pem: bytes, port: int) -> None:
    token = "fixture_" + secrets.token_hex(16)
    proxy = Proxy(scenario, context, port, token)
    try:
        completed = subprocess.run([str(executable), "--fixture", scenario.name, "--proxy-port", str(proxy.port)],
                                   input=frame(token.encode("ascii")) + frame(authority_pem), stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, timeout=TIMEOUT_SECONDS * 2, check=False)
    finally:
        proxy.close()
    output = completed.stdout.decode("utf-8", "replace").strip()
    expected = f"native-https-image:{scenario.name}:{scenario.expected_output}"
    if completed.returncode != 0 or output != expected:
        raise ProbeFailure(f"{scenario.name}:output:{output or completed.stderr.decode('utf-8', 'replace').strip()}")
    if token.encode("ascii") in completed.stdout or token.encode("ascii") in completed.stderr:
        raise ProbeFailure(f"{scenario.name}:token-in-output")
    if proxy.violations:
        raise ProbeFailure(f"{scenario.name}:violation:{proxy.violations[0]}")
    observed = [(seen.authority, seen.path, seen.authorization is not None) for seen in proxy.seen]
    if observed != scenario.expected_requests:
        raise ProbeFailure(f"{scenario.name}:requests:{observed}")
    if proxy.refused != scenario.refused_authorities:
        raise ProbeFailure(f"{scenario.name}:refused:{proxy.refused}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--openssl")
    arguments = parser.parse_args()
    try:
        executable = executable_path(arguments.executable)
        openssl = Path(arguments.openssl or shutil.which("openssl") or "")
        if not openssl.is_absolute():
            raise ProbeFailure("missing-openssl")
        raw_ports = os.environ.get("BPR_TEST_LOOPBACK_PORTS", "")
        port = int(raw_ports.split(",")[0]) if raw_ports else 0
        with contained_temporary_directory(prefix="bpr0084-native-images-") as temporary:
            directory = Path(temporary)
            directory.chmod(0o700)
            authority, leaf, key = generate_certificates(openssl, directory)
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.minimum_version = ssl.TLSVersion.TLSv1_2
            context.load_cert_chain(str(leaf), str(key))
            authority_pem = authority.read_bytes()
            for scenario in scenarios():
                verify(executable, scenario, context, authority_pem, port)
    except ProbeFailure as error:
        print(f"native-https-images:fail:{error.code}", file=sys.stderr)
        return 1
    print("native-https-images:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
