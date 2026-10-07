#!/usr/bin/env python3
"""[[BPR-1302]] The real `bpr` through the real remote reader against a loopback GitHub.

The fixture courses swap the remote reader for a fixture, so nothing exercised
the live reader's waits, and every online command waited on a predicate that
is true only after shutdown. This course runs the built `bpr` itself:

* Its native exchanges go through the GitHub adapter's existing test seam, one
  loopback CONNECT proxy on 127.0.0.1 and one disposable fixture authority
  (`BPR_GITHUB_NATIVE_HTTPS_TEST_LOOPBACK`, present only in test-enabled
  builds). Behind the proxy this course answers as github.com and
  api.github.com: the device flow, `/user`, the nine capability parts, the
  contract fixture's pull request read, and Git's smart HTTP protocol v2 from a
  real `git upload-pack` over the cli-read-v1 objects.
* On Linux the vault is the Secret Service on a private session bus with a
  test-only keyring, the harness the linux_secret_service_* rows use.
  Nothing reaches the user's own keyring or the network.
* On macOS the gh transport runs a fake `gh` from a temporary PATH
  (`rfc0015_cli_read_fixture.py`).

Every step is one `bpr` process under a bounded wait. A step that does not
return is interrupted, reported as `timeout` with the requests the fake saw,
and the course goes on to the next step, so a regression fails in seconds at
each path instead of hanging ctest.

`--seed-binary` names a different `bpr` for the preparation only: the sign-in
and saved target the paths start from, and the local edits between them. The
failing-before controls use it: the reverted build is under test while a good
build prepares the profile, so each path is reached and times out on its own.
"""

from __future__ import annotations

import argparse
import errno
import json
import os
from pathlib import Path
import re
import secrets
import select
import signal
import socket
import ssl
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "end_to_end"))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

PREFIX = "tour-cli-loopback"
PULL_REQUEST = "https://github.com/octo-org/review-repository/pull/17"
REPOSITORY = "/octo-org/review-repository.git"
CLIENT_ID = "Iv23liitiYN1WSSbag50"
STEP_SECONDS = 30.0
INTERRUPT_SECONDS = 10.0
SOCKET_SECONDS = 10.0
HEADER_LIMIT = 64 * 1024
BODY_LIMIT = 1024 * 1024
AUTHORITIES = ("github.com:443", "api.github.com:443")
CAPABILITY_PART = re.compile(r"BetterPullReviewCapabilityProbeV1Part([0-8])")
GRAPHQL_OPERATION = re.compile(r"\s*(query|mutation)\s+([A-Za-z_][A-Za-z0-9_]*)")


class CourseFailure(RuntimeError):
    pass


def say(line: str) -> None:
    print(line, flush=True)


def openssl(executable: str, arguments: list[str], directory: Path) -> None:
    result = subprocess.run([executable, *arguments], cwd=directory, stdin=subprocess.DEVNULL,
                            capture_output=True, timeout=30, check=False)
    if result.returncode != 0:
        raise CourseFailure("certificate-generation")


