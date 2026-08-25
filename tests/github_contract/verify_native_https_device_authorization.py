#!/usr/bin/env python3
from __future__ import annotations

import sys
import argparse
from dataclasses import dataclass, field
import json
import os
from pathlib import Path
import secrets
import socket
import ssl
import struct
import subprocess
import threading
import time


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

HOST = "github.com"
AUTHORITY = f"{HOST}:443"
DEVICE_CODE_TARGET = "/login/device/code"
TOKEN_TARGET = "/login/oauth/access_token"
CLIENT_ID = "Iv23bprfixtureclient"
ENCODED_GRANT_TYPE = (
    "urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code"
)
VERIFICATION_URI = "https://github.com/login/device"
ERROR_URI = (
    "https://docs.github.com/apps/oauth-apps/building-oauth-apps"
    "/authorizing-oauth-apps"
)
DEVICE_CODE_EXPIRES_IN = 899
DEVICE_CODE_INTERVAL = 5
TOKEN_EXPIRES_IN = 28800
REFRESH_TOKEN_EXPIRES_IN = 15897600
SLOW_DOWN_INTERVAL = 10
RESPONSE_LIMIT = 64 * 1024
CONTENT_TYPE = "application/json; charset=utf-8"

HEADER_LIMIT = 32 * 1024
BODY_LIMIT = 256 * 1024
CHILD_OUTPUT_LIMIT = 16 * 1024
SOCKET_TIMEOUT = 4.0
CHILD_TIMEOUT = 15.0
STALL_LIMIT = 3.0

MARKER = "native-https-device-authorization"

SCENARIOS = (
    "device-code-success",
    "device-code-disabled",
    "device-code-untrusted-certificate",
    "device-code-cross-redirect",
    "device-code-query-redirect",
    "device-code-response-limit",
    "device-code-cancel",
    "token-success",
    "token-slow-down",
    "token-untrusted-certificate",
    "token-cross-redirect",
    "token-query-redirect",
    "token-response-limit",
    "token-cancel",
)

TOLERATED_CLOSE_COURSES = frozenset(
    {"cancel", "untrusted-certificate", "response-limit"}
)


class ProbeFailure(RuntimeError):
    def __init__(self, code: str) -> None:
        super().__init__(code)
        self.code = code


@dataclass(frozen=True)
class Certificates:
    authority: Path
    leaf: Path
    key: Path


@dataclass(frozen=True)
class Material:
    device_code: str
    user_code: str
    access_token: str
    refresh_token: str

    def transient_values(self) -> tuple[bytes, ...]:
        return (
            self.device_code.encode("ascii"),
            self.user_code.encode("ascii"),
            self.access_token.encode("ascii"),
            self.refresh_token.encode("ascii"),
        )


@dataclass
class Observation:
    lock: threading.Lock = field(default_factory=threading.Lock)
    connects: int = 0
    handshakes: int = 0
    handshake_failures: int = 0
    origins: int = 0
    sni: list[str | None] = field(default_factory=list)
    violations: set[str] = field(default_factory=set)

    def violate(self, reason: str) -> None:
        with self.lock:
            self.violations.add(reason)


def endpoint_of(scenario: str) -> str:
    return "device_code" if scenario.startswith("device-code-") else "token"


def course_of(scenario: str) -> str:
    prefix = "device-code-" if scenario.startswith("device-code-") else "token-"
    return scenario[len(prefix):]


def target_of(scenario: str) -> str:
    return (
        DEVICE_CODE_TARGET
        if endpoint_of(scenario) == "device_code"
        else TOKEN_TARGET
    )


def configured_loopback_port() -> int:
    raw = os.environ.get("BPR_TEST_LOOPBACK_PORTS", "")
    if not raw:
        return 0
    try:
        ports = [int(value) for value in raw.split(",") if value]
    except ValueError as error:
        raise ProbeFailure("invalid-loopback-port") from error
    if len(ports) != 1 or ports[0] < 1 or ports[0] > 65535:
        raise ProbeFailure("invalid-loopback-port")
    return ports[0]


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
CN = BetterPullReview BPR0278 fixture authority
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
CN = BetterPullReview device authorization fixture
[ext]
subjectAltName = DNS:{HOST}
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
subjectKeyIdentifier = hash
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
            "-sha256",
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
            "-sha256",
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
            "-sha256",
            "-days",
            "1",
            "-in",
            str(leaf_request),
            "-CA",
            str(authority_cert),
            "-CAkey",
            str(authority_key),
            "-CAcreateserial",
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


