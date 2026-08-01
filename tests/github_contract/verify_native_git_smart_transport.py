#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
from dataclasses import dataclass, field
import os
from pathlib import Path
import secrets
import socket
import ssl
import struct
import subprocess
import tempfile
import threading
import time


GHES_HOST = "ghe.bpr.invalid"
GITHUB_COM_HOST = "github.com"
DISCOVERY_TARGET = (
    "/octo-org/review-repository.git/info/refs"
    "?service=git-upload-pack"
)
UPLOAD_TARGET = "/octo-org/review-repository.git/git-upload-pack"
REQUEST_BODY = b"0014command=ls-refs\n0000"
DISCOVERY_BODY = b"001eversion 2\n0000"
LS_REFS_BODY = b"0008NAK\n0000"
FETCH_BODY_BYTES = 200_000
HEADER_LIMIT = 32 * 1024
SOCKET_TIMEOUT = 4.0
CHILD_TIMEOUT = 12.0
SCENARIOS = (
    "discovery",
    "ls-refs",
    "fetch",
    "redirect-300",
    "redirect-302",
    "redirect-399",
    "http-401",
    "http-403",
    "http-503",
    "header-limit",
    "wrong-media",
    "response-limit",
    "consumer-reject",
    "direct-fallback",
    "timeout",
    "cancel",
    "revoke",
    "revoke-isolation",
    "active-shutdown",
)
GITHUB_COM_SCENARIOS = ("discovery", "ls-refs", "fetch")


class ProbeFailure(RuntimeError):
    pass


@dataclass(frozen=True)
class Certificates:
    authority: Path
    leaf: Path
    key: Path


@dataclass
class Observation:
    lock: threading.Lock = field(default_factory=threading.Lock)
    connects: int = 0
    origins: int = 0
    sni: list[str | None] = field(default_factory=list)
    violations: set[str] = field(default_factory=set)

    def fail(self, reason: str) -> None:
        with self.lock:
            self.violations.add(reason)


def run_openssl(openssl: Path, arguments: list[str], cwd: Path) -> None:
    result = subprocess.run(
        [str(openssl), *arguments],
        cwd=cwd,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=CHILD_TIMEOUT,
        check=False,
    )
    if result.returncode != 0:
        raise ProbeFailure("certificate-generation")


def certificates(openssl: Path, directory: Path) -> Certificates:
    authority_config = directory / "authority.cnf"
    authority_key = directory / "authority-key.pem"
    authority_cert = directory / "authority.pem"
    leaf_config = directory / "leaf.cnf"
    leaf_key = directory / "leaf-key.pem"
    leaf_request = directory / "leaf.csr"
    leaf_cert = directory / "leaf.pem"

    authority_config.write_text(
        """
[req]
distinguished_name = dn
x509_extensions = ext
prompt = no
[dn]
CN = BetterPullReview BPR0043 fixture authority
[ext]
basicConstraints = critical,CA:TRUE,pathlen:0
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
""".lstrip(),
        encoding="ascii",
    )
    leaf_config.write_text(
        f"""
[req]
distinguished_name = dn
req_extensions = ext
prompt = no
[dn]
CN = BetterPullReview native Git fixture
[ext]
subjectAltName = @alt_names
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
[alt_names]
DNS.1 = {GHES_HOST}
DNS.2 = {GITHUB_COM_HOST}
""".lstrip(),
        encoding="ascii",
    )
    run_openssl(
        openssl,
        [
            "req",
            "-x509",
            "-newkey",
            "rsa:2048",
            "-nodes",
            "-days",
            "1",
            "-config",
            str(authority_config),
            "-keyout",
            str(authority_key),
            "-out",
            str(authority_cert),
        ],
        directory,
    )
    run_openssl(
        openssl,
        [
            "req",
            "-new",
            "-newkey",
            "rsa:2048",
            "-nodes",
            "-config",
            str(leaf_config),
            "-keyout",
            str(leaf_key),
            "-out",
            str(leaf_request),
        ],
        directory,
    )
    run_openssl(
        openssl,
        [
            "x509",
            "-req",
            "-in",
            str(leaf_request),
            "-CA",
            str(authority_cert),
            "-CAkey",
            str(authority_key),
            "-CAcreateserial",
            "-days",
            "1",
            "-extfile",
            str(leaf_config),
            "-extensions",
            "ext",
            "-out",
            str(leaf_cert),
        ],
        directory,
    )
    os.chmod(authority_key, 0o600)
    os.chmod(leaf_key, 0o600)
    return Certificates(authority_cert, leaf_cert, leaf_key)