def certificates(executable: str, directory: Path) -> tuple[Path, Path, Path]:
    """A disposable authority and one leaf for github.com and api.github.com."""
    directory.mkdir(mode=0o700)
    (directory / "authority.cnf").write_text(
        "[req]\ndistinguished_name = dn\nx509_extensions = ext\nprompt = no\n"
        "[dn]\nCN = BetterPullReview BPR-1302 loopback fixture authority\n"
        "[ext]\nbasicConstraints = critical,CA:TRUE,pathlen:0\nkeyUsage = critical,keyCertSign,cRLSign\n"
        "subjectKeyIdentifier = hash\nauthorityKeyIdentifier = keyid:always\n", encoding="ascii")
    (directory / "leaf.cnf").write_text(
        "[req]\ndistinguished_name = dn\nreq_extensions = ext\nprompt = no\n"
        "[dn]\nCN = BetterPullReview BPR-1302 loopback GitHub\n"
        "[ext]\nsubjectAltName = DNS:github.com,DNS:api.github.com\nbasicConstraints = critical,CA:FALSE\n"
        "keyUsage = critical,digitalSignature,keyEncipherment\nextendedKeyUsage = serverAuth\n"
        "subjectKeyIdentifier = hash\n", encoding="ascii")
    openssl(executable, ["req", "-x509", "-newkey", "rsa:2048", "-sha256", "-nodes", "-days", "1",
                         "-config", "authority.cnf", "-keyout", "authority-key.pem", "-out", "authority.pem"], directory)
    openssl(executable, ["req", "-new", "-newkey", "rsa:2048", "-sha256", "-nodes", "-config", "leaf.cnf",
                         "-keyout", "leaf-key.pem", "-out", "leaf.csr"], directory)
    openssl(executable, ["x509", "-req", "-sha256", "-days", "1", "-in", "leaf.csr", "-CA", "authority.pem",
                         "-CAkey", "authority-key.pem", "-CAcreateserial", "-extfile", "leaf.cnf",
                         "-extensions", "ext", "-out", "leaf.pem"], directory)
    for name in ("authority-key.pem", "leaf-key.pem"):
        (directory / name).chmod(0o600)
    return directory / "authority.pem", directory / "leaf.pem", directory / "leaf-key.pem"


def configured_port() -> int:
    raw = os.environ.get("BPR_TEST_LOOPBACK_PORTS", "")
    ports = [int(value) for value in raw.split(",") if value]
    return ports[0] if ports else 0


def pkt_line(text: bytes) -> bytes:
    return b"%04x" % (len(text) + 4) + text


