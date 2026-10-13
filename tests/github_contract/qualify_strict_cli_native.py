#!/usr/bin/env python3
"""Explicit macOS RFC-0078 course. Stock gh, production sensitive acquisition.

Only random synthetic gh:<host> Keychain items are created/deleted. No real
credential is opened here. --live-api separately authorizes exact named product
acquisition and /user observations for the two designated BPR-Test identities.
Raw process output, credentials and HTTP authorization are never printed.
"""
from __future__ import annotations

import argparse
import base64
import ctypes
import hashlib
import json
import os
from pathlib import Path
import secrets
import socketserver
import ssl
import subprocess
import sys
import threading
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise RuntimeError(reason)


def run(command: list[str], *, cwd: Path, env: dict[str, str] | None = None,
        data: bytes = b"", timeout: int = 30) -> subprocess.CompletedProcess:
    return subprocess.run(command, cwd=cwd, env=env, input=data,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          timeout=timeout, check=False)


class SyntheticItems:
    """Legacy items compatible with stock gh's /usr/bin/security reader.

    The ACL admits that reader. Item references, not broad searches, authorize
    deletion. No existing item is replaced, and absence is checked by exact key.
    """
    def __init__(self):
        self.cf = ctypes.CDLL("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")
        self.sec = ctypes.CDLL("/System/Library/Frameworks/Security.framework/Security")
        pointer = ctypes.c_void_p
        self.cf.CFRelease.argtypes = [pointer]
        self.sec.SecKeychainItemDelete.argtypes = [pointer]
        self.sec.SecKeychainFindGenericPassword.argtypes = [pointer, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, pointer, pointer, pointer]
        self.items: list[tuple[ctypes.c_void_p, bytes, bytes]] = []

    def add(self, host: str, account: str, token: str) -> None:
        service = ("gh:" + host).encode()
        login = account.encode()
        require(self.sec.SecKeychainFindGenericPassword(None, len(service), service, len(login), login, None, None, None) == -25300, "synthetic-item-must-not-exist")
        # Same stdin mechanism as stock go-keyring, with no update option.
        # The only named reader is security itself. Setup output is discarded.
        command = 'add-generic-password -s "' + service.decode() + '" -a "' + account + '" -w "' + token + '" -T /usr/bin/security\n'
        result = run(["/usr/bin/security", "-i"], cwd=Path("/private/tmp"), data=command.encode())
        require(result.returncode == 0, "synthetic-item-create")
        item = ctypes.c_void_p()
        status = self.sec.SecKeychainFindGenericPassword(None, len(service), service, len(login), login, None, None, ctypes.byref(item))
        require(status == 0 and item.value, "synthetic-exact-item-reference")
        self.items.append((item, service, login))

    def close(self) -> None:
        failed = False
        for item, service, login in reversed(self.items):
            failed |= self.sec.SecKeychainItemDelete(item) != 0
            self.cf.CFRelease(item)
            failed |= self.sec.SecKeychainFindGenericPassword(None, len(service), service, len(login), login, None, None, None) != -25300
        self.items.clear()
        require(not failed, "synthetic-exact-cleanup")

    def remove_named(self, host: str, login: str) -> None:
        service, account = ("gh:" + host).encode(), login.encode()
        matches = [(index, item) for index, item in enumerate(self.items) if item[1:] == (service, account)]
        require(len(matches) == 1, "synthetic-exact-removal-authority")
        index, (item, _, _) = matches[0]
        require(self.sec.SecKeychainItemDelete(item) == 0, "synthetic-named-removal")
        self.cf.CFRelease(item)
        self.items.pop(index)
        require(self.sec.SecKeychainFindGenericPassword(None, len(service), service, len(account), account, None, None, None) == -25300, "synthetic-named-absence")


def certificates(root: Path, hosts: list[str]) -> tuple[Path, Path, Path]:
    cert, key = root / "authority.pem", root / "authority-key.pem"
    config = root / "authority.cnf"
    config.write_text("[req]\ndistinguished_name=dn\nx509_extensions=ext\nprompt=no\n[dn]\nCN=BPR disposable native gh qualification\n[ext]\nbasicConstraints=critical,CA:TRUE\nkeyUsage=critical,digitalSignature,keyCertSign\nsubjectAltName=" + ",".join("DNS:" + host for host in hosts) + "\n")
    result = run(["/usr/bin/openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-config", str(config), "-keyout", str(key), "-out", str(cert)], cwd=root)
    require(result.returncode == 0, "certificate-generation")
    return cert, cert, key


