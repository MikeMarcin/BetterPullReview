#!/usr/bin/env python3
"""[[BPR-1846]]. Loopback courses for the GitHub CLI avatar helper.

A fake `gh` prints a fresh token only when asked `auth token` for the pinned
host and account under a private configuration; a CONNECT proxy on loopback
terminates TLS for a fake private-mode GHES with a disposable authority. Its
avatars API answers a picture only to `Authorization: token <token>`; every
other route answers as private mode does. The application side (the
`gh_cli_avatar_helper_tests` driver: a pinned gh configuration, the shipped gh
process runner and the transport's helper launch) prints what it received and
writes every launch it made; this script judges both, and checks the token is
nowhere on the application's side: not in a launch's program, arguments,
environment, input or output, nor in the driver's own output.

Nothing leaves loopback: an authority outside the route table is refused at
CONNECT and counted.
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
import stat
import subprocess
import sys
import threading

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

GHES = "ghe.bpr.invalid"
AVATARS = "avatars.ghe.bpr.invalid"
OTHER = "other.bpr.invalid"
HOSTS = (GHES, AVATARS, OTHER)
LOGIN = "octo"
PNG = b"\x89PNG\r\n\x1a\n" + b"\x00" * 56
SIGN_IN = b"<!DOCTYPE html><title>Sign in</title>"
TIMEOUT_SECONDS = 20.0
HEADER_LIMIT = 32 * 1024
API = f"https://{GHES}/api/v3/enterprise/avatars/"


class ProbeFailure(Exception):
    def __init__(self, code: str) -> None:
        super().__init__(code)
        self.code = code


@dataclass
class Seen:
    authority: str
    path: str
    authorization: str | None


@dataclass
class Course:
    name: str
    gh: str
    addresses: list[str]
    expected: str
    requests: list[tuple[str, str, bool]] = field(default_factory=list)
    remove_gh: bool = False


def gh_script(mode: str, token: str) -> str:
    """A POSIX `gh` with no PATH: builtins only. It answers the token only for
    the pinned host and account, run under a private configuration naming that
    account, exactly as the helper must ask."""
    answer = {
        "ok": f"printf '%s\\n' '{token}'\nexit 0",
        "signed-out": "exit 1",
        "broken": "printf 'not a token\\n'\nexit 0",
    }[mode]
    return f"""#!/bin/sh
pinned=0
if [ -n "$GH_CONFIG_DIR" ] && [ -r "$GH_CONFIG_DIR/hosts.yml" ]; then
    while IFS= read -r line; do
        case "$line" in
            *"user: {LOGIN}"*) pinned=1 ;;
        esac
    done < "$GH_CONFIG_DIR/hosts.yml"