def receive_headers(connection: socket.socket) -> tuple[bytes, bytes]:
    result = bytearray()
    while b"\r\n\r\n" not in result:
        chunk = connection.recv(4096)
        if not chunk:
            raise ProbeFailure("truncated-headers")
        result.extend(chunk)
        if len(result) > HEADER_LIMIT:
            raise ProbeFailure("header-limit")
    marker = result.index(b"\r\n\r\n") + 4
    return bytes(result[:marker]), bytes(result[marker:])


def parse_headers(raw: bytes) -> tuple[str, dict[str, list[bytes]]]:
    lines = raw[:-4].split(b"\r\n")
    if not lines:
        raise ProbeFailure("missing-request-line")
    try:
        request_line = lines[0].decode("ascii")
    except UnicodeDecodeError as error:
        raise ProbeFailure("request-line-encoding") from error
    headers: dict[str, list[bytes]] = {}
    for line in lines[1:]:
        if b":" not in line:
            raise ProbeFailure("malformed-header")
        name, value = line.split(b":", 1)
        try:
            key = name.decode("ascii").lower()
        except UnicodeDecodeError as error:
            raise ProbeFailure("header-name-encoding") from error
        headers.setdefault(key, []).append(value.strip())
    return request_line, headers


def one_header(
    headers: dict[str, list[bytes]],
    name: str,
) -> bytes:
    values = headers.get(name, [])
    if len(values) != 1:
        raise ProbeFailure(f"header-{name}")
    return values[0]


