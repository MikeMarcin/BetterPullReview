#!/usr/bin/env python3
"""Release-only HTTPS fixture: no DNS, no external sockets, no live credentials."""
from __future__ import annotations

import argparse
import secrets
import socket
import ssl
import subprocess
import threading
from pathlib import Path

import verify_native_https_transport as support


class Proxy:
    def __init__(self, certificate, port: int, scenario: str, secret: bytes):
        self.scenario, self.secret = scenario, secret
        self.requests = []
        self.errors = []
        self.stop = threading.Event()
        self.socket = socket.socket()
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind(("127.0.0.1", port))
        self.socket.listen(4)
        self.socket.settimeout(0.1)
        self.port = self.socket.getsockname()[1]
        self.tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.tls.load_cert_chain(certificate.leaf_certificate, certificate.leaf_private_key)
        self.thread = threading.Thread(target=self.run)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_):
        self.stop.set()
        self.thread.join(5)
        self.socket.close()
        assert not self.thread.is_alive(), "fixture thread failed to drain"
        assert not self.errors, self.errors

    def run(self):
        while not self.stop.is_set():
            try:
                connection, _ = self.socket.accept()
            except socket.timeout:
                continue
            try:
                with connection:
                    connection.settimeout(2)
                    header, rest = support.receive_through(connection, b"\r\n\r\n", 16384)
                    authority = header.split(b" ")[1]
                    assert authority in (b"api.github.com:443", b"release-assets.githubusercontent.com:443")
                    assert self.secret not in header and not rest
                    connection.sendall(b"HTTP/1.1 200 Connection established\r\n\r\n")
                    with self.tls.wrap_socket(connection, server_side=True) as channel:
                        origin, _ = support.receive_through(channel, b"\r\n\r\n", 32768)
                        self.requests.append((authority, origin))
                        self.respond(channel, authority)
            except (ssl.SSLError, BrokenPipeError, ConnectionResetError, socket.timeout):
                pass
            except support.ProbeFailure as error:
                if self.scenario != "untrusted" or error.code != "inbound-header-truncated":
                    self.errors.append(error.code)
            except Exception as error:
                self.errors.append(type(error).__name__)

    def respond(self, channel, authority):
        scenario = self.scenario
        if scenario == "cancel":
            channel.sendall(b"HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/octet-stream\r\nContent-Length: 100\r\n\r\nx")
            self.stop.wait(1)
            return
        if scenario == "timeout":
            self.stop.wait(1)
            return
        body = b"{}" if scenario == "metadata" else b"x" * 100
        status, headers = b"200 OK", []
        content_type = b"application/json" if scenario == "metadata" else b"application/octet-stream"
        if scenario in ("401", "403", "404", "429"):
            status, body = scenario.encode() + b" Refused", b""
            if scenario == "429":
                headers.append((b"Retry-After", b"61"))
        elif "redirect" in scenario:
            if scenario != "asset-redirect" or authority == b"api.github.com:443":
                status, body = b"302 Found", b""
                destination = b"https://release-assets.githubusercontent.com/fixture?signature=opaque"
                if scenario == "cross-redirect":
                    destination = b"https://evil.invalid/steal"
                elif scenario == "http-redirect":
                    destination = b"http://release-assets.githubusercontent.com/fixture"
                elif scenario == "userinfo-redirect":
                    destination = b"https://user@release-assets.githubusercontent.com/fixture"
                elif scenario == "api-redirect":
                    destination = b"https://api.github.com/other"
                headers.append((b"Location", destination))
                headers.append((b"Set-Cookie", b"credential=fixture-cookie; Secure"))
        if scenario == "encoding":
            headers.append((b"Content-Encoding", b"gzip"))
        if scenario == "headers":
            headers.append((b"X-Large", b"z" * 9000))
        if scenario == "type":
            content_type = b"text/html"
        headers.append((b"Content-Type", content_type))
        if scenario == "stream-size":
            body += b"x"
        else:
            headers.append((b"Content-Length", b"101" if scenario == "size" else str(len(body)).encode()))
        if scenario == "duplicate":
            headers.append((b"Content-Type", b"text/html"))
        channel.sendall(b"HTTP/1.1 " + status + b"\r\nConnection: close\r\n" + b"".join(k + b": " + v + b"\r\n" for k, v in headers) + b"\r\n" + body)

    def verify(self):
        for authority, request in self.requests:
            first = request.split(b"\r\n", 1)[0]
            headers = support.parse_headers(request, first)
            assert support.one_header(headers, "x-github-api-version") == b"2022-11-28"
            assert support.one_header(headers, "accept-encoding") == b"identity"
            assert b"cookie:" not in request.lower()
            if authority == b"api.github.com:443":
                path = b"releases/latest" if self.scenario in ("metadata", "metadata-redirect") else b"releases/assets/20"
                assert first == b"GET /repos/MikeMarcin/BetterPullReview/" + path + b" HTTP/1.1"
                assert support.one_header(headers, "authorization") == b"Bearer " + self.secret
            else:
                assert self.secret not in request and b"authorization:" not in request.lower()
        if self.scenario == "asset-redirect":
            assert len(self.requests) == 2
        elif self.scenario == "redirect-loop":
            assert len(self.requests) == 4
        elif self.scenario not in ("pre-cancel", "untrusted"):
            assert len(self.requests) == 1
        else:
            assert not self.requests


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--openssl", type=Path, required=True)
    args = parser.parse_args()
    scenarios = ("metadata", "asset", "asset-redirect", "metadata-redirect", "cross-redirect", "http-redirect", "userinfo-redirect", "api-redirect", "redirect-loop", "401", "403", "404", "429", "encoding", "headers", "type", "duplicate", "size", "stream-size", "sink-refuses", "cancel", "timeout", "pre-cancel", "untrusted")
    support.HOST = "release-assets.githubusercontent.com"
    with support.contained_temporary_directory(prefix="bpr-release-wire-") as temporary:
        certificate = support.generate_certificate_material(args.openssl, Path(temporary))
        port = support.configured_loopback_ports(1)[0]
        for scenario in scenarios:
            secret = secrets.token_hex(20).encode()
            with Proxy(certificate, port, scenario, secret) as proxy:
                result = subprocess.run([str(args.executable), scenario, str(proxy.port), str(certificate.authority_certificate)], input=secret + b"\n", capture_output=True, timeout=10)
                assert secret not in result.stdout + result.stderr, "secret canary escaped diagnostics"
                assert result.returncode == 0, (scenario, result.stdout, result.stderr)
                proxy.verify()
            print(f"release-wire:{scenario}:pass")
    print("release-update-native-https:pass")


if __name__ == "__main__":
    main()
