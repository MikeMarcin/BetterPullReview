#!/usr/bin/env python3
from __future__ import annotations

import sys
import argparse
from dataclasses import dataclass
import hashlib
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import re
import secrets
import shutil
import socket
import ssl
import stat
import subprocess
import tempfile
import threading
import time
from typing import Callable


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory, make_contained_temporary_directory  # noqa: E402

PROCESS_TIMEOUT_SECONDS = 10
MAXIMUM_VERSION_OUTPUT_BYTES = 4 * 1024


class ProbeFailure(RuntimeError):
    pass


def short_unix_socket_root() -> Path:
    if os.name != "posix" or not hasattr(socket, "AF_UNIX"):
        raise ProbeFailure(
            "the Unix-socket trust probe requires macOS or Linux"
        )
    conventional_root = Path("/tmp")
    if conventional_root.is_dir():
        return conventional_root
    return Path(tempfile.gettempdir())


@dataclass
class Observation:
    tcp_accepts: int = 0
    tls_failures: int = 0
    tls_bad_certificate_alerts: int = 0
    tls_other_failures: int = 0
    http_requests: int = 0
    authorization_requests: int = 0
    exact_authorization_requests: int = 0
    body_bytes: int = 0
    exact_body_requests: int = 0


@dataclass
class UnixSocketObservation:
    accepts: int = 0
    http_requests: int = 0
    authorization_requests: int = 0
    exact_authorization_requests: int = 0


@dataclass(frozen=True)
class RedirectProbeResult:
    status: int
    origin: Observation
    sink: Observation
    return_code: int


class QuietHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format: str, *args: object) -> None:
        del format, args


class ObservedTlsServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(
        self,
        handler: type[BaseHTTPRequestHandler],
        context: ssl.SSLContext,
        observation: Observation,
    ) -> None:
        super().__init__(("127.0.0.1", 0), handler)
        self._context = context
        self._observation = observation

    def get_request(self) -> tuple[socket.socket, object]:
        connection, address = super().get_request()
        self._observation.tcp_accepts += 1
        try:
            secured = self._context.wrap_socket(
                connection,
                server_side=True,
            )
        except ssl.SSLError as error:
            self._observation.tls_failures += 1
            if getattr(error, "reason", None) == "SSLV3_ALERT_BAD_CERTIFICATE":
                self._observation.tls_bad_certificate_alerts += 1
            else:
                self._observation.tls_other_failures += 1
            connection.close()
            raise
        return secured, address

    def handle_error(
        self,
        request: object,
        client_address: object,
    ) -> None:
        del request, client_address


class ObservedHttpServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(
        self,
        handler: type[BaseHTTPRequestHandler],
        observation: Observation,
    ) -> None:
        super().__init__(("127.0.0.1", 0), handler)
        self._observation = observation

    def get_request(self) -> tuple[socket.socket, object]:
        connection, address = super().get_request()
        self._observation.tcp_accepts += 1
        return connection, address


def start_unix_socket_sentinel(
    path: Path,
    observation: UnixSocketObservation,
    expected_authorization: str,
) -> tuple[socket.socket, threading.Event, threading.Thread]:
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    listener.bind(str(path))
    listener.listen()
    listener.settimeout(0.1)
    stopped = threading.Event()

    def observe() -> None:
        while not stopped.is_set():
            try:
                connection, _ = listener.accept()
            except TimeoutError:
                continue
            except OSError:
                return
            observation.accepts += 1
            try:
                connection.settimeout(PROCESS_TIMEOUT_SECONDS)
                request = bytearray()
                while b"\r\n\r\n" not in request:
                    block = connection.recv(4096)
                    if not block:
                        break
                    request.extend(block)
                    if len(request) > 64 * 1024:
                        break
                if b"\r\n\r\n" in request:
                    observation.http_requests += 1
                    for line in request.split(b"\r\n")[1:]:
                        if b":" not in line:
                            continue
                        name, value = line.split(b":", 1)
                        if name.lower() != b"authorization":
                            continue
                        observation.authorization_requests += 1
                        if hmac.compare_digest(
                            value.strip(),
                            expected_authorization.encode("utf-8"),
                        ):
                            observation.exact_authorization_requests += 1
                    connection.sendall(
                        b"HTTP/1.1 200 OK\r\n"
                        b"Content-Type: application/json\r\n"
                        b"Content-Length: 2\r\n"
                        b"Connection: close\r\n"
                        b"\r\n"
                        b"{}"
                    )
            finally:
                connection.close()

    thread = threading.Thread(target=observe, daemon=True)
    thread.start()
    return listener, stopped, thread


