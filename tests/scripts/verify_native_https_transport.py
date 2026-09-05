#!/usr/bin/env python3
from __future__ import annotations

import argparse
import base64
from dataclasses import dataclass, field
import hashlib
import hmac
import json
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
import time
from typing import BinaryIO


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

AUTHORITY = "ghe.bpr.invalid:443"
HOST = "ghe.bpr.invalid"
# Anonymous reads and the supported capability normalization course use the
# GitHub.com authority. The other courses use GHES. One disposable leaf
# certificate covers both names.
GITHUB_AUTHORITY = "api.github.com:443"
GITHUB_HOST = "api.github.com"
ANONYMOUS_REST_PATH = "/repos/octo-org/review-repository"
REST_PATH = "/api/v3/user"
REDIRECTED_REST_PATH = "/api/v3/user/redirected"
GRAPHQL_PATH = "/api/graphql"
# Same canonical query digest as the capability compiler contract/fixture manifest.
CAPABILITY_QUERY_SHA256 = "62ae9a45c13abe671bf7329ed43260d659afd52f93e4ddd62809bf73e500e184"
CAPABILITY_RESPONSE = (
    Path(__file__).resolve().parents[1]
    / "fixtures/github/capabilities/v1/github-com/supported.json"
)
MUTATION_PATH = "/api/v3/repos/octo-org/review-repository/pulls/17/comments"

CONNECT_HEADER_LIMIT = 16 * 1024
ORIGIN_HEADER_LIMIT = 32 * 1024
ORIGIN_BODY_LIMIT = 256 * 1024
CHILD_OUTPUT_LIMIT = 16 * 1024
CHILD_TIMEOUT_SECONDS = 10.0
SOCKET_TIMEOUT_SECONDS = 3.0
STALL_LIMIT_SECONDS = 5.0
HANDLER_GRACE_SECONDS = 0.25

SCENARIOS = (
    "success",
    "mutation-witness",
    "mutation-witness-failure",
    "mutation-proxy-no-fallback",
    "proxy-fallback",
    "same-redirect",
    "same-query-redirect",
    "cross-redirect",
    "graphql-redirect",
    "untrusted-certificate",
    "response-limit",
    "header-limit",
    "cancel",
    "timeout",
    "pre-cancel",
    "http-401",
    "http-403",
    "http-403-secondary",
    "http-403-secondary-retry",
    "http-408",
    "http-429",
    "http-503-retry",
    "http-504",
    "graphql-403-rate",
    "content-encoding",
    "response-metadata",
    "duplicate-rate-limit",
    "invalid-rate-limit",
    "wrong-rate-resource",
    "invalid-link",
    "graphql-link",
    "capability-success",
    "capability-metadata",
    "capability-refusal-encoding",
    "capability-refusal-duplicate",
    "capability-refusal-invalid",
    "capability-refusal-missing-rate",
    "capability-refusal-partial-rate",
    "capability-refusal-wrong-resource",
    "capability-refusal-content-type",
    "capability-refusal-header-bound",
    "capability-refusal-body-bound",
    "capability-redirect",
    "capability-403-rate",
    "capability-429",
)

# RFC-BPR-0017's eligibility gate asks one bounded question: can this transport
# issue a request with no authorization header while every other rule holds.
# It runs as its own suite so the CTest inventory can count exactly one
# selector for it, and so the answer is never buried in the general matrix.
RFC0017_SCENARIOS = ("credential-free",)

# The anonymous read authority answers two questions that must be asked of the
# same fixture: a real REST read succeeds with no authorization header, and the
# fixture that says so would have seen one. The canary is what keeps the first
# result from being a fixture that simply never looks.
ANONYMOUS_READ_SCENARIOS = ("anonymous-read", "anonymous-read-canary")

SUITES = {
    "default": SCENARIOS,
    "rfc0017": RFC0017_SCENARIOS,
    "anonymous_read": ANONYMOUS_READ_SCENARIOS,
}

SUITE_LABELS = {
    "default": "native-https-transport-fixture",
    "rfc0017": "rfc0017-credential-free-native-https",
    "anonymous_read": "native-https-anonymous-read-fixture",
}


def scenario_host(scenario: str) -> str:
    return (
        GITHUB_HOST
        if scenario in ANONYMOUS_READ_SCENARIOS or scenario == "capability-success" or scenario.startswith("capability-refusal-")
        else HOST
    )


def scenario_authority(scenario: str) -> str:
    return (
        GITHUB_AUTHORITY
        if scenario in ANONYMOUS_READ_SCENARIOS or scenario == "capability-success" or scenario.startswith("capability-refusal-")
        else AUTHORITY
    )


class ProbeFailure(RuntimeError):
    def __init__(self, code: str) -> None:
        super().__init__(code)
        self.code = code


def configured_loopback_ports(required: int) -> list[int]:
    raw = os.environ.get("BPR_TEST_LOOPBACK_PORTS", "")
    if not raw:
        return [0] * required
    try:
        ports = [int(value) for value in raw.split(",") if value]
    except ValueError as error:
        raise ProbeFailure("invalid-loopback-ports") from error
    if len(ports) < required or any(port < 1 or port > 65535 for port in ports):
        raise ProbeFailure("invalid-loopback-ports")
    return ports[:required]


def wipe_mutable_bytes(value: bytearray) -> None:
    for index in range(len(value)):
        value[index] = 0


@dataclass(frozen=True)
class CertificateMaterial:
    authority_certificate: Path
    leaf_certificate: Path
    leaf_private_key: Path


