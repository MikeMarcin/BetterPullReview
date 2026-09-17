#!/usr/bin/env python3
"""Offline feasibility probe, not production GitHub credential qualification.

Uses an unmodified private copy of official gh, a synthetic API on an AF_UNIX
socket, and a local Git repository. Git network protocols are disabled. No real
account configuration is opened. Results contain structural observations only.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import secrets
import shutil
import socketserver
import subprocess
import sys
import threading

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
# Contained on Windows a tempfile root is unusable (BPR-0350); see the helper.
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402


def packet(data: bytes) -> bytes:
    return f"{len(data) + 4:04x}".encode() + data


def run_probe(gh: Path, git: Path, current_schema: bool, operation: str, sandbox: bool, symlink_hosts: bool) -> dict:
    # On POSIX the contained support delegates to `tempfile.mkdtemp`, so this
    # root is still created mode 0700 and stays owner-private.
    with contained_temporary_directory(prefix="bpr-gh-", parent="/tmp") as name:
        root = Path(name)
        copied_gh = root / "gh"
        shutil.copy2(gh, copied_gh)
        copied_gh.chmod(0o500)
        digest = hashlib.sha256(copied_gh.read_bytes()).hexdigest()
        version_home = root / "version-home"
        version_home.mkdir(mode=0o700)
        version = subprocess.run(
            [str(copied_gh), "--version"], cwd=root,
            env={"HOME": str(version_home), "PATH": "/usr/bin:/bin"},
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10, check=True,
        ).stdout.decode().splitlines()[0]
        config = root / ".config" / "gh"
        config.mkdir(parents=True, mode=0o700)
        clones = root / "clones"
        clones.mkdir(mode=0o700)
        canary = secrets.token_hex(32)
        (config / "hosts.yml").write_text(
            f"github.com:\n    user: fixture\n    oauth_token: {canary}\n"
        )
        (config / "hosts.yml").chmod(0o600)
        if symlink_hosts:
            external_hosts = root / "external-hosts.yml"
            (config / "hosts.yml").rename(external_hosts)
            (config / "hosts.yml").symlink_to(external_hosts)
        socket_path = root / "api.sock"
        (config / "config.yml").write_text(
            ("version: 1\n" if current_schema else "")
            + f"http_unix_socket: {socket_path}\n"
        )
        before = {p.name: p.read_bytes() for p in config.iterdir()}
        before_metadata = {p.name: (p.stat().st_ino, p.stat().st_size, p.stat().st_mtime_ns)
                           for p in config.iterdir()}
        env = {
            "HOME": str(root),
            "PATH": str(git.parent) + ":/usr/bin:/bin",
            "GH_PROMPT_DISABLED": "1",
            "GH_NO_UPDATE_NOTIFIER": "1",
            "GH_NO_EXTENSION_UPDATE_NOTIFIER": "1",
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": "/dev/null",
            "GIT_TERMINAL_PROMPT": "0",
            "GIT_ALLOW_PROTOCOL": "file",
            "GIT_AUTHOR_NAME": "Fixture",
            "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
            "GIT_COMMITTER_NAME": "Fixture",
            "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
        }

        def execute(executable: Path, args: list[str], data: bytes = b""):
            command = [str(executable), *args]
            if sandbox and executable == copied_gh:
                profile = ('(version 1)(allow default)(deny file-write*)'
                           '(allow file-write* (literal "/dev/null"))')
                if operation == "bridge":
                    profile += '(allow file-write* (subpath "' + str(clones.resolve()) + '"))'
                command = [
                    "/usr/bin/sandbox-exec", "-p", profile,
                    *command,
                ]
            result = subprocess.run(
                command, cwd=root, env=env, input=data,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20,
            )
            assert canary.encode() not in result.stdout + result.stderr
            return result

        def checked(args: list[str], data: bytes = b"") -> bytes:
            result = execute(git, args, data)
            assert result.returncode == 0, "local-git-operation-failed"
            return result.stdout.strip()

        # Harmless hostile values would break a clone or execute an executable
        # config path if ambient configuration were admitted. No shell is run.
        (root / ".gitconfig").write_text(
            '[url "ext::BPR_DISABLED_EXECUTABLE"]\n insteadOf = https://github.com/\n'
            '[credential]\n helper = BPR_DISABLED_HELPER\n'
            '[core]\n hooksPath = /BPR_DISABLED_HOOKS\n fsmonitor = BPR_DISABLED_FSMONITOR\n'
            '[uploadpack]\n packObjectsHook = BPR_DISABLED_PACK_HOOK\n'
        )
        ambient = checked(["config", "--show-origin", "--list"])
        assert b"BPR_DISABLED" not in ambient and b".gitconfig" not in ambient
        source = root / "source.git"
        checked(["init", "--bare", "--initial-branch=main", str(source)])
        scope = ["--git-dir=" + str(source)]
        blob = checked(scope + ["hash-object", "-w", "--stdin"], b"fixture\n")
        tree = checked(scope + ["mktree"], b"100644 blob " + blob + b"\tfile.txt\n")
        base = checked(scope + ["commit-tree", tree.decode()], b"base\n")
        head = checked(scope + ["commit-tree", tree.decode(), "-p", base.decode()], b"head\n")
        checked(scope + ["update-ref", "refs/heads/main", base.decode()])
        checked(scope + ["update-ref", "refs/pull/1/head", head.decode()])
        observations: list[str] = []

        class Handler(socketserver.StreamRequestHandler):
            def handle(self):
                self.connection.settimeout(5)
                line = self.rfile.readline(8192)
                assert line in (b"POST /graphql HTTP/1.1\r\n", b"GET /user HTTP/1.1\r\n"), "unexpected-api-route"
                length = 0
                for _ in range(64):
                    header = self.rfile.readline(8192)
                    if header == b"\r\n":
                        break
                    if header.lower().startswith(b"content-length:"):
                        length = int(header.split(b":", 1)[1])
                if line.startswith(b"POST"):
                    assert 0 < length < 16384
                    query = json.loads(self.rfile.read(length))
                    assert query["variables"] == {"name": "repo", "owner": "fixture"}
                    assert "query RepositoryInfo" in query["query"]
                    observations.append("repository-info")
                    body = json.dumps({"data": {"repository": {
                        "id": "R_fixture", "databaseId": 1, "name": "repo",
                        "owner": {"login": "fixture"}, "parent": None,
                        "defaultBranchRef": {"name": "main"},
                    }}}).encode()
                else:
                    assert length == 0
                    observations.append("viewer")
                    body = json.dumps({"id": 1, "login": "fixture"}).encode()
                self.wfile.write(
                    b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                    + f"Content-Length: {len(body)}\r\nConnection: close\r\n\r\n".encode()
                    + body
                )

        server = socketserver.UnixStreamServer(str(socket_path), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        clone_results = []
        operation_exit = 0
        try:
            if operation != "bridge":
                args = (["--version"] if operation == "version" else
                        ["api", "user", "--hostname", "github.com", "--include"])
                result = execute(copied_gh, args)
                operation_exit = result.returncode
                assert result.returncode == 0 or (sandbox and not current_schema), "gh-api-failed"
            for index, oid in enumerate((base, head) if operation == "bridge" else ()):
                destination = clones / f"clone-{index}.git"
                result = execute(copied_gh, [
                    "repo", "clone", "https://github.com/fixture/repo.git",
                    str(destination), "--no-upstream", "--", "--bare", "--quiet",
                    "--revision=" + oid.decode(),
                    "--config", "url." + source.as_uri() + ".insteadOf=https://github.com/fixture/repo.git",
                    "--config", "transfer.unpackLimit=0",
                ])
                clone_results.append({
                    "exit": result.returncode,
                    "stdout_bytes": len(result.stdout),
                    "stderr_bytes": len(result.stderr),
                    "exact_commit": result.returncode == 0 and checked(
                        ["--git-dir=" + str(destination), "rev-parse", "HEAD"]
                    ) == oid,
                })
                assert result.returncode == 0, "gh-clone-failed"
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
            assert not thread.is_alive()

        discovery = b""
        fetched = b""
        if operation == "bridge":
            serving = root / "serve.git"
            checked(["init", "--bare", "--initial-branch=main", str(serving)])
            (serving / "objects/info/alternates").write_text("".join(
                str(clones / f"clone-{index}.git/objects") + "\n" for index in range(2)
            ))
            serving_scope = ["--git-dir=" + str(serving)]
            checked(serving_scope + ["update-ref", "refs/heads/main", base.decode()])
            checked(serving_scope + ["update-ref", "refs/pull/1/head", head.decode()])
            env["GIT_PROTOCOL"] = "version=2"
            discovery = checked(["upload-pack", "--stateless-rpc", "--advertise-refs", str(serving)])
            request = (packet(b"command=fetch\n") + packet(b"object-format=sha1\n")
                       + b"0001" + packet(b"want " + base + b"\n")
                       + packet(b"want " + head + b"\n") + packet(b"done\n") + b"0000")
            fetched = checked(["upload-pack", "--stateless-rpc", str(serving)], request)
            assert b"packfile\n" in fetched and b"PACK" in fetched
        assert hashlib.sha256(copied_gh.read_bytes()).hexdigest() == digest
        after = {p.name: p.read_bytes() for p in config.iterdir()}
        changed_config = sorted(name for name in before.keys() | after.keys()
                                if before.get(name) != after.get(name))
        if sandbox:
            assert not changed_config, "sandbox-permitted-config-write"
            if not current_schema:
                assert operation_exit != 0 and not observations, "legacy-migration-not-refused-before-api"
        elif not current_schema:
            assert changed_config == ["config.yml", "hosts.yml"], "legacy-migration-behavior-changed"
        return {
            "schema": "rfc0015.gh-clone-local-bridge-probe.v1",
            "configuration_schema": "1" if current_schema else "legacy",
            "operation": operation,
            "operation_exit": operation_exit,
            "sandbox_config_write_denial": sandbox,
            "symlinked_hosts_outside_config": symlink_hosts,
            "gh_sha256": digest,
            "gh_version": version,
            "git_version": checked(["--version"]).decode(),
            "clone_results": clone_results,
            "api_requests": len(observations),
            "git_network_protocols": "disabled; file only",
            "discovery_v2": b"version 2\n" in discovery,
            "fetch_pack": bool(fetched),
            "config_unchanged": not changed_config,
            "config_metadata_unchanged": before_metadata == {
                p.name: (p.stat().st_ino, p.stat().st_size, p.stat().st_mtime_ns)
                for p in config.iterdir()
            },
            "changed_config_files": changed_config,
            "real_credential_used": False,
            "hostile_ambient_git_config_ignored": True,
            "canary_absent_from_child_output_and_object_store": all(
                canary.encode() not in p.read_bytes()
                for p in clones.rglob("*") if p.is_file()
            ),
            "production_bound_or_cancellation_proven": False,
            "limitation": "gh produces disk objects; separate local Git and bounded temporary storage are required",
        }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gh", type=Path, required=True)
    parser.add_argument("--git", type=Path, required=True)
    parser.add_argument("--current-schema", action="store_true")
    parser.add_argument("--operation", choices=("api", "bridge", "version"), default="bridge")
    parser.add_argument("--sandbox-config", action="store_true")
    parser.add_argument("--symlink-hosts", action="store_true")
    options = parser.parse_args()
    print(json.dumps(run_probe(options.gh.resolve(), options.git.resolve(), options.current_schema, options.operation, options.sandbox_config, options.symlink_hosts), indent=2))