def one_header(headers: dict[str, list[bytes]], name: str) -> bytes:
    values = headers.get(name, [])
    if len(values) != 1:
        raise ProbeFailure(f"header-{name}")
    return values[0]


def json_body(payload: dict[str, object]) -> bytes:
    return json.dumps(payload, sort_keys=True).encode("ascii")


def response_bytes(
    status: int,
    reason: str,
    body: bytes,
    extra_headers: tuple[tuple[str, str], ...] = (),
    declared_length: int | None = None,
) -> bytes:
    length = len(body) if declared_length is None else declared_length
    fields = [
        f"HTTP/1.1 {status} {reason}\r\n",
        f"Content-Type: {CONTENT_TYPE}\r\n",
        f"Content-Length: {length}\r\n",
        "Connection: close\r\n",
    ]
    fields.extend(f"{name}: {value}\r\n" for name, value in extra_headers)
    fields.append("\r\n")
    return "".join(fields).encode("ascii") + body


def success_payload(scenario: str, material: Material) -> bytes:
    if endpoint_of(scenario) == "device_code":
        return json_body(
            {
                "device_code": material.device_code,
                "expires_in": DEVICE_CODE_EXPIRES_IN,
                "interval": DEVICE_CODE_INTERVAL,
                "user_code": material.user_code,
                "verification_uri": VERIFICATION_URI,
            }
        )
    return json_body(
        {
            "access_token": material.access_token,
            "expires_in": TOKEN_EXPIRES_IN,
            "refresh_token": material.refresh_token,
            "refresh_token_expires_in": REFRESH_TOKEN_EXPIRES_IN,
            "scope": "",
            "token_type": "bearer",
        }
    )