class Proxy(socketserver.ThreadingTCPServer):
    allow_reuse_address = False
    daemon_threads = True


def local_course(arguments, root: Path, items: SyntheticItems) -> dict:
    nonce = secrets.token_hex(8)
    hosts = ["bpr" + nonce + ".invalid", "bpr" + nonce + ".github.localhost", "bpr" + nonce + ".ghe.com"]
    cert, leaf, key = certificates(root, hosts)
    chosen, unnamed = secrets.token_hex(32), secrets.token_hex(32)
    login = "bpr" + nonce
    # Stock security resolves the user's Keychain search list through HOME.
    # gh configuration is still explicitly private and token free.
    home = Path(os.environ["HOME"])
    source = root / "source" / "MikeMarcin" / "BPR-Test.git"
    source.parent.mkdir(parents=True)
    env = {"PATH": "/usr/bin:/bin", "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null", "GIT_ALLOW_PROTOCOL": "", "GIT_AUTHOR_NAME": "Synthetic", "GIT_AUTHOR_EMAIL": "fixture@example.invalid", "GIT_COMMITTER_NAME": "Synthetic", "GIT_COMMITTER_EMAIL": "fixture@example.invalid"}

    def git(args: list[str], data: bytes = b"") -> bytes:
        result = run(["/usr/bin/git", *args], cwd=root, env=env, data=data)
        require(result.returncode == 0, "synthetic-local-git")
        return result.stdout.strip()

    git(["init", "--bare", "--initial-branch=main", str(source)])
    scope = ["--git-dir=" + str(source)]
    blob = git(scope + ["hash-object", "-w", "--stdin"], b"synthetic native qualification\n")
    tree = git(scope + ["mktree"], b"100644 blob " + blob + b"\tfile.txt\n")
    commit = git(scope + ["commit-tree", tree.decode()], b"synthetic\n").decode()
    git(scope + ["update-ref", "refs/heads/main", commit])
    observations = {"api": 0, "challenge": 0, "selected_helper": 0, "fallback_rejected": 0, "violations": 0}
    lock = threading.Lock()
    tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls.load_cert_chain(leaf, key)
    selected_header = "Basic " + base64.b64encode(("x-access-token:" + chosen).encode()).decode()
    fallback_header = "Basic " + base64.b64encode((login + ":" + unnamed).encode()).decode()

    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            try:
                self.request.settimeout(10)
                first = self.request.makefile("rb")
                line = first.readline(8192)
                target = line.decode().split()[1]
                require(target in {host + ":443" for host in hosts}, "unexpected-connect")
                while first.readline(8192) not in (b"\r\n", b"\n", b""):
                    pass
                self.request.sendall(b"HTTP/1.1 200 Connection established\r\n\r\n")
                connection = tls.wrap_socket(self.request, server_side=True)
                stream = connection.makefile("rb")
                method, path, _ = stream.readline(8192).decode().strip().split()
                headers = {}
                while True:
                    line = stream.readline(8192)
                    if line in (b"\r\n", b"\n", b""):
                        break
                    name, value = line.decode().split(":", 1)
                    headers[name.lower()] = value.strip()
                body = stream.read(int(headers.get("content-length", "0")))
                hostname = target[:-4]
                if path in ("/api/graphql", "/graphql"):
                    with lock:
                        observations["api"] += 1
                    # API deliberately permits either authority. The negative
                    # must reach Git's 401/helper exchange before it can fail.
                    document = {"data": {"repository": {"id": "synthetic", "name": "BPR-Test", "owner": {"login": "MikeMarcin"}, "url": "https://" + hostname + "/MikeMarcin/BPR-Test", "isFork": False, "isPrivate": True, "defaultBranchRef": {"name": "main"}, "hasWikiEnabled": False}}}
                    payload = json.dumps(document).encode()
                    response_headers = b"Content-Type: application/json\r\n"
                    status = b"200 OK"
                else:
                    require(path.startswith("/MikeMarcin/BPR-Test.git/"), "unexpected-git-route")
                    auth = headers.get("authorization", "")
                    if auth != selected_header:
                        with lock:
                            observations["challenge"] += auth == ""
                            observations["fallback_rejected"] += auth == fallback_header
                        payload, response_headers, status = b"authentication required", b'WWW-Authenticate: Basic realm="synthetic"\r\n', b"401 Unauthorized"
                    else:
                        with lock:
                            observations["selected_helper"] += 1
                        parsed = urlsplit(path)
                        backend_env = {**env, "GIT_PROJECT_ROOT": str(root / "source"), "GIT_HTTP_EXPORT_ALL": "1", "PATH_INFO": parsed.path, "QUERY_STRING": parsed.query, "REQUEST_METHOD": method, "CONTENT_TYPE": headers.get("content-type", ""), "CONTENT_LENGTH": str(len(body)), "REMOTE_USER": "synthetic"}
                        result = run(["/usr/bin/git", "http-backend"], cwd=root, env=backend_env, data=body)
                        require(result.returncode == 0, "git-http-backend")
                        response_headers, payload = result.stdout.split(b"\r\n\r\n", 1)
                        response_headers += b"\r\n"
                        status = b"200 OK"
                connection.sendall(b"HTTP/1.1 " + status + b"\r\n" + response_headers + b"Connection: close\r\nContent-Length: " + str(len(payload)).encode() + b"\r\n\r\n" + payload)
                connection.close()
            except Exception:
                with lock:
                    observations["violations"] += 1

    class ApiHandler(socketserver.StreamRequestHandler):
        def handle(self):
            self.connection.settimeout(10)
            self.rfile.readline(8192)
            headers = {}
            while True:
                line = self.rfile.readline(8192)
                if line in (b"\r\n", b"\n", b""):
                    break
                key_name, value = line.decode().split(":", 1)
                headers[key_name.lower()] = value.strip()
            self.rfile.read(int(headers.get("content-length", "0")))
            hostname = headers["host"]
            with lock:
                observations["api"] += 1
            document = {"data": {"repository": {"id": "synthetic", "name": "BPR-Test", "owner": {"login": "MikeMarcin"}, "url": "https://" + hostname + "/MikeMarcin/BPR-Test", "isFork": False, "isPrivate": True, "defaultBranchRef": {"name": "main"}, "hasWikiEnabled": False}}}
            payload = json.dumps(document).encode()
            self.wfile.write(b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + str(len(payload)).encode() + b"\r\n\r\n" + payload)

    socket_path = root / "api.sock"
    api = socketserver.ThreadingUnixStreamServer(str(socket_path), ApiHandler)
    api_thread = threading.Thread(target=api.serve_forever, daemon=True)
    api_thread.start()
    proxy = Proxy(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=proxy.serve_forever, daemon=True)
    thread.start()
    proxy_url = "http://127.0.0.1:" + str(proxy.server_address[1])
    records = []
    try:
        for host in hosts:
            items.add(host, "", unnamed)
            # A usable unnamed token is confirmed using stock gh and only
            # synthetic token-free local config, before named acquisition.
            config = root / ("config-" + host)
            config.mkdir(mode=0o700)
            (config / "config.yml").write_text('version: "1"\n')
            (config / "hosts.yml").write_text(host + ":\n    user: " + login + "\n    users:\n        " + login + ":\n")
            result = run([str(arguments.gh), "auth", "token", "--hostname", host], cwd=root, env={"HOME": str(home), "GH_CONFIG_DIR": str(config), "PATH": "/usr/bin:/bin", "GH_PROMPT_DISABLED": "1"})
            require(result.returncode == 0 and result.stdout.strip() == unnamed.encode(), "unnamed-synthetic-not-usable:exit=" + str(result.returncode) + ":bytes=" + str(len(result.stdout)) + ":error-bytes=" + str(len(result.stderr)))
            result = run([str(arguments.probe), "missing", str(arguments.gh), str(home), host, login, "-", "-", "-", "-"], cwd=root)
            require(result.returncode == 0 and b"native-missing:pass" in result.stdout, "product-named-missing-refusal")
            for mode in ("clone", "clone-negative"):
                items.add(host, login, chosen)
                before = dict(observations)
                process = subprocess.Popen([str(arguments.probe), mode, str(arguments.gh), str(home), host, login, proxy_url, str(cert), commit, str(socket_path)], cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    require(process.stdout.readline() == b"native-acquired\n", "product-sensitive-acquisition-before-removal")
                    items.remove_named(host, login)
                    stdout, stderr = process.communicate(b"\n", timeout=30)
                    result = subprocess.CompletedProcess([], process.returncode, stdout, stderr)
                finally:
                    if process.poll() is None:
                        process.stdin.close()
                        process.wait(timeout=150)
                require(chosen.encode() not in result.stdout + result.stderr and unnamed.encode() not in result.stdout + result.stderr, "native-output-canary")
                require(result.returncode == 0, "native-" + mode + "-failed:" + result.stderr.decode(errors="replace")[:512] + ":observations=" + json.dumps(observations))
                delta = {name: observations[name] - before[name] for name in observations}
                require(delta["api"] > 0 and delta["challenge"] > 0 and delta["violations"] == 0, "native-helper-not-reached")
                require(delta["selected_helper"] > 0 if mode == "clone" else delta["selected_helper"] == 0 and delta["fallback_rejected"] > 0, "native-helper-negative-not-auth-dependent:" + json.dumps(delta))
                records.append({"host_class": "enterprise" if host.endswith(".invalid") else "github.localhost" if host.endswith(".github.localhost") else "ghe.com", "mode": mode, "observations": delta})
        for file in root.rglob("*"):
            if file.is_file() and not file.is_socket():
                data = file.read_bytes()
                require(chosen.encode() not in data and unnamed.encode() not in data, "native-persistent-canary")
    finally:
        api.shutdown()
        api.server_close()
        api_thread.join(timeout=5)
        proxy.shutdown()
        proxy.server_close()
        thread.join(timeout=5)
    return {"local": records, "synthetic_named_missing": "pass", "native_persistence": "pass", "routing": "test-owned gh private config Unix socket for metadata; test-only Git http.sslCAInfo; loopback CONNECT/TLS; production macOS trust policy unchanged", "helper": "stock gh -> stock Git -> stock gh auth git-credential; Basic x-access-token accepted; unnamed fallback rejected"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--gh", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--live-api", action="store_true")
    arguments = parser.parse_args()
    arguments.probe = arguments.probe.resolve(strict=True)
    arguments.gh = arguments.gh.resolve(strict=True)
    require(sys.platform == "darwin", "macOS-only")
    items = SyntheticItems()
    with contained_temporary_directory(prefix="bpr-native-strict-", parent="/private/tmp") as name:
        root = Path(name)
        active_before = run([str(arguments.gh), "config", "get", "user", "--host", "github.com"], cwd=root)
        require(active_before.returncode == 0, "global-active-observation")
        def configuration_metadata():
            # Metadata only, never open/read/hash the owner's configuration.
            result = {}
            for filename in ("config.yml", "hosts.yml"):
                path = Path(os.environ["HOME"]) / ".config" / "gh" / filename
                if path.exists():
                    stat = path.lstat()
                    result[filename] = [stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_mode]
            return result
        metadata_before = configuration_metadata()
        facts = {"gh_sha256": hashlib.sha256(arguments.gh.read_bytes()).hexdigest(), "gh_version": run([str(arguments.gh), "--version"], cwd=root).stdout.decode().splitlines()[0], "platform": os.uname().release}
        try:
            facts.update(local_course(arguments, root, items))
            if arguments.live_api:
                facts["live"] = []
                for login in ("MikeMarcin", "MikeMarcinTest"):
                    result = run([str(arguments.probe), "api", str(arguments.gh), os.environ["HOME"], "github.com", login, "-", "-", "-", login], cwd=root)
                    require(result.returncode == 0, "native-selected-api-failed")
                    facts["live"].append(result.stdout.decode().strip())
            active_after = run([str(arguments.gh), "config", "get", "user", "--host", "github.com"], cwd=root)
            require(active_after.returncode == 0 and active_before.stdout == active_after.stdout, "global-active-state-changed")
            require(configuration_metadata() == metadata_before, "global-configuration-metadata-changed")
            facts["global_active_unchanged"] = active_before.stdout.decode().strip()
            facts["global_configuration_metadata_unchanged"] = metadata_before
        finally:
            items.close()
        facts["synthetic_exact_cleanup"] = "pass"
        arguments.output.write_text(json.dumps(facts, indent=2) + "\n")
    print("strict-cli-native:pass")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print("strict-cli-native:fail:" + str(error), file=sys.stderr)
        raise SystemExit(1)