def stop_unix_socket_sentinel(
    listener: socket.socket,
    stopped: threading.Event,
    thread: threading.Thread,
    path: Path,
) -> None:
    stopped.set()
    listener.close()
    thread.join(timeout=PROCESS_TIMEOUT_SECONDS)
    if thread.is_alive():
        raise ProbeFailure("the Unix-socket sentinel did not stop")
    path.unlink(missing_ok=True)


def canonical_executable(value: str) -> Path:
    candidate = Path(value)
    if not candidate.is_absolute():
        raise ProbeFailure("the selected gh executable must be absolute")
    try:
        selected = candidate.resolve(strict=True)
    except OSError as error:
        raise ProbeFailure("the selected gh executable is unavailable") from error
    if not selected.is_file() or not os.access(selected, os.X_OK):
        raise ProbeFailure("the selected gh executable is not executable")
    if selected.name not in {"gh", "gh.exe"}:
        raise ProbeFailure("the selected executable is not named gh")
    return selected


def executable_sha256(executable: Path) -> str:
    digest = hashlib.sha256()
    with executable.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def has_repository_ancestor(directory: Path) -> bool:
    for candidate in (directory, *directory.parents):
        if (candidate / ".git").exists():
            return True
        if (
            (candidate / "HEAD").is_file()
            and (candidate / "objects").is_dir()
            and (candidate / "refs").is_dir()
        ):
            return True
    return False


def copy_selected_executable(
    executable: Path,
    directory: Path,
) -> tuple[Path, str]:
    copied = directory / executable.name
    flags = os.O_RDONLY
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(executable, flags)
    digest = hashlib.sha256()
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise ProbeFailure("the selected gh executable is not regular")
        with (
            os.fdopen(descriptor, "rb", closefd=False) as source,
            copied.open("xb") as destination,
        ):
            while block := source.read(1024 * 1024):
                destination.write(block)
                digest.update(block)
    finally:
        os.close(descriptor)
    copied.chmod(0o500)
    return copied, digest.hexdigest()