@dataclass
class Observation:
    lock: threading.Lock = field(default_factory=threading.Lock)
    tcp_accepts: int = 0
    connect_requests: int = 0
    tls_handshakes: int = 0
    tls_failures: int = 0
    origin_requests: int = 0
    exact_authorization_requests: int = 0
    request_targets: list[str] = field(default_factory=list)
    violations: set[str] = field(default_factory=set)
    next_request: int = 0

    def violate(self, name: str) -> None:
        with self.lock:
            self.violations.add(name)


class RefusingLoopbackProxy:
    def __init__(self, port: int) -> None:
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_REUSEADDR,
            1,
        )
        self.listener.bind(("127.0.0.1", port))
        self.listener.listen(1)
        self.listener.settimeout(0.1)
        self.port = int(self.listener.getsockname()[1])
        self.stop_event = threading.Event()
        self.accepts = 0
        self.valid_connect = False
        self.thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self) -> RefusingLoopbackProxy:
        self.thread.start()
        return self

    def __exit__(self, *_: object) -> None:
        self.stop_event.set()
        self.listener.close()
        self.thread.join(timeout=SOCKET_TIMEOUT_SECONDS)
        if self.thread.is_alive():
            raise ProbeFailure("proxy-fallback-refuser-reap")
        if self.accepts != 1 or not self.valid_connect:
            raise ProbeFailure("proxy-fallback-primary-observation")

    def _serve(self) -> None:
        while not self.stop_event.is_set():
            try:
                connection, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            self.accepts += 1
            try:
                connection.settimeout(SOCKET_TIMEOUT_SECONDS)
                request = bytearray()
                while (
                    b"\r\n\r\n" not in request
                    and len(request) <= CONNECT_HEADER_LIMIT
                ):
                    chunk = connection.recv(1024)
                    if not chunk:
                        break
                    request.extend(chunk)
                observed = bytes(request)
                first_line = observed.split(b"\r\n", 1)[0]
                lowered = observed.lower()
                self.valid_connect = (
                    first_line
                    == f"CONNECT {AUTHORITY} HTTP/1.1".encode("ascii")
                    and b"\r\nauthorization:" not in lowered
                    and b"\r\nproxy-authorization:" not in lowered
                )
                connection.sendall(
                    b"HTTP/1.1 502 Bad Gateway\r\n"
                    b"Content-Length: 0\r\n"
                    b"Connection: close\r\n\r\n"
                )
            except OSError:
                pass
            finally:
                wipe_mutable_bytes(request)
                connection.close()
            self.listener.close()
            return


@dataclass(frozen=True)
class OriginRequest:
    method: str
    target: str
    headers: dict[str, list[bytes]]
    body_size: int
    exact_capability_body: bool


def executable_path(value: str | None, fallback: str | None) -> Path:
    selected = value if value is not None else fallback
    if selected is None:
        raise ProbeFailure("missing-executable")
    candidate = Path(selected)
    if not candidate.is_absolute():
        raise ProbeFailure("non-absolute-executable")
    try:
        resolved = candidate.resolve(strict=True)
    except OSError as error:
        raise ProbeFailure("unavailable-executable") from error
    if not resolved.is_file() or not os.access(resolved, os.X_OK):
        raise ProbeFailure("unusable-executable")
    return resolved