class DeviceAuthorizationProxy:
    def __init__(
        self,
        material: Certificates,
        scenario: str,
        transients: Material,
        port: int,
    ) -> None:
        self.scenario = scenario
        self.course = course_of(scenario)
        self.transients = transients
        self.observation = Observation()
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", port))
        self.listener.listen(4)
        self.listener.settimeout(0.1)
        self.port = int(self.listener.getsockname()[1])
        self.stop = threading.Event()
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version = ssl.TLSVersion.TLSv1_2
        self.context.set_alpn_protocols(["http/1.1"])
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

    def __enter__(self) -> DeviceAuthorizationProxy:
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
                connection, address = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            if address[0] != "127.0.0.1":
                self.observation.violate("non-loopback-peer")
                connection.close()
                continue
            try:
                self._handle(connection)
            except (OSError, ProbeFailure) as error:
                tolerated = (
                    self.course in TOLERATED_CLOSE_COURSES
                    and isinstance(error, OSError)
                )
                if not tolerated:
                    self.observation.violate(str(error))
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
        if connect_line != f"CONNECT {AUTHORITY} HTTP/1.1":
            raise ProbeFailure("connect-authority")
        if (
            "authorization" in connect_headers
            or "proxy-authorization" in connect_headers
        ):
            raise ProbeFailure("connect-credential")
        connection.sendall(
            b"HTTP/1.1 200 Connection Established\r\n"
            b"Content-Length: 0\r\n\r\n"
        )

        try:
            tls = self.context.wrap_socket(connection, server_side=True)
        except (OSError, ssl.SSLError):
            with self.observation.lock:
                self.observation.handshake_failures += 1
            return
        with tls:
            tls.settimeout(SOCKET_TIMEOUT)
            with self.observation.lock:
                self.observation.handshakes += 1
            if tls.selected_alpn_protocol() not in {None, "http/1.1"}:
                raise ProbeFailure("tls-application-protocol")
            if self.course == "untrusted-certificate":
                # The child was handed no authority for this leaf, so it must
                # refuse before a single request byte leaves the process.
                try:
                    leaked = tls.recv(1)
                except socket.timeout:
                    raise ProbeFailure("untrusted-client-held-open") from None
                except (OSError, ssl.SSLError):
                    leaked = b""
                if leaked:
                    raise ProbeFailure("untrusted-origin-bytes")
                return

            request_raw, remainder = receive_headers(tls)
            request_line, headers = parse_headers(request_raw)
            body = self._read_body(tls, headers, remainder)
            with self.observation.lock:
                self.observation.origins += 1
            self._validate(request_raw, request_line, headers, body)
            self._respond(tls)

    def _read_body(
        self,
        tls: ssl.SSLSocket,
        headers: dict[str, list[bytes]],
        remainder: bytes,
    ) -> bytes:
        if "transfer-encoding" in headers:
            raise ProbeFailure("origin-transfer-encoding")
        declared = one_header(headers, "content-length").decode("ascii")
        if not declared.isdecimal():
            raise ProbeFailure("origin-content-length")
        content_length = int(declared)
        if content_length > BODY_LIMIT:
            raise ProbeFailure("origin-body-limit")
        body = bytearray(remainder)
        if len(body) > content_length:
            raise ProbeFailure("origin-body-overrun")
        while len(body) < content_length:
            chunk = tls.recv(content_length - len(body))
            if not chunk:
                raise ProbeFailure("origin-body-truncated")
            body.extend(chunk)
        return bytes(body)

    def _validate(
        self,
        request_raw: bytes,
        request_line: str,
        headers: dict[str, list[bytes]],
        body: bytes,
    ) -> None:
        target = target_of(self.scenario)
        if request_line != f"POST {target} HTTP/1.1":
            raise ProbeFailure("origin-target")
        if one_header(headers, "host") != HOST.encode("ascii"):
            raise ProbeFailure("origin-host")
        if one_header(headers, "accept") != b"application/json":
            raise ProbeFailure("origin-accept")
        if one_header(headers, "user-agent") != b"BetterPullReview/0.1":
            raise ProbeFailure("origin-user-agent")
        if (
            one_header(headers, "content-type")
            != b"application/x-www-form-urlencoded"
        ):
            raise ProbeFailure("origin-content-type")
        if one_header(headers, "accept-encoding") != b"identity":
            raise ProbeFailure("origin-accept-encoding")
        if one_header(headers, "accept-language") != b"*":
            raise ProbeFailure("origin-accept-language")
        # The device flow runs before any credential exists. Any authorization
        # material on the wire, under any header name, falsifies this suite.
        for forbidden in ("authorization", "proxy-authorization", "cookie"):
            if forbidden in headers:
                raise ProbeFailure(f"origin-{forbidden}")
        lowered = request_raw.lower()
        if b"authorization" in lowered or b"bearer " in lowered:
            raise ProbeFailure("origin-credential-bytes")

        expected = f"client_id={CLIENT_ID}"
        if endpoint_of(self.scenario) == "token":
            expected += (
                f"&device_code={self.transients.device_code}"
                f"&grant_type={ENCODED_GRANT_TYPE}"
            )
        if body != expected.encode("ascii"):
            raise ProbeFailure("origin-form-body")

    def _respond(self, tls: ssl.SSLSocket) -> None:
        target = target_of(self.scenario)
        if self.course == "cross-redirect":
            tls.sendall(
                response_bytes(
                    302,
                    "Found",
                    b"",
                    (("Location", f"https://redirect.bpr.invalid{target}"),),
                )
            )
            return
        if self.course == "query-redirect":
            tls.sendall(
                response_bytes(
                    302,
                    "Found",
                    b"",
                    (("Location", f"https://{HOST}{target}?retry=1"),),
                )
            )
            return
        if self.course == "response-limit":
            # One byte past the device bound, declared and never sent: the
            # refusal has to land before any response body is transmitted.
            tls.sendall(
                response_bytes(
                    200,
                    "OK",
                    b"",
                    declared_length=RESPONSE_LIMIT + 1,
                )
            )
            return
        if self.course == "cancel":
            tls.sendall(
                response_bytes(
                    200,
                    "OK",
                    b'{"device',
                    declared_length=512,
                )
            )
            self._stall(tls)
            return
        if self.course == "disabled":
            tls.sendall(
                response_bytes(
                    400,
                    "Bad Request",
                    json_body(
                        {
                            "error": "device_flow_disabled",
                            "error_description": (
                                "Device flow is not enabled for this client."
                            ),
                            "error_uri": ERROR_URI,
                        }
                    ),
                )
            )
            return
        if self.course == "slow-down":
            tls.sendall(
                response_bytes(
                    200,
                    "OK",
                    json_body(
                        {
                            "error": "slow_down",
                            "error_description": (
                                "Too many requests have been made."
                            ),
                            "error_uri": ERROR_URI,
                            "interval": SLOW_DOWN_INTERVAL,
                        }
                    ),
                )
            )
            return
        tls.sendall(
            response_bytes(
                200,
                "OK",
                success_payload(self.scenario, self.transients),
            )
        )

    def _stall(self, tls: ssl.SSLSocket) -> None:
        deadline = time.monotonic() + STALL_LIMIT
        tls.settimeout(0.1)
        while not self.stop.is_set() and time.monotonic() < deadline:
            try:
                if tls.recv(1) == b"":
                    return
            except socket.timeout:
                continue
            except (OSError, ssl.SSLError):
                return

    def verify(self) -> None:
        with self.observation.lock:
            connects = self.observation.connects
            handshakes = self.observation.handshakes
            failures = self.observation.handshake_failures
            origins = self.observation.origins
            sni = list(self.observation.sni)
            violations = set(self.observation.violations)

        if violations:
            raise ProbeFailure(
                f"{self.scenario}-violations:" + ",".join(sorted(violations))
            )
        if sni != [HOST]:
            raise ProbeFailure(f"{self.scenario}-sni")
        if connects != 1:
            raise ProbeFailure(f"{self.scenario}-connects={connects}")
        if self.course == "untrusted-certificate":
            if origins != 0 or (handshakes, failures) not in {(1, 0), (0, 1)}:
                raise ProbeFailure(f"{self.scenario}-untrusted-observation")
            return
        if handshakes != 1 or failures != 0 or origins != 1:
            raise ProbeFailure(
                f"{self.scenario}-observation:"
                f"handshakes={handshakes},failures={failures},"
                f"origins={origins}"
            )