def bounded_standard_output(
    arguments: list[str],
    environment: dict[str, str],
    working_directory: Path,
    limit: int,
) -> tuple[int, bytes]:
    process = subprocess.Popen(
        arguments,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        env=environment,
        cwd=working_directory,
    )
    if process.stdout is None:
        process.kill()
        process.wait(timeout=PROCESS_TIMEOUT_SECONDS)
        raise ProbeFailure("the selected process output was unavailable")

    output = bytearray()
    exceeded = threading.Event()

    def read_output() -> None:
        while block := process.stdout.read(1024):
            if len(output) + len(block) > limit:
                exceeded.set()
                return
            output.extend(block)

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    deadline = time.monotonic() + PROCESS_TIMEOUT_SECONDS
    timed_out = False
    while process.poll() is None:
        if exceeded.is_set():
            process.kill()
            break
        if time.monotonic() >= deadline:
            timed_out = True
            process.kill()
            break
        exceeded.wait(0.01)
    try:
        return_code = process.wait(timeout=PROCESS_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as error:
        process.kill()
        process.wait(timeout=PROCESS_TIMEOUT_SECONDS)
        raise ProbeFailure("the selected process could not be reaped") from error
    reader.join(timeout=PROCESS_TIMEOUT_SECONDS)
    if reader.is_alive():
        raise ProbeFailure("the selected process output could not be drained")
    if timed_out:
        raise ProbeFailure("the selected process exceeded its deadline")
    if exceeded.is_set():
        raise ProbeFailure("the selected process exceeded its output bound")
    return return_code, bytes(output)


def selected_version(executable: Path, directory: Path) -> str:
    state = directory / "state"
    state.mkdir(mode=0o700)
    return_code, output = bounded_standard_output(
        [str(executable), "--version"],
        {
            "HOME": str(directory),
            "XDG_STATE_HOME": str(state),
            "GH_TELEMETRY": "0",
            "NO_COLOR": "1",
        },
        directory,
        MAXIMUM_VERSION_OUTPUT_BYTES,
    )
    if return_code != 0:
        raise ProbeFailure("the selected gh version could not be identified")
    lines = output.decode("utf-8", errors="strict").splitlines()
    if not lines:
        raise ProbeFailure("the selected gh version output was empty")
    first_line = lines[0]
    match = re.fullmatch(r"gh version ([0-9]+\.[0-9]+\.[0-9]+).*", first_line)
    if match is None:
        raise ProbeFailure("the selected gh version output is not canonical")
    return match.group(1)


def generate_certificate(openssl: Path, directory: Path) -> tuple[Path, Path]:
    certificate = directory / "loopback-certificate.pem"
    private_key = directory / "loopback-private-key.pem"
    command = [
        str(openssl),
        "req",
        "-x509",
        "-newkey",
        "rsa:2048",
        "-sha256",
        "-nodes",
        "-days",
        "1",
        "-subj",
        "/CN=bpr0025-loopback",
        "-addext",
        "subjectAltName=IP:127.0.0.1,DNS:localhost",
        "-addext",
        "basicConstraints=critical,CA:TRUE",
        "-keyout",
        str(private_key),
        "-out",
        str(certificate),
    ]
    result = subprocess.run(
        command,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
        timeout=PROCESS_TIMEOUT_SECONDS,
        env={},
        cwd=directory,
    )
    if result.returncode != 0:
        raise ProbeFailure("the loopback certificate could not be created")
    private_key.chmod(0o600)
    certificate.chmod(0o600)
    return certificate, private_key


def tls_context(certificate: Path, private_key: Path) -> ssl.SSLContext:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(certificate, private_key)
    return context


def record_request(
    handler: BaseHTTPRequestHandler,
    observation: Observation,
    expected_body: bytes | None,
    expected_authorization: str | None,
) -> None:
    maximum_body_bytes = 256 * 1024
    body_digest = hashlib.sha256()
    transfer_encoding = handler.headers.get("Transfer-Encoding")
    content_length_text = handler.headers.get("Content-Length", "0")
    try:
        content_length = int(content_length_text)
    except ValueError as error:
        raise ProbeFailure("the loopback request length was malformed") from error
    if content_length < 0 or content_length > maximum_body_bytes:
        raise ProbeFailure("the loopback request exceeded its body bound")
    if transfer_encoding is not None:
        if transfer_encoding.lower() != "chunked" or content_length != 0:
            raise ProbeFailure("the loopback request framing was unsupported")
        content_length = 0
        while True:
            size_line = handler.rfile.readline(129)
            if not size_line or len(size_line) > 128 or not size_line.endswith(b"\r\n"):
                raise ProbeFailure("the loopback chunk size was malformed")
            raw_size = size_line[:-2].split(b";", 1)[0]
            try:
                chunk_size = int(raw_size, 16)
            except ValueError as error:
                raise ProbeFailure("the loopback chunk size was invalid") from error
            if (
                chunk_size < 0
                or content_length + chunk_size > maximum_body_bytes
            ):
                raise ProbeFailure("the loopback chunked body exceeded its bound")
            if chunk_size == 0:
                if handler.rfile.read(2) != b"\r\n":
                    raise ProbeFailure("the loopback chunk trailer was malformed")
                break
            remaining = chunk_size
            while remaining:
                block = handler.rfile.read(min(remaining, 16 * 1024))
                if not block:
                    raise ProbeFailure("the loopback chunk was truncated")
                body_digest.update(block)
                remaining -= len(block)
            if handler.rfile.read(2) != b"\r\n":
                raise ProbeFailure("the loopback chunk terminator was malformed")
            content_length += chunk_size
    elif content_length:
        remaining = content_length
        while remaining:
            block = handler.rfile.read(min(remaining, 16 * 1024))
            if not block:
                raise ProbeFailure("the loopback request body was truncated")
            body_digest.update(block)
            remaining -= len(block)
    observation.http_requests += 1
    observation.body_bytes += content_length
    if (
        expected_body is not None
        and content_length == len(expected_body)
        and hmac.compare_digest(
            body_digest.digest(),
            hashlib.sha256(expected_body).digest(),
        )
    ):
        observation.exact_body_requests += 1
    authorization = handler.headers.get("Authorization")
    if authorization is not None:
        observation.authorization_requests += 1
        if (
            expected_authorization is not None
            and hmac.compare_digest(
                authorization.encode("utf-8"),
                expected_authorization.encode("utf-8"),
            )
        ):
            observation.exact_authorization_requests += 1


def response_handler(
    observation: Observation,
    status: int,
    location: Callable[[], str] | None = None,
    expected_body: bytes | None = None,
    expected_authorization: str | None = None,
) -> type[QuietHandler]:
    class Handler(QuietHandler):
        def respond(self) -> None:
            record_request(
                self,
                observation,
                expected_body,
                expected_authorization,
            )
            self.send_response(status)
            if location is not None:
                self.send_header("Location", location())
            self.send_header("Content-Length", "0")
            self.end_headers()

        def do_GET(self) -> None:
            self.respond()

        def do_POST(self) -> None:
            self.respond()

    return Handler


def start_server(server: ThreadingHTTPServer) -> threading.Thread:
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return thread


def stop_server(server: ThreadingHTTPServer, thread: threading.Thread) -> None:
    server.shutdown()
    server.server_close()
    thread.join(timeout=PROCESS_TIMEOUT_SECONDS)
    if thread.is_alive():
        raise ProbeFailure("a loopback fixture did not stop")


def isolated_config(
    directory: Path,
    token: str,
    unix_socket: str,
    name: str = "gh-config",
) -> Path:
    config = directory / name
    config.mkdir(mode=0o700)
    general = config / "config.yml"
    general.write_text(
        f"http_unix_socket: {unix_socket}\n",
        encoding="utf-8",
    )
    general.chmod(0o600)
    hosts = config / "hosts.yml"
    hosts.write_text(
        "github.com:\n"
        "    user: bpr0025-fixture\n"
        f"    oauth_token: {token}\n"
        "    git_protocol: https\n",
        encoding="utf-8",
    )
    hosts.chmod(0o600)
    return config


def require_configured_unix_socket(
    executable: Path,
    config: Path,
    expected: str,
) -> None:
    return_code, output = bounded_standard_output(
        [
            str(executable),
            "config",
            "get",
            "http_unix_socket",
        ],
        gh_environment(config),
        config.parent,
        MAXIMUM_VERSION_OUTPUT_BYTES,
    )
    if return_code != 0 or output.decode("utf-8").strip() != expected:
        raise ProbeFailure(
            "the selected gh did not load the Unix-socket sentinel setting"
        )


def authorize_fixture_host(config: Path, host: str, token: str) -> None:
    if not re.fullmatch(r"127\.0\.0\.1(?::[0-9]{1,5})?", host):
        raise ProbeFailure("the synthetic fixture authority is invalid")
    hosts = config / "hosts.yml"
    with hosts.open("a", encoding="utf-8") as destination:
        destination.write(
            f'"{host}":\n'
            "    user: bpr0025-fixture\n"
            f"    oauth_token: {token}\n"
            "    git_protocol: https\n"
        )
    hosts.chmod(0o600)


def gh_environment(config: Path) -> dict[str, str]:
    state = config.parent / "state"
    state.mkdir(mode=0o700, exist_ok=True)
    environment = {
        "HOME": str(config.parent),
        "XDG_STATE_HOME": str(state),
        "GH_CONFIG_DIR": str(config),
        "GH_PROMPT_DISABLED": "1",
        "GH_NO_UPDATE_NOTIFIER": "1",
        "GH_NO_EXTENSION_UPDATE_NOTIFIER": "1",
        "GH_SPINNER_DISABLED": "1",
        "GH_TELEMETRY": "0",
        "NO_COLOR": "1",
        "CLICOLOR": "0",
    }
    return environment


def run_gh_api(
    executable: Path,
    endpoint: str,
    hostname: str,
    environment: dict[str, str],
    method: str,
    body: bytes = b"",
) -> int:
    command = [
        str(executable),
        "api",
        endpoint,
        "--hostname",
        hostname,
        "--method",
        method,
    ]
    if body:
        command.extend(["--input", "-"])
    result = subprocess.run(
        command,
        input=body if body else None,
        stdin=None if body else subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
        timeout=PROCESS_TIMEOUT_SECONDS,
        env=environment,
        cwd=Path(environment["GH_CONFIG_DIR"]).parent,
    )
    return result.returncode


def certificate_probe(
    executable: Path,
    config: Path,
    token: str,
    certificate: Path,
    private_key: Path,
) -> Observation:
    observation = Observation()
    server = ObservedTlsServer(
        response_handler(observation, 200),
        tls_context(certificate, private_key),
        observation,
    )
    thread = start_server(server)
    try:
        endpoint = (
            f"https://127.0.0.1:{server.server_address[1]}"
            "/certificate-failure"
        )
        authority = f"127.0.0.1:{server.server_address[1]}"
        authorize_fixture_host(config, authority, token)
        return_code = run_gh_api(
            executable,
            endpoint,
            "github.com",
            gh_environment(config),
            "GET",
        )
    finally:
        stop_server(server, thread)
    if (
        return_code == 0
        or observation.tcp_accepts != 1
        or observation.tls_failures != 1
        or observation.tls_bad_certificate_alerts != 1
        or observation.tls_other_failures != 0
        or observation.http_requests != 0
    ):
        raise ProbeFailure(
            "the selected gh did not directly produce only a bad-certificate "
            "TLS alert before HTTP "
            f"(return-code={return_code}, "
            f"tcp={observation.tcp_accepts}, "
            f"bad-certificate={observation.tls_bad_certificate_alerts}, "
            f"other-tls={observation.tls_other_failures}, "
            f"http={observation.http_requests})"
        )
    return observation


def redirect_probe(
    executable: Path,
    config: Path,
    token: str,
    status: int,
) -> RedirectProbeResult:
    if status not in {302, 307, 308}:
        raise ProbeFailure("the selected redirect status is unsupported")
    body = b'{"query":"query Bpr0025TrustProbe{__typename}"}'
    expected_authorization = f"token {token}"
    sink_observation = Observation()
    sink = ObservedHttpServer(
        response_handler(sink_observation, 200),
        sink_observation,
    )
    sink_thread = start_server(sink)

    origin_observation = Observation()
    sink_location = lambda: (
        f"http://localhost:{sink.server_address[1]}"
        "/cross-authority-target"
    )
    origin = ObservedHttpServer(
        response_handler(
            origin_observation,
            status,
            sink_location,
            body,
            expected_authorization,
        ),
        origin_observation,
    )
    origin_thread = start_server(origin)
    origin_authority = f"127.0.0.1:{origin.server_address[1]}"
    authorize_fixture_host(config, origin_authority, token)
    try:
        endpoint = (
            f"http://{origin_authority}"
            f"/redirect-{status}-origin"
        )
        return_code = run_gh_api(
            executable,
            endpoint,
            "github.com",
            gh_environment(config),
            "POST",
            body,
        )
    finally:
        try:
            stop_server(origin, origin_thread)
        finally:
            stop_server(sink, sink_thread)

    if origin_observation.http_requests != 1:
        raise ProbeFailure(
            "the trusted redirect origin was not reached exactly once "
            f"(return-code={return_code}, "
            f"tcp={origin_observation.tcp_accepts}, "
            f"tls-failures={origin_observation.tls_failures}, "
            f"http={origin_observation.http_requests})"
        )
    if (
        origin_observation.authorization_requests != 1
        or origin_observation.exact_authorization_requests != 1
    ):
        raise ProbeFailure(
            "the exact disposable authorization did not reach only the "
            "selected origin"
        )
    if (
        origin_observation.body_bytes != len(body)
        or origin_observation.exact_body_requests != 1
    ):
        raise ProbeFailure(
            "the exact selected request body did not reach the origin"
        )
    if sink_observation.http_requests > 1:
        raise ProbeFailure("the cross-authority sink was reached more than once")
    if sink_observation.authorization_requests != 0:
        raise ProbeFailure("the disposable credential reached the redirect sink")
    if sink_observation.body_bytes != 0:
        raise ProbeFailure("a request body reached the redirect sink")
    return RedirectProbeResult(
        status=status,
        origin=origin_observation,
        sink=sink_observation,
        return_code=return_code,
    )


def redirect_result(observation: Observation) -> str:
    if observation.http_requests:
        return "followed"
    if observation.tcp_accepts:
        return "contacted-without-http"
    return "not-observed-unproven"


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Directly characterize one selected gh binary's ambient "
            "Unix-socket, certificate, and cross-authority redirect behavior "
            "using local-only fixtures."
        )
    )
    parser.add_argument(
        "--gh",
        required=True,
        help="absolute path to the selected gh executable",
    )
    parser.add_argument(
        "--openssl",
        default=shutil.which("openssl"),
        help="absolute path to openssl",
    )
    parser.add_argument("--expect-version")
    parser.add_argument("--expect-sha256")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        executable = canonical_executable(arguments.gh)
        if arguments.openssl is None:
            raise ProbeFailure("openssl is required for the loopback TLS fixture")
        openssl = Path(arguments.openssl).resolve(strict=True)

        with contained_temporary_directory(prefix="bpr0025-gh-trust-") as raw:
            directory = Path(raw)
            directory.chmod(0o700)
            if has_repository_ancestor(directory.resolve(strict=True)):
                raise ProbeFailure(
                    "the loopback fixture working directory is in a "
                    "repository"
                )
            probe_executable, digest = copy_selected_executable(
                executable,
                directory,
            )
            if (
                arguments.expect_sha256 is not None
                and digest != arguments.expect_sha256.lower()
            ):
                raise ProbeFailure("the selected gh executable digest changed")
            version = selected_version(probe_executable, directory)
            if (
                arguments.expect_version is not None
                and version != arguments.expect_version
            ):
                raise ProbeFailure("the selected gh version changed")
            disposable_token = "BPR0025_" + secrets.token_urlsafe(48)
            unix_socket_directory = Path(
                make_contained_temporary_directory(
                    prefix="bpr-gh-socket-",
                    parent=short_unix_socket_root(),
                )
            )
            unix_socket_path = unix_socket_directory / "u"
            unix_socket_value = str(unix_socket_path)
            unix_socket_observation = UnixSocketObservation()
            try:
                expected_authorization = f"token {disposable_token}"
                listener, stopped, unix_thread = start_unix_socket_sentinel(
                    unix_socket_path,
                    unix_socket_observation,
                    expected_authorization,
                )
                try:
                    socket_config = isolated_config(
                        directory,
                        disposable_token,
                        unix_socket_value,
                        "gh-socket-config",
                    )
                    require_configured_unix_socket(
                        probe_executable,
                        socket_config,
                        unix_socket_value,
                    )
                    unix_socket_return_code = run_gh_api(
                        probe_executable,
                        "user",
                        "github.com",
                        gh_environment(socket_config),
                        "GET",
                    )
                finally:
                    stop_unix_socket_sentinel(
                        listener,
                        stopped,
                        unix_thread,
                        unix_socket_path,
                    )
            finally:
                shutil.rmtree(unix_socket_directory)
            if (
                unix_socket_return_code != 0
                or unix_socket_observation.accepts != 1
                or unix_socket_observation.http_requests != 1
                or unix_socket_observation.authorization_requests != 1
                or unix_socket_observation.exact_authorization_requests != 1
            ):
                raise ProbeFailure(
                    "gh api did not route its exact authenticated request "
                    "through the configured Unix socket"
                )
            config = isolated_config(
                directory,
                disposable_token,
                "",
            )
            certificate, private_key = generate_certificate(openssl, directory)
            certificate_observation = certificate_probe(
                probe_executable,
                config,
                disposable_token,
                certificate,
                private_key,
            )
            redirect_observations = [
                redirect_probe(
                    probe_executable,
                    config,
                    disposable_token,
                    status,
                )
                for status in (302, 307, 308)
            ]
            if executable_sha256(probe_executable) != digest:
                raise ProbeFailure(
                    "the private gh executable changed during the probes"
                )

        for redirect_observation in redirect_observations:
            result = redirect_result(redirect_observation.sink)
            if (
                result == "followed"
                and redirect_observation.return_code != 0
            ):
                raise ProbeFailure(
                    "the selected gh followed a redirect but reported failure"
                )
        availability = "unavailable"
        print("gh-cli-trust-probe: completed")
        print(f"selected-version: {version}")
        print(f"selected-sha256: {digest}")
        print("certificate-rejection: bad-certificate-alert")
        print(
            "certificate-bad-certificate-alerts: "
            f"{certificate_observation.tls_bad_certificate_alerts}"
        )
        print(
            "certificate-http-requests: "
            f"{certificate_observation.http_requests}"
        )
        print("http-unix-socket-config-loaded: yes")
        print(
            "http-unix-socket-api-accepts: "
            f"{unix_socket_observation.accepts}"
        )
        print(
            "http-unix-socket-api-http-requests: "
            f"{unix_socket_observation.http_requests}"
        )
        unix_socket_authorization_exact = (
            unix_socket_observation.exact_authorization_requests == 1
        )
        print(
            "http-unix-socket-api-authorization-exact: "
            f"{'yes' if unix_socket_authorization_exact else 'no'}"
        )
        for redirect_observation in redirect_observations:
            status = redirect_observation.status
            origin = redirect_observation.origin
            sink = redirect_observation.sink
            print(
                f"cross-authority-redirect-{status}-result: "
                f"{redirect_result(sink)}"
            )
            print(
                f"redirect-{status}-origin-body-bytes: "
                f"{origin.body_bytes}"
            )
            print(
                f"redirect-{status}-origin-body-exact: "
                f"{'yes' if origin.exact_body_requests else 'no'}"
            )
            print(
                f"redirect-{status}-origin-authorization: "
                f"{'yes' if origin.authorization_requests else 'no'}"
            )
            print(
                f"redirect-{status}-origin-authorization-exact: "
                f"{'yes' if origin.exact_authorization_requests else 'no'}"
            )
            print(
                f"cross-authority-{status}-sink-requests: "
                f"{sink.http_requests}"
            )
            print(
                f"cross-authority-{status}-sink-body-bytes: "
                f"{sink.body_bytes}"
            )
            print(
                f"cross-authority-{status}-sink-authorization: "
                f"{'yes' if sink.authorization_requests else 'no'}"
            )
        print(f"broker-availability: {availability}")
        return 0
    except (OSError, UnicodeError, subprocess.SubprocessError, ProbeFailure) as error:
        print(f"gh-cli-trust-probe: failed: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