class FakeGitHub:
    """A loopback CONNECT proxy that answers as github.com and api.github.com."""

    def __init__(self, leaf: Path, key: Path, fixture: dict, repository: Path) -> None:
        self.fixture = fixture
        self.repository = repository
        self.device_code = secrets.token_urlsafe(24)
        self.user_code = "BPRL-" + secrets.token_hex(2).upper()
        self.access_token = secrets.token_urlsafe(32)
        self.refresh_token = secrets.token_urlsafe(32)
        self.lock = threading.Lock()
        self.requests: list[str] = []
        self.violations: list[str] = []
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version = ssl.TLSVersion.TLSv1_2
        self.context.set_alpn_protocols(["http/1.1"])
        self.context.load_cert_chain(leaf, key)
        # Each handshake runs on its connection's own thread, so the name the
        # client sent is kept per thread and compared with the CONNECT authority.
        self.handshake = threading.local()
        self.context.sni_callback = self._observe_server_name
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", configured_port()))
        self.listener.listen(16)
        self.listener.settimeout(0.1)
        self.port = int(self.listener.getsockname()[1])
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self._accept, daemon=True)

    def _observe_server_name(self, _socket: ssl.SSLObject, name: str | None, _context: ssl.SSLContext) -> None:
        self.handshake.name = name

    def __enter__(self) -> FakeGitHub:
        self.thread.start()
        return self

    def __exit__(self, *_: object) -> None:
        self.stop.set()
        self.listener.close()
        self.thread.join(timeout=SOCKET_SECONDS)

    def mark(self) -> int:
        with self.lock:
            return len(self.requests)

    def since(self, mark: int) -> list[str]:
        with self.lock:
            return list(self.requests[mark:])

    def violate(self, reason: str) -> None:
        with self.lock:
            self.violations.append(reason)

    def record(self, entry: str) -> None:
        with self.lock:
            self.requests.append(entry)

    def _accept(self) -> None:
        while not self.stop.is_set():
            try:
                connection, address = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            if address[0] != "127.0.0.1":
                self.violate("non-loopback-peer")
                connection.close()
                continue
            threading.Thread(target=self._connection, args=(connection,), daemon=True).start()

    def _connection(self, connection: socket.socket) -> None:
        try:
            connection.settimeout(SOCKET_SECONDS)
            head, rest = self._headers(connection)
            line, headers = self._parse(head)
            authority = line.split(" ")[1] if line.startswith("CONNECT ") else ""
            if authority not in AUTHORITIES or not line.endswith(" HTTP/1.1") or rest:
                self.violate(f"connect:{line[:80]}")
                return
            if "authorization" in headers or "proxy-authorization" in headers:
                self.violate("connect-credential")
                return
            connection.sendall(b"HTTP/1.1 200 Connection Established\r\nContent-Length: 0\r\n\r\n")
            self.handshake.name = None
            with self.context.wrap_socket(connection, server_side=True) as tls:
                tls.settimeout(SOCKET_SECONDS)
                if self.handshake.name != authority.split(":")[0]:
                    self.violate(f"sni:{self.handshake.name}:{authority}")
                    return
                self._serve(tls, authority.split(":")[0])
        except (OSError, ssl.SSLError, CourseFailure, ValueError) as error:
            if not self.stop.is_set() and not isinstance(error, (ConnectionError, socket.timeout)):
                self.violate(f"connection:{type(error).__name__}:{error}"[:160])
        finally:
            connection.close()

    @staticmethod
    def _headers(connection: socket.socket, initial: bytes = b"") -> tuple[bytes, bytes]:
        data = bytearray(initial)
        while b"\r\n\r\n" not in data:
            chunk = connection.recv(65536)
            if not chunk:
                raise ConnectionError("closed")
            data.extend(chunk)
            if len(data) > HEADER_LIMIT:
                raise CourseFailure("header-limit")
        marker = data.index(b"\r\n\r\n") + 4
        return bytes(data[:marker]), bytes(data[marker:])

    @staticmethod
    def _parse(head: bytes) -> tuple[str, dict[str, str]]:
        lines = head[:-4].decode("latin-1").split("\r\n")
        headers: dict[str, str] = {}
        for item in lines[1:]:
            name, _, value = item.partition(":")
            headers[name.strip().lower()] = value.strip()
        return lines[0], headers

    def _serve(self, tls: ssl.SSLSocket, host: str) -> None:
        pending = b""
        while True:
            try:
                head, pending = self._headers(tls, pending)
            except ConnectionError:
                return
            line, headers = self._parse(head)
            method, target, _ = line.split(" ", 2)
            length = int(headers.get("content-length", "0"))
            if length > BODY_LIMIT or "transfer-encoding" in headers:
                raise CourseFailure("request-body")
            body = bytearray(pending[:length])
            pending = pending[length:]
            while len(body) < length:
                chunk = tls.recv(min(65536, length - len(body)))
                if not chunk:
                    raise ConnectionError("truncated")
                body.extend(chunk)
            if headers.get("host", host).split(":")[0] != host:
                self.violate(f"host-header:{host}")
            response = self._answer(host, method, target, headers, bytes(body))
            tls.sendall(response)
            if headers.get("connection", "").lower() == "close":
                return

    def _answer(self, host: str, method: str, target: str, headers: dict[str, str], body: bytes) -> bytes:
        path = target.split("?", 1)[0]
        if host == "github.com" and path == "/login/device/code" and method == "POST":
            self.record("device-code")
            if f"client_id={CLIENT_ID}" not in body.decode("ascii", "replace"):
                self.violate("device-client")
            return self._json(200, {"device_code": self.device_code, "user_code": self.user_code,
                                    "verification_uri": "https://github.com/login/device",
                                    "expires_in": 899, "interval": 1})
        if host == "github.com" and path == "/login/oauth/access_token" and method == "POST":
            self.record("device-token")
            if self.device_code not in body.decode("ascii", "replace"):
                self.violate("device-token-code")
            return self._json(200, {"access_token": self.access_token, "token_type": "bearer", "scope": "",
                                    "expires_in": 28800, "refresh_token": self.refresh_token,
                                    "refresh_token_expires_in": 15897600})
        if host == "github.com" and path.startswith(REPOSITORY + "/"):
            return self._git(method, target, path, headers, body)
        if host != "api.github.com":
            self.record(f"unmatched:{host}:{method}:{path}")
            return self._json(404, {"message": "Not Found"})
        if headers.get("authorization", "") not in (f"Bearer {self.access_token}", f"token {self.access_token}"):
            self.violate(f"api-authorization:{method}:{path}")
        if method == "POST" and path == "/graphql":
            document = json.loads(body).get("query", "")
            part = CAPABILITY_PART.search(document)
            if part:
                self.record(f"graphql:capability-{part[1]}")
                return self._json(200, self.fixture["parts"][int(part[1])], "graphql")
            operation = GRAPHQL_OPERATION.match(document)
            if not operation or operation[1] != "query":
                # Any mutation is a remote write; this fake never performs one.
                self.record(f"write:graphql:{operation[2] if operation else 'unnamed'}")
                return self._json(404, {"message": "Not Found"}, "graphql")
            if operation[2] == "GetReviewData":
                self.record(f"graphql:{operation[2]}")
                return self._json(200, self.fixture["routes"]["/graphql"], "graphql")
            self.record(f"unmatched:graphql:{operation[2]}")
            return self._json(404, {"message": "Not Found"}, "graphql")
        if method != "GET":
            self.record(f"write:{method}:{path}")
            return self._json(404, {"message": "Not Found"})
        if path == "/repos/octo-org/review-repository/issues/17/timeline":
            self.record(f"GET {path}")
            return self._json(200, [], "core")
        if path in self.fixture["routes"]:
            self.record(f"GET {path}")
            resource = "search" if path == "/search/issues" else "core"
            return self._json(200, self.fixture["routes"][path], resource)
        self.record(f"unmatched:{method}:{path}")
        return self._json(404, {"message": "Not Found"})

    def _git(self, method: str, target: str, path: str, headers: dict[str, str], body: bytes) -> bytes:
        """Git's smart HTTP protocol v2, answered by the real git upload-pack."""
        if "version=2" not in headers.get("git-protocol", ""):
            self.violate(f"git-protocol:{path}")
        environment = {"PATH": "/usr/bin:/bin", "GIT_PROTOCOL": "version=2", "GIT_CONFIG_NOSYSTEM": "1",
                       "GIT_CONFIG_GLOBAL": "/dev/null", "HOME": str(self.repository.parent)}
        if method == "GET" and target == REPOSITORY + "/info/refs?service=git-upload-pack":
            self.record("git:discovery")
            advertisement = subprocess.run(["git", "upload-pack", "--stateless-rpc", "--http-backend-info-refs",
                                            str(self.repository)], env=environment, capture_output=True,
                                           timeout=SOCKET_SECONDS, check=True).stdout
            payload = pkt_line(b"# service=git-upload-pack\n") + b"0000" + advertisement
            return self._raw(200, payload, "application/x-git-upload-pack-advertisement")
        if method == "POST" and target == REPOSITORY + "/git-upload-pack":
            command = re.search(rb"command=([a-z-]+)", body)
            self.record("git:" + (command[1].decode() if command else "unknown"))
            result = subprocess.run(["git", "upload-pack", "--stateless-rpc", str(self.repository)], input=body,
                                    env=environment, capture_output=True, timeout=SOCKET_SECONDS, check=True).stdout
            return self._raw(200, result, "application/x-git-upload-pack-result")
        self.record(f"unmatched:git:{method}:{target}")
        return self._json(404, {"message": "Not Found"})

    @staticmethod
    def _raw(status: int, payload: bytes, content_type: str, extra: tuple[tuple[str, str], ...] = ()) -> bytes:
        reason = {200: "OK", 404: "Not Found"}.get(status, "OK")
        fields = [f"HTTP/1.1 {status} {reason}", f"Content-Type: {content_type}",
                  f"Content-Length: {len(payload)}", "Connection: close"]
        fields.extend(f"{name}: {value}" for name, value in extra)
        return ("\r\n".join(fields) + "\r\n\r\n").encode("ascii") + payload

    def _json(self, status: int, value: object, resource: str | None = None) -> bytes:
        extra: tuple[tuple[str, str], ...] = ()
        if resource is not None:
            extra = (("X-RateLimit-Limit", "5000"), ("X-RateLimit-Remaining", "4999"),
                     ("X-RateLimit-Reset", "1999999999"), ("X-RateLimit-Resource", resource))
        return self._raw(status, json.dumps(value).encode("utf-8"), "application/json; charset=utf-8", extra)