def frame(value: bytes) -> bytes:
    return struct.pack(">I", len(value)) + value


def child_input(authority_pem: bytes, material: Material) -> bytes:
    framed = bytearray(frame(authority_pem))
    for value in material.transient_values():
        framed.extend(frame(value))
    return bytes(framed)


def run_scenario(
    executable: Path,
    certificate: Certificates,
    scenario: str,
    port: int,
) -> None:
    material = Material(
        device_code=secrets.token_urlsafe(32),
        user_code=secrets.token_urlsafe(6),
        access_token=secrets.token_urlsafe(32),
        refresh_token=secrets.token_urlsafe(32),
    )
    with DeviceAuthorizationProxy(
        certificate,
        scenario,
        material,
        port,
    ) as proxy:
        child = subprocess.run(
            [str(executable), "--fixture", scenario, str(proxy.port)],
            input=child_input(
                certificate.authority.read_bytes(),
                material,
            ),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=CHILD_TIMEOUT,
            check=False,
        )
        time.sleep(0.2)

    if len(child.stdout) > CHILD_OUTPUT_LIMIT or (
        len(child.stderr) > CHILD_OUTPUT_LIMIT
    ):
        raise ProbeFailure(f"{scenario}-child-output-limit")
    for transient in material.transient_values():
        if transient in child.stdout or transient in child.stderr:
            raise ProbeFailure(f"{scenario}-transient-leak")
    expected = f"{MARKER}:{scenario}:pass\n".encode("ascii")
    if child.returncode != 0 or child.stdout != expected or child.stderr:
        raise ProbeFailure(
            f"{scenario}-child:"
            + child.stderr[:2048].decode("utf-8", errors="replace")
            + ":violations="
            + ",".join(sorted(proxy.observation.violations))
        )
    proxy.verify()


def executable_path(value: str) -> Path:
    candidate = Path(value)
    if not candidate.is_absolute():
        raise ProbeFailure("non-absolute-executable")
    try:
        resolved = candidate.resolve(strict=True)
    except OSError as error:
        raise ProbeFailure("unavailable-executable") from error
    if not resolved.is_file() or not os.access(resolved, os.X_OK):
        raise ProbeFailure("unusable-executable")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Verify the BetterPullReview native HTTPS device authorization "
            "path against a disposable loopback CONNECT and TLS fixture."
        )
    )
    parser.add_argument("--executable", required=True)
    parser.add_argument("--openssl", required=True)
    arguments = parser.parse_args()
    try:
        target = executable_path(arguments.executable)
        openssl = executable_path(arguments.openssl)
        with contained_temporary_directory(prefix="bpr0278-device-") as value:
            directory = Path(value)
            directory.chmod(0o700)
            certificate = certificates(openssl, directory)
            port = configured_loopback_port()
            for scenario in SCENARIOS:
                run_scenario(target, certificate, scenario, port)
    except ProbeFailure as error:
        print(f"{MARKER}:fail:{error.code}", file=sys.stderr)
        return 1
    except Exception as error:
        print(
            f"{MARKER}:fail:internal:{type(error).__name__}",
            file=sys.stderr,
        )
        return 1

    print(f"{MARKER}:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