fi
if [ $# -eq 6 ] && [ "$1" = auth ] && [ "$2" = token ] && [ "$3" = --hostname ] && [ "$4" = {GHES} ] && [ "$5" = --user ] && [ "$6" = {LOGIN} ] && [ $pinned -eq 1 ]; then
{answer}
fi
exit 2
"""


def courses() -> list[Course]:
    octo, moved, escape = API + "octo?s=48", API + "moved?s=48", API + "escape?s=48"
    return [
        Course(
            "private-mode",
            "ok",
            [octo, moved, escape, f"https://{OTHER}/api/v3/enterprise/avatars/octo?s=48", f"https://{GHES}/octo.png?size=48"],
            f"answered:loaded/200/image/png/{len(PNG)}:loaded/200/image/png/{len(PNG)}:blocked/302/-/0:blocked/0/-/0:blocked/0/-/0",
            [(GHES, "/api/v3/enterprise/avatars/octo?s=48", True), (GHES, "/api/v3/enterprise/avatars/moved?s=48", True),
             (AVATARS, "/u/7?s=48", False), (GHES, "/api/v3/enterprise/avatars/escape?s=48", True)],
        ),
        Course("signed-out", "signed-out", [octo], "cli_signed_out"),
        Course("broken-gh", "broken", [octo], "cli_unavailable"),
        Course("gh-removed", "ok", [octo], "failure:configuration_changed", remove_gh=True),
    ]


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
        "[req]\ndistinguished_name = n\nx509_extensions = e\nprompt = no\n[n]\nCN = BetterPullReview BPR-1846 disposable avatar fixture authority\n"
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


def receive_head(connection: socket.socket) -> bytes:
    received = bytearray()
    while b"\r\n\r\n" not in received:
        if len(received) > HEADER_LIMIT:
            raise ProbeFailure("header-limit")
        block = connection.recv(4096)
        if not block:
            raise ProbeFailure("truncated")
        received.extend(block)
    return bytes(received).partition(b"\r\n\r\n")[0]


class PrivateModeGhes:
    """A CONNECT proxy and TLS origin answering as a private-mode GHES does."""

    def __init__(self, context: ssl.SSLContext, port: int, token: str) -> None:
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

    def answer(self, authority: str, path: str, authorization: str | None) -> tuple[int, list[tuple[str, str]], bytes]:
        credentialed = authorization == f"token {self.token}"
        if authority == GHES and path.startswith("/api/v3/enterprise/avatars/"):
            if not credentialed:
                # Private mode: anything without the token goes to sign in.
                return 302, [("Location", f"https://{GHES}/login")], b""
            if path == "/api/v3/enterprise/avatars/octo?s=48":
                return 200, [("Content-Type", "image/png; charset=binary")], PNG
            if path == "/api/v3/enterprise/avatars/moved?s=48":
                return 302, [("Location", f"https://{AVATARS}/u/7?s=48")], b""
            if path == "/api/v3/enterprise/avatars/escape?s=48":
                return 302, [("Location", f"https://{OTHER}/e.png")], b""
            return 404, [], b""
        if authority == AVATARS and path == "/u/7?s=48":
            if authorization is not None:
                with self.lock:
                    self.violations.append("token-off-host")
            return 200, [("Content-Type", "image/png")], PNG
        return 200, [("Content-Type", "text/html; charset=utf-8")], SIGN_IN

    def serve(self, connection: socket.socket) -> None:
        connection.settimeout(TIMEOUT_SECONDS)
        try:
            head = receive_head(connection)
            parts = head.split(b"\r\n", 1)[0].decode("ascii", "replace").split(" ")
            if len(parts) != 3 or parts[0] != "CONNECT" or not parts[1].endswith(":443"):
                with self.lock:
                    self.violations.append("bad-connect")
                connection.sendall(b"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n")
                return
            authority = parts[1][:-4]
            if b"authorization" in head.lower():
                with self.lock:
                    self.violations.append("authorization-on-connect")
            if authority not in HOSTS or authority == OTHER:
                with self.lock:
                    self.refused.append(authority)
                connection.sendall(b"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n")
                return
            connection.sendall(b"HTTP/1.1 200 Connection Established\r\n\r\n")
            with self.context.wrap_socket(connection, server_side=True) as tls:
                lines = receive_head(tls).decode("latin-1").split("\r\n")
                method, path, _ = lines[0].split(" ", 2)
                headers: dict[str, list[str]] = {}
                for line in lines[1:]:
                    name, _, value = line.partition(":")
                    headers.setdefault(name.strip().lower(), []).append(value.strip())
                authorization = headers.get("authorization", [None])[0]
                with self.lock:
                    self.seen.append(Seen(authority, path, authorization))
                    if method != "GET":
                        self.violations.append(f"method:{method}")
                    if "cookie" in headers:
                        self.violations.append("cookie")
                    if headers.get("accept-encoding") != ["identity"]:
                        self.violations.append("encoding")
                    if authorization is not None and authorization != f"token {self.token}":
                        self.violations.append("foreign-authorization")
                status, extra, body = self.answer(authority, path, authorization)
                reason = {200: "OK", 302: "Found", 404: "Not Found"}.get(status, "Status")
                head_lines = [f"HTTP/1.1 {status} {reason}", "Connection: close", f"Content-Length: {len(body)}",
                              *[f"{name}: {value}" for name, value in extra]]
                tls.sendall(("\r\n".join(head_lines) + "\r\n\r\n").encode("latin-1") + body)
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


def executable_path(value: str) -> Path:
    candidate = Path(value)
    if not candidate.is_absolute():
        raise ProbeFailure("non-absolute-executable")
    resolved = candidate.resolve(strict=True)
    if not resolved.is_file() or not os.access(resolved, os.X_OK):
        raise ProbeFailure("unusable-executable")
    return resolved


def lay_out(root: Path, helper: Path, authority: Path, port: int, mode: str, token: str) -> Path:
    for name in ("gh", "helper", "home", "runtime", "neutral"):
        (root / name).mkdir(mode=0o700)
    gh = root / "gh" / "gh"
    gh.write_text(gh_script(mode, token), encoding="ascii")
    gh.chmod(stat.S_IRWXU)
    # The application gives the helper no channel for a test proxy, so the
    # fixture build reads one beside itself.
    copied = root / "helper" / helper.name
    shutil.copy2(helper, copied)
    (root / "helper" / "bpr_gh_avatar_helper_test.conf").write_text(f"proxy_port={port}\nca={authority}\n", encoding="utf-8")
    return copied


def run_course(driver: Path, helper: Path, course: Course, context: ssl.SSLContext, authority: Path, port: int) -> None:
    token = "gho_fixture" + secrets.token_hex(16)
    server = PrivateModeGhes(context, port, token)
    try:
        with contained_temporary_directory(prefix="bpr1846-avatar-") as temporary:
            root = Path(temporary)
            root.chmod(0o700)
            lay_out(root, helper, authority, server.port, course.gh, token)
            arguments = [str(driver), "--root", str(root), "--course", course.name]
            for address in course.addresses:
                arguments += ["--address", address]
            if course.remove_gh:
                arguments.append("--remove-gh")
            completed = subprocess.run(arguments, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       timeout=TIMEOUT_SECONDS * 3, check=False)
            app_side = (root / "app-side.txt").read_bytes() if (root / "app-side.txt").is_file() else b""
    finally:
        server.close()
    output = completed.stdout.decode("utf-8", "replace").strip()
    expected = f"gh-cli-avatar:{course.name}:{course.expected}"
    if completed.returncode != 0 or output != expected:
        raise ProbeFailure(f"{course.name}:output:{output or completed.stderr.decode('utf-8', 'replace').strip()}")
    secret = token.encode("ascii")
    if secret in completed.stdout or secret in completed.stderr or secret in app_side:
        raise ProbeFailure(f"{course.name}:token-on-application-side")
    launched = course.expected != "failure:configuration_changed"
    if launched != (b"program=" in app_side):
        raise ProbeFailure(f"{course.name}:launches")
    if launched and (b"argument=--user\nargument=octo\n" not in app_side or b"environment=GH_CONFIG_DIR=" not in app_side):
        raise ProbeFailure(f"{course.name}:pinned-launch")
    if server.violations:
        raise ProbeFailure(f"{course.name}:violation:{server.violations[0]}")
    observed = sorted((seen.authority, seen.path, seen.authorization is not None) for seen in server.seen)
    if observed != sorted(course.requests):
        raise ProbeFailure(f"{course.name}:requests:{observed}")
    if server.refused:
        raise ProbeFailure(f"{course.name}:refused:{server.refused}")


def run_helper_directly(helper: Path, arguments: list[str], addresses: list[str]) -> bytes:
    batch = ("bpr-gh-avatar/1\n" + "".join(address + "\n" for address in addresses)).encode("ascii")
    completed = subprocess.run([str(helper), *arguments], input=batch, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               timeout=TIMEOUT_SECONDS, check=False)
    if completed.returncode != 0 or completed.stderr:
        raise ProbeFailure("helper-direct-exit")
    return completed.stdout


def direct_courses(helper: Path) -> None:
    # Another host is refused before any gh launch: a gh that does not exist
    # would otherwise make the batch cli_unavailable.
    missing = str(Path(helper).parent / "no-such-gh")
    wrong = run_helper_directly(helper, ["--gh", missing, "--hostname", GHES, "--user", LOGIN],
                                [f"https://{OTHER}/api/v3/enterprise/avatars/octo?s=48"])
    if wrong != b"bpr-gh-avatar/1 answered 1\nblocked 0 - 0\n":
        raise ProbeFailure(f"wrong-host:{wrong!r}")
    absent = run_helper_directly(helper, ["--gh", missing, "--hostname", GHES, "--user", LOGIN], [API + "octo?s=48"])
    if absent != b"bpr-gh-avatar/1 cli_unavailable 0\n":
        raise ProbeFailure(f"gh-missing:{absent!r}")
    refused = run_helper_directly(helper, ["--gh", missing, "--hostname", "GHE.bpr.invalid", "--user", LOGIN], [API + "octo?s=48"])
    if refused != b"bpr-gh-avatar/1 refused 0\n":
        raise ProbeFailure(f"bad-pin:{refused!r}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--driver", required=True)
    parser.add_argument("--helper", required=True)
    parser.add_argument("--openssl")
    arguments = parser.parse_args()
    try:
        driver = executable_path(arguments.driver)
        helper = executable_path(arguments.helper)
        openssl = Path(arguments.openssl or shutil.which("openssl") or "")
        if not openssl.is_absolute():
            raise ProbeFailure("missing-openssl")
        raw_ports = os.environ.get("BPR_TEST_LOOPBACK_PORTS", "")
        port = int(raw_ports.split(",")[0]) if raw_ports else 0
        direct_courses(helper)
        with contained_temporary_directory(prefix="bpr1846-authority-") as temporary:
            directory = Path(temporary)
            directory.chmod(0o700)
            authority, leaf, key = generate_certificates(openssl, directory)
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.minimum_version = ssl.TLSVersion.TLSv1_2
            context.load_cert_chain(str(leaf), str(key))
            for course in courses():
                run_course(driver, helper, course, context, authority, port)
    except ProbeFailure as error:
        print(f"gh-cli-avatar-helper:fail:{error.code}", file=sys.stderr)
        return 1
    print("gh-cli-avatar-helper:pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