class Step:
    def __init__(self, name: str, code: int | None, output: str, seconds: float, timed_out: bool,
                 requests: list[str]) -> None:
        self.name, self.code, self.output, self.seconds = name, code, output, seconds
        self.timed_out, self.requests = timed_out, requests

    def json(self) -> dict:
        try:
            return json.loads(self.output)
        except ValueError as error:
            raise CourseFailure(f"{self.name}: not JSON: {self.output[:400]!r}") from error


class Course:
    def __init__(self, binary: str, seed: str, environment: dict[str, str], server: FakeGitHub | None,
                 seconds: float) -> None:
        self.binary, self.seed, self.environment, self.server = binary, seed, environment, server
        self.seconds = seconds
        self.failures: list[str] = []

    def mark(self) -> int:
        return self.server.mark() if self.server else 0

    def since(self, mark: int) -> list[str]:
        return self.server.since(mark) if self.server else []

    def run(self, name: str, arguments: list[str], seed: bool = False) -> Step:
        """One `bpr` process with no terminal, interrupted if it does not return in time."""
        mark = self.mark()
        started = time.monotonic()
        child = subprocess.Popen([self.seed if seed else self.binary, "tour", *arguments], env=self.environment,
                                 stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        timed_out = False
        try:
            stdout, stderr = child.communicate(timeout=self.seconds)
        except subprocess.TimeoutExpired:
            timed_out = True
            if os.name == "posix":
                child.send_signal(signal.SIGINT)
            else:
                child.terminate()
            try:
                stdout, stderr = child.communicate(timeout=INTERRUPT_SECONDS)
            except subprocess.TimeoutExpired:
                child.kill()
                stdout, stderr = child.communicate()
        output = stdout.decode("utf-8", "replace")
        if timed_out and stderr:
            output += "\n" + stderr.decode("utf-8", "replace")
        return Step(name, child.returncode, output, time.monotonic() - started, timed_out, self.since(mark))

    def terminal(self, name: str, arguments: list[str], seed: bool = False) -> Step:
        """One `bpr` process on a pseudo-terminal, as a person would run it."""
        import pty
        mark = self.mark()
        started = time.monotonic()
        master, slave = pty.openpty()
        child = subprocess.Popen([self.seed if seed else self.binary, "tour", *arguments], stdin=slave,
                                 stdout=slave, stderr=slave, env=self.environment)
        os.close(slave)
        output = bytearray()
        timed_out = False
        deadline = started + self.seconds
        try:
            while child.poll() is None or select.select([master], [], [], 0)[0]:
                if time.monotonic() > deadline and child.poll() is None and not timed_out:
                    timed_out = True
                    child.send_signal(signal.SIGINT)
                    deadline = time.monotonic() + INTERRUPT_SECONDS
                elif timed_out and time.monotonic() > deadline and child.poll() is None:
                    child.kill()
                if select.select([master], [], [], 0.05)[0]:
                    try:
                        part = os.read(master, 65536)
                    except OSError as error:
                        if error.errno == errno.EIO:
                            break
                        raise
                    if not part:
                        break
                    output.extend(part)
            child.wait(timeout=INTERRUPT_SECONDS)
        finally:
            os.close(master)
            if child.poll() is None:
                child.kill()
                child.wait()
        return Step(name, child.returncode, output.decode("utf-8", "replace"), time.monotonic() - started,
                    timed_out, self.since(mark))

    def check(self, step: Step, expected_code: int, condition: bool, detail: str = "") -> bool:
        if step.timed_out:
            self.failures.append(step.name)
            say(f"{PREFIX}:{step.name}:timeout:{step.seconds:.1f}s:requests={step.requests}")
            return False
        if step.code != expected_code or not condition:
            self.failures.append(step.name)
            say(f"{PREFIX}:{step.name}:FAILED:exit={step.code}:{detail}:requests={step.requests}")
            say(step.output[-2000:])
            return False
        say(f"{PREFIX}:{step.name}:pass:{step.seconds:.1f}s")
        return True


def error_code(step: Step) -> str | None:
    try:
        error = step.json().get("error")
    except CourseFailure:
        return None
    return error.get("code") if isinstance(error, dict) else None


def never_signed_in(course: Course, root: Path) -> None:
    """A profile that never signed in refuses at once; it waited forever before."""
    step = course.run("never-signed-in-open", ["open", PULL_REQUEST, "--json", "--data-root", str(root / "fresh")])
    course.check(step, 5, error_code(step) == "authentication_required" and not step.requests,
                 f"code={error_code(step)}")


READ = "GET /repos/octo-org/review-repository/pulls/17"
SOURCE_TEXT = "int review_line_1 = 21;\nint review_line_2 = 22;\n"
UNQUALIFIED = "operation qualification is still required"


def native_paths(course: Course, root: Path, server: FakeGitHub) -> None:
    """Device sign-in, then every online path, each through the real reader and vault."""
    data = root / "native"
    seeded = course.seed != course.binary

    def connect(name: str, directory: Path, seed: bool = False) -> Step:
        return course.terminal(name, ["connect", "--host", "github.com", "--transport", "native",
                                      "--data-root", str(directory)], seed=seed)

    def prepared(step: Step) -> Step:
        if step.code != 0:
            raise CourseFailure(f"the seed build could not prepare {step.name}: {step.output[-800:]}")
        return step

    if seeded:
        prepared(connect("seed-connect", data, seed=True))
    connected = connect("native-connect", root / "native-tested" if seeded else data)
    course.check(connected, 0, "Connected account 1001" in connected.output and server.user_code in connected.output
                 and "device-token" in connected.requests and "GET /user" in connected.requests, "connected")
    if any(token in connected.output for token in (server.access_token, server.refresh_token, server.device_code)):
        course.failures.append("native-connect-leak")
        say(f"{PREFIX}:native-connect:FAILED:transient material reached the terminal")

    if seeded:
        prepared(course.run("seed-open", ["open", PULL_REQUEST, "--json", "--data-root", str(data)], seed=True))
    opened = course.run("online-open", ["open", PULL_REQUEST, "--json", "--data-root", str(data)])
    if course.check(opened, 0, READ in opened.requests and opened.json().get("status") == "ok", "read"):
        target = opened.json()["data"]
    elif seeded:
        target = prepared(course.run("seed-open", ["open", PULL_REQUEST, "--json", "--data-root", str(data)],
                                     seed=True)).json()["data"]
    else:
        for name in ("first-pinned-source-read", "pinned-source-show", "refresh", "publish", "recover"):
            course.failures.append(name)
            say(f"{PREFIX}:{name}:blocked:no saved target")
        return
    select_draft = ["--profile", "1", "--draft", target["draft_id"], "--data-root", str(data)]
    field = root / "field.md"

    def revision() -> str:
        return course.run("status", ["status", *select_draft, "--json"], seed=True).json()["data"]["revision"]

    # The first pinned-source read: `add` verifies the range through a fresh epoch
    # host, which fetches the pinned commits over Git's smart protocol.
    field.write_text("Loopback stop comment\n", encoding="utf-8")
    add = ["add", "review.cpp:1-2", *select_draft, "--file", str(field), "--json"]
    added = course.run("first-pinned-source-read", [*add[:-1], "--expected-revision", revision(), "--json"])
    fetched = "git:fetch" in added.requests
    if not course.check(added, 0, fetched and added.json().get("status") == "ok", f"fetched={fetched}") and seeded:
        prepared(course.run("seed-add", [*add[:-1], "--expected-revision", revision(), "--json"], seed=True))
    shown = course.run("pinned-source-show", ["show", "stop:1", "--source", *select_draft, "--json"])
    source = shown.json().get("data", {}) if not shown.timed_out and shown.code == 0 else {}
    course.check(shown, 0, source.get("source_availability") == "available"
                 and source.get("source", {}).get("text") == SOURCE_TEXT, "source")

    # Local authoring completes the draft, so publication reaches its own gate.
    for name, text in (("problem", "Loopback problem\n"), ("solution", "Loopback solution\n")):
        field.write_text(text, encoding="utf-8")
        prepared(course.run(f"edit-{name}", ["edit", name, *select_draft, "--expected-revision", revision(),
                                             "--file", str(field), "--json"], seed=True))
    prepared(course.run("ready", ["ready", "1", *select_draft, "--expected-revision", revision(), "--json"],
                        seed=True))

    refreshed = course.run("refresh", ["preview", "--refresh", *select_draft, "--json"])
    status = refreshed.json().get("data", {}).get("publication_status", "") if refreshed.code == 0 else ""
    course.check(refreshed, 0, READ in refreshed.requests and UNQUALIFIED in status, f"status={status!r}")

    # Live Tour publication keeps the parent's unqualified host/transport gate: a
    # complete draft is read, its plan refused at that gate, and nothing written.
    published = course.terminal("publish", ["publish", *select_draft])
    writes = [entry for entry in published.requests if entry.startswith("write:")]
    course.check(published, 5, READ in published.requests and UNQUALIFIED in published.output and not writes
                 and "Type publish" not in published.output, f"writes={writes}")

    recovered = course.run("recover", ["recover", *select_draft, "--json"])
    state = recovered.json().get("data", {}).get("publication_state") if recovered.code == 0 else None
    course.check(recovered, 0, READ in recovered.requests and state == "unavailable", f"state={state}")
    writes = [entry for entry in server.requests if entry.startswith("write:")]
    if writes:
        course.failures.append("remote-writes")
        say(f"{PREFIX}:remote-writes:FAILED:{writes}")


def gh_paths(course: Course, root: Path) -> None:
    """macOS: the explicitly selected gh transport with a fake gh on a temporary PATH."""
    import rfc0015_cli_read_fixture as gh_fixture
    directory = root / "gh"
    prepared = gh_fixture.prepare(directory, "positive")
    environment = dict(course.environment)
    environment.pop("BPR_GITHUB_NATIVE_HTTPS_TEST_LOOPBACK", None)
    environment.update(HOME=str(directory / "home"), PATH=str(directory / "bin") + ":/usr/bin:/bin:/usr/sbin:/sbin")
    gh_course = Course(course.binary, course.seed, environment, None, course.seconds)
    data = root / "gh-data"
    calls = directory / "calls.jsonl"

    def operations() -> list[str]:
        return [json.loads(line)["operation"] for line in calls.read_text().splitlines()] if calls.exists() else []

    connected = gh_course.terminal("gh-connect", ["connect", "--host", "github.com", "--transport", "gh",
                                                  "--data-root", str(data)])
    course.check(connected, 0, "Connected account" in connected.output and "/user" in operations(),
                 f"gh={operations()}")
    opened = gh_course.run("gh-open", ["open", prepared["pull_request"], "--transport", "gh", "--json",
                                       "--data-root", str(data)])
    course.check(opened, 0, "/repos/octo-org/review-repository/pulls/17" in operations(), f"gh={operations()}")


def linux_environment(root: Path, server: FakeGitHub, authority: Path):
    import linux_secret_service_private_session as session
    bus = session.PrivateBus(session.tool("dbus-daemon"), root / "bus")
    keyring = session.start_keyring(session.tool("gnome-keyring-daemon"), session.tool("dbus-send"), bus,
                                    root / "keyring")
    environment = session.row_environment(root / "row", {
        "DBUS_SESSION_BUS_ADDRESS": bus.address,
        "BPR_GITHUB_NATIVE_HTTPS_TEST_LOOPBACK": f"{server.port}:{authority}",
        "QT_QPA_PLATFORM": "bpr-no-display",
        "NO_COLOR": "1",
    })
    return environment, bus, keyring, session


def gh_rows_selected(requested: bool) -> tuple[bool, str]:
    """Whether the gh rows run here, and why not when they do not."""
    if sys.platform != "darwin":
        return False, "the gh transport is composed on macOS only"
    if requested:
        return True, ""
    if os.environ.get("BPR_AUTOMATED_TEST_BOUNDARY"):
        # gh runs under its own sandbox-exec profile, which cannot be applied
        # inside the test runner's; the station command passes --gh outside it.
        return False, "gh's own sandbox-exec cannot start inside the test sandbox; run with --gh outside ctest"
    return True, ""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("binary")
    parser.add_argument("--openssl")
    parser.add_argument("--seed-binary")
    parser.add_argument("--gh", action="store_true", help="run the macOS gh rows (outside ctest)")
    parser.add_argument("--step-seconds", type=float, default=STEP_SECONDS)
    arguments = parser.parse_args()
    binary = str(Path(arguments.binary).resolve(strict=True))
    seed = str(Path(arguments.seed_binary).resolve(strict=True)) if arguments.seed_binary else binary
    gh_selected, gh_reason = gh_rows_selected(arguments.gh)
    if arguments.gh and not gh_selected:
        parser.error(f"--gh: {gh_reason}")
    failures: list[str] = []
    violations: list[str] = []
    unmatched: list[str] = []
    with contained_temporary_directory(prefix="bpr-tour-loopback-") as temporary:
        root = Path(temporary).resolve()
        if os.name != "posix":
            environment = {name: value for name, value in os.environ.items()
                           if not name.startswith(("GH_", "GITHUB_"))}
            environment.update(QT_QPA_PLATFORM="bpr-no-display", NO_COLOR="1")
            course = Course(binary, seed, environment, None, arguments.step_seconds)
            never_signed_in(course, root)
            say(f"{PREFIX}:native-paths:not-run:the vault here is the user's own; Linux runs them on a private "
                "Secret Service")
            say(f"{PREFIX}:gh-paths:not-run:{gh_reason}")
            failures = course.failures
        else:
            import rfc0015_cli_read_fixture as gh_fixture
            if not arguments.openssl:
                parser.error("--openssl is required on this platform")
            root.chmod(0o700)
            authority, leaf, key = certificates(arguments.openssl, root / "certificates")
            prepared = gh_fixture.prepare(root / "fixture", "positive")
            fixture_root = Path(prepared["directory"])
            fixture = json.loads((fixture_root / "fixture.json").read_text())
            repository = fixture_root / "repository.git"
            subprocess.run(["git", "--git-dir", str(repository), "config", "uploadpack.allowAnySHA1InWant", "true"],
                           check=True, capture_output=True)
            with FakeGitHub(leaf, key, fixture, repository) as server:
                bus = keyring = session = None
                if sys.platform.startswith("linux"):
                    environment, bus, keyring, session = linux_environment(root, server, authority)
                else:
                    environment = {name: value for name, value in os.environ.items()
                                   if not name.startswith(("GH_", "GITHUB_"))}
                    environment.update(QT_QPA_PLATFORM="bpr-no-display", NO_COLOR="1",
                                       BPR_GITHUB_NATIVE_HTTPS_TEST_LOOPBACK=f"{server.port}:{authority}")
                course = Course(binary, seed, environment, server, arguments.step_seconds)
                try:
                    never_signed_in(course, root)
                    if sys.platform.startswith("linux"):
                        native_paths(course, root, server)
                    else:
                        say(f"{PREFIX}:native-paths:not-run:the vault here is the user's own; Linux runs them "
                            "on a private Secret Service")
                    if gh_selected:
                        gh_paths(course, root)
                    else:
                        say(f"{PREFIX}:gh-paths:not-run:{gh_reason}")
                finally:
                    if keyring is not None:
                        session.stop(keyring)
                    if bus is not None and bus.stop():
                        violations.append("the test bus activated a service")
                failures = course.failures
                violations.extend(server.violations)
                unmatched = [entry for entry in server.requests if entry.startswith("unmatched:")]
    for violation in violations:
        say(f"{PREFIX}:violation:{violation}")
    for entry in unmatched:
        say(f"{PREFIX}:{entry}")
    if failures or violations:
        say(f"{PREFIX}:fail:{','.join(failures)}")
        return 1
    say(f"{PREFIX}:pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