class SmartProxy:
    def __init__(
        self,
        material: Certificates,
        scenario: str,
        token: str,
        host: str,
    ) -> None:
        self.scenario = scenario
        self.token = token
        self.host = host
        self.observation = Observation()
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(4)
        self.listener.settimeout(0.1)
        self.port = int(self.listener.getsockname()[1])
        self.stop = threading.Event()
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version = ssl.TLSVersion.TLSv1_2
        self.context.maximum_version = ssl.TLSVersion.TLSv1_2
        self.context.load_cert_chain(material.leaf, material.key)

        def observe_sni(
            _socket: ssl.SSLSocket,
            server_name: str | None,
            _context: ssl.SSLContext,
        ) -> None:
            with self.observation.lock:
                self.observation.sni.append(server_name)

        self.context.set_servername_callback(observe_sni)
        self.thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self) -> SmartProxy:
        self.thread.start()
        return self

    def __exit__(self, *_: object) -> None:
        self.stop.set()
        self.listener.close()
        self.thread.join(timeout=SOCKET_TIMEOUT)
        if self.thread.is_alive():
            raise ProbeFailure("proxy-reap")

    def _serve(self) -> None:
        while not self.stop.is_set():
            try:
                connection, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            try:
                self._handle(connection)
            except (OSError, ssl.SSLError, ProbeFailure) as error:
                if not (
                    self.scenario
                    in {
                        "timeout",
                        "cancel",
                        "revoke",
                        "active-shutdown",
                        "consumer-reject",
                    }
                    and isinstance(error, OSError)
                ):
                    self.observation.fail(str(error))
            finally:
                connection.close()

    def _handle(self, connection: socket.socket) -> None:
        connection.settimeout(SOCKET_TIMEOUT)
        connect_raw, connect_remainder = receive_headers(connection)
        if connect_remainder:
            raise ProbeFailure("connect-body")
        connect_line, connect_headers = parse_headers(connect_raw)
        with self.observation.lock:
            self.observation.connects += 1
        if connect_line != f"CONNECT {self.host}:443 HTTP/1.1":
            raise ProbeFailure("connect-authority")
        if "authorization" in connect_headers:
            raise ProbeFailure("proxy-received-authorization")
        expected_authorization = b"Basic " + base64.b64encode(
            f"x-access-token:{self.token}".encode("ascii")
        )
        if (
            self.token.encode("ascii") in connect_raw
            or expected_authorization in connect_raw
        ):
            raise ProbeFailure("proxy-received-credential")
        connection.sendall(
            b"HTTP/1.1 200 Connection Established\r\n"
            b"Content-Length: 0\r\n\r\n"
        )
        with self.context.wrap_socket(
            connection,
            server_side=True,
        ) as tls:
            tls.settimeout(SOCKET_TIMEOUT)
            request_raw, request_remainder = receive_headers(tls)
            request_line, headers = parse_headers(request_raw)
            content_length = int(one_header(headers, "content-length")) if (
                "content-length" in headers
            ) else 0
            body = bytearray(request_remainder)
            if len(body) > content_length:
                raise ProbeFailure("oversized-body")
            while len(body) < content_length:
                chunk = tls.recv(content_length - len(body))
                if not chunk:
                    raise ProbeFailure("truncated-body")
                body.extend(chunk)
            with self.observation.lock:
                self.observation.origins += 1
            self._validate_origin(
                request_raw,
                request_line,
                headers,
                bytes(body),
            )
            self._respond(tls)

    def _validate_origin(
        self,
        request_raw: bytes,
        request_line: str,
        headers: dict[str, list[bytes]],
        body: bytes,
    ) -> None:
        discovery = (
            self.scenario.startswith("redirect")
            or self.scenario.startswith("http-")
            or self.scenario
            in {
                "discovery",
                "wrong-media",
                "header-limit",
                "direct-fallback",
                "timeout",
                "cancel",
                "revoke",
                "revoke-isolation",
                "active-shutdown",
            }
        )
        expected_line = (
            f"GET {DISCOVERY_TARGET} HTTP/1.1"
            if discovery
            else f"POST {UPLOAD_TARGET} HTTP/1.1"
        )
        if request_line != expected_line:
            raise ProbeFailure("origin-target")
        if one_header(headers, "host") != self.host.encode("ascii"):
            raise ProbeFailure("origin-host")
        if one_header(headers, "git-protocol") != b"version=2":
            raise ProbeFailure("git-protocol")
        expected_authorization = b"Basic " + base64.b64encode(
            f"x-access-token:{self.token}".encode("ascii")
        )
        if one_header(headers, "authorization") != expected_authorization:
            raise ProbeFailure("authorization")
        if (
            request_raw.count(expected_authorization) != 1
            or self.token.encode("ascii") in request_raw
        ):
            raise ProbeFailure("credential-origin")
        expected_accept = (
            b"application/x-git-upload-pack-advertisement"
            if discovery
            else b"application/x-git-upload-pack-result"
        )
        if one_header(headers, "accept") != expected_accept:
            raise ProbeFailure("accept")
        if one_header(headers, "accept-encoding") != b"identity":
            raise ProbeFailure("accept-encoding")
        if discovery:
            if body or "content-type" in headers:
                raise ProbeFailure("discovery-body")
        else:
            if (
                one_header(headers, "content-type")
                != b"application/x-git-upload-pack-request"
                or body != REQUEST_BODY
            ):
                raise ProbeFailure("upload-request")

    def _respond(self, tls: ssl.SSLSocket) -> None:
        if self.scenario in {
            "timeout",
            "cancel",
            "revoke",
            "active-shutdown",
        }:
            time.sleep(2.0 if self.scenario == "timeout" else 3.0)
            return
        if self.scenario == "revoke-isolation":
            time.sleep(2.5)
        if self.scenario.startswith("redirect"):
            status = self.scenario.removeprefix("redirect-").encode("ascii")
            hostile_headers = (
                b"X-Fill: " + b"x" * 9000 + b"\r\n"
                if self.scenario == "redirect-399"
                else b""
            )
            tls.sendall(
                b"HTTP/1.1 " + status + b" Redirect\r\n"
                b"Location: https://"
                + self.host.encode("ascii")
                + b"/redirected"
                b"?service=git-upload-pack\r\n"
                + hostile_headers
                + b"Content-Length: 0\r\n"
                b"Connection: close\r\n\r\n"
            )
            return
        if self.scenario.startswith("http-"):
            status = self.scenario.removeprefix("http-").encode("ascii")
            challenge = (
                b'WWW-Authenticate: Basic realm="fixture"\r\n'
                if self.scenario == "http-401"
                else b""
            )
            tls.sendall(
                b"HTTP/1.1 "
                + status
                + b" Fixture\r\n"
                + challenge
                + b"Content-Length: 0\r\n"
                b"Connection: close\r\n\r\n"
            )
            return
        if self.scenario == "header-limit":
            tls.sendall(
                b"HTTP/1.1 200 OK\r\n"
                b"Content-Type: "
                b"application/x-git-upload-pack-advertisement\r\n"
                b"X-Fill: "
                + b"x" * 9000
                + b"\r\nContent-Length: 0\r\n"
                b"Connection: close\r\n\r\n"
            )
            return
        if self.scenario == "wrong-media":
            body = DISCOVERY_BODY
            content_type = b"application/json"
        elif self.scenario in {
            "discovery",
            "direct-fallback",
            "revoke-isolation",
        }:
            body = DISCOVERY_BODY
            content_type = (
                b"application/x-git-upload-pack-advertisement"
            )
        elif self.scenario == "ls-refs":
            body = LS_REFS_BODY
            content_type = b"application/x-git-upload-pack-result"
        elif self.scenario == "response-limit":
            body = b"P" * 2048
            content_type = b"application/x-git-upload-pack-result"
        else:
            body = b"P" * FETCH_BODY_BYTES
            content_type = b"application/x-git-upload-pack-result"
        tls.sendall(
            b"HTTP/1.1 200 OK\r\n"
            + b"Content-Type: "
            + content_type
            + b"\r\nContent-Length: "
            + str(len(body)).encode("ascii")
            + b"\r\nConnection: close\r\n\r\n"
        )
        for offset in range(0, len(body), 4096):
            tls.sendall(body[offset : offset + 4096])