def run_openssl(openssl: Path, arguments: list[str], directory: Path) -> None:
    try:
        completed = subprocess.run(
            [str(openssl), *arguments],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=directory,
            timeout=CHILD_TIMEOUT_SECONDS,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ProbeFailure("certificate-generation") from error
    if completed.returncode != 0:
        raise ProbeFailure("certificate-generation")


def generate_certificate_material(
    openssl: Path,
    directory: Path,
) -> CertificateMaterial:
    authority_configuration = directory / "authority.cnf"
    authority_private_key = directory / "authority-private-key.pem"
    authority_certificate = directory / "authority-certificate.pem"
    leaf_configuration = directory / "leaf.cnf"
    leaf_private_key = directory / "leaf-private-key.pem"
    leaf_request = directory / "leaf-request.pem"
    leaf_certificate = directory / "leaf-certificate.pem"

    authority_configuration.write_text(
        """
[req]
distinguished_name = authority_name
x509_extensions = authority_extensions
prompt = no

[authority_name]
CN = BetterPullReview BPR0026 disposable fixture authority

[authority_extensions]
basicConstraints = critical,CA:TRUE,pathlen:0
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
""".lstrip(),
        encoding="ascii",
    )
    leaf_configuration.write_text(
        f"""
[req]
distinguished_name = leaf_name
req_extensions = leaf_extensions
prompt = no

[leaf_name]
CN = {HOST}

[leaf_extensions]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = DNS:{HOST}, DNS:{GITHUB_HOST}
subjectKeyIdentifier = hash
""".lstrip(),
        encoding="ascii",
    )

    run_openssl(
        openssl,
        [
            "req",
            "-new",
            "-x509",
            "-newkey",
            "rsa:2048",
            "-sha256",
            "-nodes",
            "-days",
            "1",
            "-config",
            str(authority_configuration),
            "-keyout",
            str(authority_private_key),
            "-out",
            str(authority_certificate),
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
            "-sha256",
            "-nodes",
            "-config",
            str(leaf_configuration),
            "-keyout",
            str(leaf_private_key),
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
            "-sha256",
            "-days",
            "1",
            "-in",
            str(leaf_request),
            "-CA",
            str(authority_certificate),
            "-CAkey",
            str(authority_private_key),
            "-CAcreateserial",
            "-extfile",
            str(leaf_configuration),
            "-extensions",
            "leaf_extensions",
            "-out",
            str(leaf_certificate),
        ],
        directory,
    )
    authority_private_key.chmod(0o600)
    leaf_private_key.chmod(0o600)
    return CertificateMaterial(
        authority_certificate=authority_certificate,
        leaf_certificate=leaf_certificate,
        leaf_private_key=leaf_private_key,
    )


def receive_through(
    connection: socket.socket,
    marker: bytes,
    limit: int,
) -> tuple[bytes, bytes]:
    received = bytearray()
    while True:
        separator = received.find(marker)
        if separator >= 0:
            boundary = separator + len(marker)
            return bytes(received[:boundary]), bytes(received[boundary:])
        if len(received) >= limit:
            raise ProbeFailure("inbound-header-limit")
        block = connection.recv(min(4096, limit - len(received)))
        if not block:
            raise ProbeFailure("inbound-header-truncated")
        received.extend(block)


def parse_headers(
    block: bytes,
    expected_start: bytes,
) -> dict[str, list[bytes]]:
    if not block.endswith(b"\r\n\r\n"):
        raise ProbeFailure("invalid-header-terminator")
    lines = block[:-4].split(b"\r\n")
    if not lines or lines[0] != expected_start:
        raise ProbeFailure("invalid-request-line")
    if len(lines) > 65:
        raise ProbeFailure("inbound-header-count")

    headers: dict[str, list[bytes]] = {}
    for line in lines[1:]:
        if b":" not in line or len(line) > 8192:
            raise ProbeFailure("invalid-header-field")
        raw_name, raw_value = line.split(b":", 1)
        if (
            not raw_name
            or not all(
                byte in b"abcdefghijklmnopqrstuvwxyz"
                b"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-"
                for byte in raw_name
            )
        ):
            raise ProbeFailure("invalid-header-name")
        name = raw_name.decode("ascii").lower()
        headers.setdefault(name, []).append(raw_value.strip())
    return headers


def one_header(
    headers: dict[str, list[bytes]],
    name: str,
) -> bytes | None:
    values = headers.get(name)
    if values is None:
        return None
    if len(values) != 1:
        raise ProbeFailure("duplicate-request-header")
    return values[0]


def exact_capability_body(body: bytearray) -> bool:
    try:
        payload = json.loads(body)
        return (
            isinstance(payload, dict)
            and set(payload) == {"query"}
            and isinstance(payload["query"], str)
            and hashlib.sha256(payload["query"].encode("utf-8")).hexdigest()
            == CAPABILITY_QUERY_SHA256
            and json.dumps(payload, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
            == body
        )
    except (ValueError, UnicodeError):
        return False


def read_origin_request(
    connection: ssl.SSLSocket,
) -> OriginRequest:
    header, remainder = receive_through(
        connection,
        b"\r\n\r\n",
        ORIGIN_HEADER_LIMIT,
    )
    lines = header[:-4].split(b"\r\n")
    if not lines:
        raise ProbeFailure("invalid-origin-request")
    request_parts = lines[0].split(b" ")
    if len(request_parts) != 3 or request_parts[2] != b"HTTP/1.1":
        raise ProbeFailure("invalid-origin-request-line")
    try:
        method = request_parts[0].decode("ascii")
        target = request_parts[1].decode("ascii")
    except UnicodeDecodeError as error:
        raise ProbeFailure("non-ascii-origin-request") from error
    headers = parse_headers(
        header,
        b" ".join(request_parts),
    )

    transfer_encoding = one_header(headers, "transfer-encoding")
    if transfer_encoding is not None:
        raise ProbeFailure("origin-transfer-encoding")
    content_length_text = one_header(headers, "content-length")
    content_length = 0
    if content_length_text is not None:
        try:
            decoded_length = content_length_text.decode("ascii")
            if not decoded_length.isdecimal():
                raise ValueError
            content_length = int(decoded_length)
        except (UnicodeDecodeError, ValueError) as error:
            raise ProbeFailure("origin-content-length") from error
    if content_length > ORIGIN_BODY_LIMIT:
        raise ProbeFailure("origin-body-limit")

    received_body = bytearray(remainder)
    if len(received_body) > content_length:
        raise ProbeFailure("origin-body-overrun")
    while len(received_body) < content_length:
        block = connection.recv(
            min(4096, content_length - len(received_body))
        )
        if not block:
            raise ProbeFailure("origin-body-truncated")
        received_body.extend(block)
    body_size = len(received_body)
    capability_body_matches = exact_capability_body(received_body)
    wipe_mutable_bytes(received_body)

    return OriginRequest(
        method=method,
        target=target,
        headers=headers,
        body_size=body_size,
        exact_capability_body=capability_body_matches,
    )


def response_bytes(
    status: int,
    reason: str,
    body: bytes,
    extra_headers: tuple[tuple[str, str], ...] = (),
    content_type: str = "application/json",
) -> bytes:
    fields = [
        f"HTTP/1.1 {status} {reason}\r\n",
        f"Content-Type: {content_type}\r\n",
        f"Content-Length: {len(body)}\r\n",
        "Connection: close\r\n",
    ]
    fields.extend(f"{name}: {value}\r\n" for name, value in extra_headers)
    fields.append("\r\n")
    return "".join(fields).encode("ascii") + body


def rate_limit_headers(
    resource: str,
    remaining: str = "42",
) -> tuple[tuple[str, str], ...]:
    return (
        ("X-RateLimit-Limit", "60"),
        ("X-RateLimit-Remaining", remaining),
        ("X-RateLimit-Reset", "1999999999"),
        ("X-RateLimit-Resource", resource),
    )


class LoopbackConnectTlsFixture:
    def __init__(
        self,
        scenario: str,
        token: bytearray,
        certificate: CertificateMaterial,
        port: int,
    ) -> None:
        self.scenario = scenario
        self.host = scenario_host(scenario)
        self.authority = scenario_authority(scenario)
        self.token = token
        self.observation = Observation()
        self.stop_event = threading.Event()
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", port))
        self.listener.listen(4)
        self.listener.settimeout(0.1)
        self.port = int(self.listener.getsockname()[1])
        self.connections: set[socket.socket] = set()
        self.connection_lock = threading.Lock()
        self.handlers: list[threading.Thread] = []

        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.maximum_version = ssl.TLSVersion.MAXIMUM_SUPPORTED
        context.set_alpn_protocols(["http/1.1"])
        context.load_cert_chain(
            certificate.leaf_certificate,
            certificate.leaf_private_key,
        )

        def observe_server_name(
            ssl_socket: ssl.SSLSocket,
            server_name: str | None,
            selected_context: ssl.SSLContext,
        ) -> None:
            del ssl_socket, selected_context
            if server_name != self.host:
                self.observation.violate("tls-server-name")

        context.set_servername_callback(observe_server_name)
        self.tls_context = context
        self.thread = threading.Thread(
            target=self._accept_connections,
            name=f"bpr0026-{scenario}-proxy",
            daemon=True,
        )

    def __enter__(self) -> LoopbackConnectTlsFixture:
        self.thread.start()
        return self

    def __exit__(
        self,
        exception_type: object,
        exception: object,
        traceback: object,
    ) -> None:
        del exception_type, exception, traceback
        self.stop_event.set()
        try:
            self.listener.close()
        except OSError:
            pass
        self.thread.join(timeout=SOCKET_TIMEOUT_SECONDS)
        for handler in self.handlers:
            handler.join(timeout=HANDLER_GRACE_SECONDS)
        with self.connection_lock:
            active = list(self.connections)
        for connection in active:
            try:
                connection.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                connection.close()
            except OSError:
                pass
        for handler in self.handlers:
            if handler.is_alive():
                handler.join(timeout=SOCKET_TIMEOUT_SECONDS)
        if self.thread.is_alive() or any(
            handler.is_alive() for handler in self.handlers
        ):
            self.observation.violate("fixture-thread-lifecycle")

    def _accept_connections(self) -> None:
        while not self.stop_event.is_set():
            try:
                connection, address = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            if address[0] != "127.0.0.1":
                self.observation.violate("non-loopback-peer")
                connection.close()
                continue
            connection.settimeout(SOCKET_TIMEOUT_SECONDS)
            with self.observation.lock:
                self.observation.tcp_accepts += 1
            with self.connection_lock:
                self.connections.add(connection)
            handler = threading.Thread(
                target=self._handle_connection,
                args=(connection,),
                name=f"bpr0026-{self.scenario}-connection",
                daemon=True,
            )
            self.handlers.append(handler)
            handler.start()

    def _handle_connection(self, connection: socket.socket) -> None:
        try:
            connect_header, remainder = receive_through(
                connection,
                b"\r\n\r\n",
                CONNECT_HEADER_LIMIT,
            )
            connect_headers = parse_headers(
                connect_header,
                f"CONNECT {self.authority} HTTP/1.1".encode("ascii"),
            )
            with self.observation.lock:
                self.observation.connect_requests += 1
            if remainder:
                self.observation.violate("connect-request-body")
            if (
                "authorization" in connect_headers
                or "proxy-authorization" in connect_headers
            ):
                self.observation.violate("connect-credential")
            connect_host = one_header(connect_headers, "host")
            if connect_host not in {
                self.authority.encode("ascii"),
                self.host.encode("ascii"),
            }:
                self.observation.violate("connect-host")

            connection.sendall(
                b"HTTP/1.1 200 Connection Established\r\n"
                b"Proxy-Agent: BetterPullReview-BPR0026-fixture\r\n"
                b"\r\n"
            )
            try:
                secured = self.tls_context.wrap_socket(
                    connection,
                    server_side=True,
                )
            except ssl.SSLError:
                with self.observation.lock:
                    self.observation.tls_failures += 1
                return
            raw_connection = connection
            connection = secured
            connection.settimeout(SOCKET_TIMEOUT_SECONDS)
            with self.connection_lock:
                self.connections.discard(raw_connection)
                self.connections.add(connection)
            with self.observation.lock:
                self.observation.tls_handshakes += 1
            if connection.selected_alpn_protocol() not in {None, "http/1.1"}:
                self.observation.violate("tls-application-protocol")
            if self.scenario == "untrusted-certificate":
                try:
                    unexpected_origin = connection.recv(1)
                except socket.timeout:
                    self.observation.violate("untrusted-client-close")
                    return
                except (ConnectionError, ssl.SSLError):
                    unexpected_origin = b""
                if unexpected_origin:
                    self.observation.violate("untrusted-origin-bytes")
                return

            request = read_origin_request(connection)
            with self.observation.lock:
                sequence = self.observation.next_request
                self.observation.next_request += 1
                self.observation.origin_requests += 1
                self.observation.request_targets.append(request.target)
            self._inspect_origin_request(request)
            self._serve_origin_response(connection, sequence)
        except ProbeFailure as error:
            self.observation.violate(error.code)
        except (ConnectionError, OSError, ssl.SSLError):
            if (
                not self.stop_event.is_set()
                and self.scenario != "mutation-witness-failure"
            ):
                self.observation.violate("connection-handler")
        except Exception:
            self.observation.violate("connection-handler")
        finally:
            with self.connection_lock:
                self.connections.discard(connection)
            try:
                connection.close()
            except OSError:
                pass

    def _inspect_origin_request(self, request: OriginRequest) -> None:
        authorization = one_header(request.headers, "authorization")
        if self.scenario in {"credential-free", "anonymous-read"}:
            # The whole point of this course. Any authorization material on the
            # wire falsifies it, so record a violation rather than a count.
            if authorization is not None:
                self.observation.violate("credential-free-authorization")
        else:
            expected_authorization = bytearray(b"Bearer ")
            expected_authorization.extend(self.token)
            exact_authorization = (
                authorization is not None
                and hmac.compare_digest(authorization, expected_authorization)
            )
            wipe_mutable_bytes(expected_authorization)
            if exact_authorization:
                with self.observation.lock:
                    self.observation.exact_authorization_requests += 1
            else:
                self.observation.violate("origin-authorization")
        if "proxy-authorization" in request.headers:
            self.observation.violate("origin-proxy-authorization")
        if one_header(request.headers, "host") != self.host.encode("ascii"):
            self.observation.violate("origin-host")
        if one_header(request.headers, "accept-encoding") != b"identity":
            self.observation.violate("origin-accept-encoding")
        if one_header(request.headers, "accept-language") != b"*":
            self.observation.violate("origin-accept-language")

        if self.scenario in {
            "mutation-witness",
            "mutation-witness-failure",
        }:
            if (
                request.method != "POST"
                or request.target != MUTATION_PATH
                or request.body_size == 0
            ):
                self.observation.violate("mutation-request")
            if (
                one_header(request.headers, "content-type")
                != b"application/json"
            ):
                self.observation.violate("mutation-content-type")
        elif self.scenario.startswith("capability-refusal-") or self.scenario in {
            "graphql-redirect",
            "graphql-link",
            "graphql-403-rate",
            "capability-success",
            "capability-metadata",
            "capability-redirect",
            "capability-403-rate",
            "capability-429",
        }:
            if (
                request.method != "POST"
                or request.target != ("/graphql" if self.host == GITHUB_HOST else GRAPHQL_PATH)
                or request.body_size == 0
            ):
                self.observation.violate("graphql-request")
            if (
                one_header(request.headers, "content-type")
                != b"application/json"
            ):
                self.observation.violate("graphql-content-type")
            if (self.scenario == "capability-success" or self.scenario.startswith("capability-refusal-")) and (
                not request.exact_capability_body
                or one_header(request.headers, "content-encoding") is not None
            ):
                self.observation.violate("capability-exact-request-body")
        else:
            allowed_targets = {REST_PATH}
            if self.scenario == "same-redirect":
                allowed_targets.add(REDIRECTED_REST_PATH)
            if self.scenario in ANONYMOUS_READ_SCENARIOS:
                allowed_targets = {ANONYMOUS_REST_PATH}
            if (
                request.method != "GET"
                or request.target not in allowed_targets
                or request.body_size != 0
            ):
                self.observation.violate("rest-request")

    def _serve_origin_response(
        self,
        connection: ssl.SSLSocket,
        sequence: int,
    ) -> None:
        if self.scenario in ANONYMOUS_READ_SCENARIOS:
            # GitHub.com is the only contract the anonymous authority admits,
            # and that contract requires the exact rate-limit tuple, so the
            # course would fail on a response that omitted it.
            connection.sendall(
                response_bytes(
                    200,
                    "OK",
                    b'{"id":1328277631,"name":"review-repository"}',
                    rate_limit_headers("core"),
                )
            )
            return
        if self.scenario in {
            "mutation-witness",
            "mutation-witness-failure",
        }:
            connection.sendall(
                response_bytes(
                    201,
                    "Created",
                    b'{"id":44,"node_id":"PRRC_fixture_44"}',
                )
            )
            return
        if self.scenario == "same-redirect" and sequence == 0:
            connection.sendall(
                response_bytes(
                    302,
                    "Found",
                    b"",
                    (
                        (
                            "Location",
                            f"https://{HOST}{REDIRECTED_REST_PATH}",
                        ),
                    ),
                )
            )
            return
        if self.scenario == "same-query-redirect":
            connection.sendall(
                response_bytes(
                    302,
                    "Found",
                    b"",
                    (
                        (
                            "Location",
                            (
                                f"https://{HOST}{REDIRECTED_REST_PATH}"
                                "?cursor=next"
                            ),
                        ),
                    ),
                )
            )
            return
        if self.scenario == "cross-redirect":
            connection.sendall(
                response_bytes(
                    302,
                    "Found",
                    b"",
                    (
                        (
                            "Location",
                            "https://redirect.bpr.invalid/api/v3/user",
                        ),
                    ),
                )
            )
            return
        if self.scenario in {"graphql-redirect", "capability-redirect"}:
            connection.sendall(
                response_bytes(
                    302,
                    "Found",
                    b"",
                    (
                        (
                            "Location",
                            f"https://{HOST}{GRAPHQL_PATH}?redirected=1",
                        ),
                    ),
                )
            )
            return
        if self.scenario == "response-limit":
            body = b'"' + (b"x" * 1023) + b'"'
            connection.sendall(response_bytes(200, "OK", body))
            return
        if self.scenario == "header-limit":
            connection.sendall(
                response_bytes(
                    200,
                    "OK",
                    b"{}",
                    (("X-Bpr-Fixture", "x" * 8193),),
                )
            )
            return
        if self.scenario in {"cancel", "timeout"}:
            deadline = time.monotonic() + STALL_LIMIT_SECONDS
            connection.settimeout(0.1)
            while (
                not self.stop_event.is_set()
                and time.monotonic() < deadline
            ):
                try:
                    if connection.recv(1) == b"":
                        return
                except socket.timeout:
                    continue
            return
        if self.scenario == "http-401":
            connection.sendall(
                response_bytes(401, "Unauthorized", b"{}")
            )
            return
        if self.scenario == "http-403":
            connection.sendall(response_bytes(403, "Forbidden", b"{}"))
            return
        if self.scenario == "http-403-secondary":
            connection.sendall(
                response_bytes(
                    403,
                    "Forbidden",
                    b'{"message":"secondary rate limit"}',
                    rate_limit_headers("core"),
                )
            )
            return
        if self.scenario == "http-403-secondary-retry":
            connection.sendall(
                response_bytes(
                    403,
                    "Forbidden",
                    b'{"message":"secondary rate limit"}',
                    rate_limit_headers("core") + (("Retry-After", "17"),),
                )
            )
            return
        if self.scenario == "http-408":
            connection.sendall(
                response_bytes(408, "Request Timeout", b"{}")
            )
            return
        if self.scenario == "http-429":
            connection.sendall(
                response_bytes(
                    429,
                    "Too Many Requests",
                    b"{}",
                    rate_limit_headers("core") + (("Retry-After", "7"),),
                )
            )
            return
        if self.scenario == "http-503-retry":
            connection.sendall(
                response_bytes(
                    503,
                    "Service Unavailable",
                    b"{}",
                    rate_limit_headers("core") + (("Retry-After", "9"),),
                )
            )
            return
        if self.scenario == "http-504":
            connection.sendall(
                response_bytes(504, "Gateway Timeout", b"{}")
            )
            return
        if self.scenario == "graphql-403-rate":
            connection.sendall(
                response_bytes(
                    403,
                    "Forbidden",
                    b'{"errors":[{"extensions":{"code":"RATE_LIMITED"}}]}',
                    rate_limit_headers("graphql"),
                )
            )
            return
        if self.scenario == "content-encoding":
            connection.sendall(
                response_bytes(
                    200,
                    "OK",
                    b"{}",
                    (("Content-Encoding", "gzip"),),
                )
            )
            return
        if self.scenario == "response-metadata":
            connection.sendall(
                response_bytes(
                    200,
                    "OK",
                    b'{"id":7,"login":"fixture-user"}',
                    (
                        (
                            "Link",
                            (
                                "<https://ghe.bpr.invalid/api/v3/user"
                                '?page=2>; rel="next"'
                            ),
                        ),
                        ("X-RateLimit-Limit", "60"),
                        ("X-RateLimit-Remaining", "42"),
                        ("X-RateLimit-Reset", "1999999999"),
                        ("X-RateLimit-Resource", "core"),
                        ("Retry-After", "7"),
                    ),
                    content_type="Application/JSON; Charset=UTF-8",
                )
            )
            return
        if self.scenario == "duplicate-rate-limit":
            connection.sendall(
                response_bytes(
                    200,
                    "OK",
                    b'{"id":7,"login":"fixture-user"}',
                    (
                        ("X-RateLimit-Remaining", "42"),
                        ("x-ratelimit-remaining", "41"),
                    ),
                )
            )
            return
        if self.scenario == "invalid-rate-limit":
            connection.sendall(
                response_bytes(
                    200,
                    "OK",
                    b'{"id":7,"login":"fixture-user"}',
                    (("X-RateLimit-Reset", "1999999999x"),),
                )
            )
            return
        if self.scenario == "wrong-rate-resource":
            connection.sendall(
                response_bytes(
                    429,
                    "Too Many Requests",
                    b"{}",
                    rate_limit_headers("search"),
                )
            )
            return
        if self.scenario == "invalid-link":
            connection.sendall(
                response_bytes(
                    200,
                    "OK",
                    b'{"id":7,"login":"fixture-user"}',
                    (("Link", 'page=2; rel="next"'),),
                )
            )
            return
        if self.scenario == "graphql-link":
            connection.sendall(
                response_bytes(
                    200,
                    "OK",
                    b"{}",
                    (
                        (
                            "Link",
                            (
                                "<https://ghe.bpr.invalid/api/graphql"
                                '?page=2>; rel="next"'
                            ),
                        ),
                    ),
                )
            )
            return

        if self.scenario.startswith("capability-refusal-"):
            headers = list(rate_limit_headers("graphql"))
            body = b"{}"
            content_type = "application/json"
            if self.scenario == "capability-refusal-encoding":
                headers.append(("Content-Encoding", "gzip"))
            elif self.scenario == "capability-refusal-duplicate":
                headers.append(("X-RateLimit-Limit", "60"))
            elif self.scenario == "capability-refusal-invalid":
                headers = [(name, "invalid" if name == "X-RateLimit-Reset" else value) for name, value in headers]
            elif self.scenario == "capability-refusal-missing-rate":
                headers = []
            elif self.scenario == "capability-refusal-partial-rate":
                headers = [("X-RateLimit-Limit", "60")]
            elif self.scenario == "capability-refusal-wrong-resource":
                headers = list(rate_limit_headers("core"))
            elif self.scenario == "capability-refusal-content-type":
                content_type = "text/plain"
            elif self.scenario == "capability-refusal-header-bound":
                headers.append(("X-Bounded-Fixture", "x" * (64 * 1024)))
            elif self.scenario == "capability-refusal-body-bound":
                body = b"x" * 2048
            connection.sendall(response_bytes(200, "OK", body, tuple(headers), content_type=content_type))
            return
        if self.scenario in {"capability-success", "capability-metadata"}:
            connection.sendall(
                response_bytes(
                    200,
                    "OK",
                    CAPABILITY_RESPONSE.read_bytes()
                    if self.scenario == "capability-success" else b"{}",
                    (
                        ("X-RateLimit-Limit", "60"),
                        ("X-RateLimit-Remaining", "42"),
                        ("X-RateLimit-Reset", "1999999999"),
                        ("X-RateLimit-Resource", "graphql"),
                    ) + (() if self.scenario == "capability-success" else (("Retry-After", "7"),)),
                    content_type="Application/JSON; Charset=UTF-8",
                )
            )
            return
        if self.scenario == "capability-403-rate":
            connection.sendall(
                response_bytes(
                    403,
                    "Forbidden",
                    b'{"errors":[{"type":"RATE_LIMITED"}]}',
                    rate_limit_headers("graphql"),
                )
            )
            return
        if self.scenario == "capability-429":
            connection.sendall(
                response_bytes(
                    429,
                    "Too Many Requests",
                    b"{}",
                    rate_limit_headers("graphql"),
                )
            )
            return
        connection.sendall(
            response_bytes(
                200,
                "OK",
                b'{"id":7,"login":"fixture-user"}',
            )
        )

    def verify(self) -> None:
        with self.observation.lock:
            accepts = self.observation.tcp_accepts
            connects = self.observation.connect_requests
            handshakes = self.observation.tls_handshakes
            tls_failures = self.observation.tls_failures
            requests = self.observation.origin_requests
            authorizations = self.observation.exact_authorization_requests
            targets = tuple(self.observation.request_targets)
            violations = set(self.observation.violations)

        if violations:
            raise ProbeFailure(f"{self.scenario}-proxy-contract")
        if self.scenario == "pre-cancel":
            expected = (0, 0, 0, 0, 0)
            actual = (accepts, connects, handshakes, requests, authorizations)
            if actual != expected or tls_failures != 0:
                raise ProbeFailure("pre-cancel-network-effect")
            return

        if self.scenario == "mutation-proxy-no-fallback":
            actual = (accepts, connects, handshakes, requests, authorizations)
            if actual != (0, 0, 0, 0, 0) or tls_failures != 0 or targets:
                raise ProbeFailure("mutation-proxy-fallback-effect")
            return
        if self.scenario == "untrusted-certificate":
            if (
                accepts != 1
                or connects != 1
                or requests != 0
                or authorizations != 0
                or targets
                or (handshakes, tls_failures) not in {(1, 0), (0, 1)}
            ):
                raise ProbeFailure("untrusted-certificate-observation")
            return

        if self.scenario == "mutation-witness-failure":
            if (
                not (
                    0 <= authorizations <= requests <= handshakes
                    <= connects <= accepts <= 1
                )
                or tls_failures != 0
                or (targets and targets != (MUTATION_PATH,))
            ):
                raise ProbeFailure("mutation-witness-failure-observation")
            return

        if self.scenario == "anonymous-read":
            # One request, zero authorizations. The two refused operations are
            # what the count proves: they never reached the wire, so the total
            # stays at the single admitted read.
            if (
                accepts != 1
                or connects != 1
                or handshakes != 1
                or requests != 1
                or authorizations != 0
                or tls_failures != 0
                or targets != (ANONYMOUS_REST_PATH,)
            ):
                raise ProbeFailure("anonymous-read-observation")
            return

        if self.scenario == "anonymous-read-canary":
            # The same fixture, the same authority, the same path — and an
            # account transport. Exactly one exact authorization proves the
            # header check above is live rather than vacuous.
            if (
                accepts != 1
                or connects != 1
                or handshakes != 1
                or requests != 1
                or authorizations != 1
                or tls_failures != 0
                or targets != (ANONYMOUS_REST_PATH,)
            ):
                raise ProbeFailure("anonymous-read-canary-observation")
            return

        if self.scenario == "credential-free":
            # Two requests, because the course proves the state is a deliberate
            # absence rather than a spent credential, and zero authorizations.
            if (
                accepts != 2
                or connects != 2
                or handshakes != 2
                or requests != 2
                or authorizations != 0
                or tls_failures != 0
                or targets != (REST_PATH, REST_PATH)
            ):
                raise ProbeFailure("credential-free-observation")
            return

        expected_requests = 2 if self.scenario == "same-redirect" else 1
        if (
            accepts != expected_requests
            or connects != expected_requests
            or handshakes != expected_requests
            or requests != expected_requests
            or authorizations != expected_requests
            or tls_failures != 0
        ):
            raise ProbeFailure(f"{self.scenario}-observation")
        if self.scenario == "same-redirect":
            if targets != (REST_PATH, REDIRECTED_REST_PATH):
                raise ProbeFailure("same-redirect-targets")
        elif self.scenario in {
            "mutation-witness",
            "mutation-witness-failure",
        }:
            if targets != (MUTATION_PATH,):
                raise ProbeFailure("mutation-witness-target")
        elif self.scenario.startswith("capability-refusal-") or self.scenario in {
            "graphql-redirect",
            "graphql-link",
            "graphql-403-rate",
            "capability-success",
            "capability-metadata",
            "capability-redirect",
            "capability-403-rate",
            "capability-429",
        }:
            if targets != (("/graphql" if self.host == GITHUB_HOST else GRAPHQL_PATH),):
                raise ProbeFailure("graphql-redirect-target")
        elif targets != (REST_PATH,):
            raise ProbeFailure(f"{self.scenario}-target")


def read_bounded_stream(
    stream: BinaryIO,
    output: bytearray,
    exceeded: threading.Event,
) -> None:
    while block := stream.read(4096):
        if len(output) + len(block) > CHILD_OUTPUT_LIMIT:
            exceeded.set()
            return
        output.extend(block)


def child_input(token: bytearray, authority_pem: bytes) -> bytearray:
    if len(token) > 0xFFFFFFFF or len(authority_pem) > 0xFFFFFFFF:
        raise ProbeFailure("fixture-input-limit")
    framed = bytearray()
    framed.extend(struct.pack("!I", len(token)))
    framed.extend(token)
    framed.extend(struct.pack("!I", len(authority_pem)))
    framed.extend(authority_pem)
    return framed


def invoke_child(
    executable: Path,
    scenario: str,
    proxy_port: int,
    fallback_proxy_port: int | None,
    token: bytearray,
    authority_pem: bytes,
    directory: Path,
) -> None:
    arguments = [
        str(executable),
        "--fixture",
        scenario,
        "--proxy-port",
        str(proxy_port),
    ]
    if fallback_proxy_port is not None:
        arguments.extend(
            ("--fallback-proxy-port", str(fallback_proxy_port))
        )
    try:
        process = subprocess.Popen(
            arguments,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=directory,
        )
    except OSError as error:
        raise ProbeFailure(f"{scenario}-child-launch") from error
    if (
        process.stdin is None
        or process.stdout is None
        or process.stderr is None
    ):
        process.kill()
        process.wait(timeout=CHILD_TIMEOUT_SECONDS)
        raise ProbeFailure(f"{scenario}-child-pipes")

    # The credential-free child is handed no credential at all. A process that
    # never receives one cannot look one up, discover one, or fall back to one.
    framed = child_input(
        bytearray()
        if scenario in {"credential-free", "anonymous-read"}
        else token,
        authority_pem,
    )
    try:
        process.stdin.write(framed)
        process.stdin.close()
    except (BrokenPipeError, OSError):
        pass
    finally:
        wipe_mutable_bytes(framed)

    standard_output = bytearray()
    standard_error = bytearray()
    output_exceeded = threading.Event()
    output_reader = threading.Thread(
        target=read_bounded_stream,
        args=(process.stdout, standard_output, output_exceeded),
        daemon=True,
    )
    error_reader = threading.Thread(
        target=read_bounded_stream,
        args=(process.stderr, standard_error, output_exceeded),
        daemon=True,
    )
    output_reader.start()
    error_reader.start()

    deadline = time.monotonic() + CHILD_TIMEOUT_SECONDS
    timed_out = False
    while process.poll() is None:
        if output_exceeded.is_set():
            process.kill()
            break
        if time.monotonic() >= deadline:
            timed_out = True
            process.kill()
            break
        output_exceeded.wait(0.01)
    try:
        return_code = process.wait(timeout=CHILD_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as error:
        process.kill()
        process.wait(timeout=CHILD_TIMEOUT_SECONDS)
        raise ProbeFailure(f"{scenario}-child-reap") from error
    output_reader.join(timeout=CHILD_TIMEOUT_SECONDS)
    error_reader.join(timeout=CHILD_TIMEOUT_SECONDS)
    if output_reader.is_alive() or error_reader.is_alive():
        raise ProbeFailure(f"{scenario}-child-drain")

    if standard_output.find(token) >= 0 or standard_error.find(token) >= 0:
        raise ProbeFailure(f"{scenario}-credential-leak")
    if timed_out:
        raise ProbeFailure(f"{scenario}-child-timeout")
    if output_exceeded.is_set():
        raise ProbeFailure(f"{scenario}-child-output-limit")
    if return_code != 0:
        raise ProbeFailure(f"{scenario}-child-exit")
    expected_output = (
        f"native-https-transport:{scenario}:pass\n".encode("ascii")
    )
    if bytes(standard_output) != expected_output or standard_error:
        raise ProbeFailure(f"{scenario}-child-output")


def random_bearer_token() -> bytearray:
    random_bytes = bytearray(secrets.token_bytes(36))
    encoded = bytearray(base64.urlsafe_b64encode(random_bytes).rstrip(b"="))
    wipe_mutable_bytes(random_bytes)
    return encoded


def verify_scenario(
    executable: Path,
    scenario: str,
    certificate: CertificateMaterial,
    authority_pem: bytes,
    directory: Path,
    ports: list[int],
) -> None:
    token = random_bearer_token()
    try:
        with LoopbackConnectTlsFixture(
            scenario,
            token,
            certificate,
            ports[0],
        ) as fixture:
            if scenario in {
                "proxy-fallback",
                "mutation-proxy-no-fallback",
            }:
                with RefusingLoopbackProxy(ports[1]) as refused:
                    invoke_child(
                        executable,
                        scenario,
                        refused.port,
                        fixture.port,
                        token,
                        authority_pem,
                        directory,
                    )
            else:
                invoke_child(
                    executable,
                    scenario,
                    fixture.port,
                    None,
                    token,
                    authority_pem,
                    directory,
                )
        fixture.verify()
    finally:
        wipe_mutable_bytes(token)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Verify BetterPullReview native HTTPS against a disposable "
            "loopback CONNECT and TLS fixture."
        )
    )
    parser.add_argument("--executable", required=True)
    parser.add_argument("--openssl")
    parser.add_argument(
        "--suite",
        choices=sorted(SUITES),
        default="default",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    label = SUITE_LABELS[arguments.suite]
    try:
        executable = executable_path(arguments.executable, None)
        openssl_fallback = shutil.which("openssl")
        openssl = executable_path(arguments.openssl, openssl_fallback)
        with contained_temporary_directory(
            prefix="bpr0026-native-https-"
        ) as temporary:
            directory = Path(temporary)
            directory.chmod(0o700)
            certificate = generate_certificate_material(
                openssl,
                directory,
            )
            authority_pem = certificate.authority_certificate.read_bytes()
            ports = configured_loopback_ports(2)
            for scenario in SUITES[arguments.suite]:
                verify_scenario(
                    executable,
                    scenario,
                    certificate,
                    authority_pem,
                    directory,
                    ports,
                )
    except ProbeFailure as error:
        print(
            f"{label}:fail:{error.code}",
            file=sys.stderr,
        )
        return 1
    except Exception:
        print(
            f"{label}:fail:internal",
            file=sys.stderr,
        )
        return 1

    print(f"{label}:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