def frame(value: bytes) -> bytes:
    return struct.pack(">I", len(value)) + value


def run_scenario(
    executable: Path,
    material: Certificates,
    scenario: str,
    profile_name: str,
    host: str,
) -> None:
    token = secrets.token_urlsafe(32)
    with SmartProxy(material, scenario, token, host) as proxy:
        child = subprocess.run(
            [
                str(executable),
                "--fixture",
                scenario,
                str(proxy.port),
                profile_name,
            ],
            input=frame(token.encode("ascii"))
            + frame(material.authority.read_bytes()),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=CHILD_TIMEOUT,
            check=False,
        )
        time.sleep(0.2)
    if child.returncode != 0 or child.stdout != b"OK\n":
        raise ProbeFailure(
            f"{profile_name}-{scenario}-child:"
            + child.stderr[:4096].decode("utf-8", errors="replace")
            + ":violations="
            + ",".join(sorted(proxy.observation.violations))
        )
    observation = proxy.observation
    if observation.violations:
        raise ProbeFailure(
            f"{profile_name}-{scenario}-violations:"
            + ",".join(sorted(observation.violations))
        )
    if observation.connects != 1 or observation.origins != 1:
        raise ProbeFailure(
            f"{profile_name}-{scenario}-request-count:"
            f"connects={observation.connects},origins={observation.origins}"
        )
    if observation.sni != [host]:
        raise ProbeFailure(f"{profile_name}-{scenario}-sni")


def executable(value: str) -> Path:
    path = Path(value).resolve(strict=True)
    if not path.is_file() or not os.access(path, os.X_OK):
        raise ProbeFailure("executable")
    return path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--openssl", required=True)
    arguments = parser.parse_args()
    try:
        target = executable(arguments.executable)
        openssl = executable(arguments.openssl)
        with tempfile.TemporaryDirectory(prefix="bpr0043-smart-") as value:
            directory = Path(value)
            material = certificates(openssl, directory)
            for scenario in SCENARIOS:
                run_scenario(
                    target,
                    material,
                    scenario,
                    "ghes",
                    GHES_HOST,
                )
            for scenario in GITHUB_COM_SCENARIOS:
                run_scenario(
                    target,
                    material,
                    scenario,
                    "github-com",
                    GITHUB_COM_HOST,
                )
        print("OK")
        return 0
    except (
        OSError,
        ValueError,
        subprocess.SubprocessError,
        ProbeFailure,
    ) as error:
        print(f"FAILED: {error}", file=os.sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
