#!/usr/bin/env python3
"""Provision a persistent named Codex task through a local App Server."""

from __future__ import annotations

import base64
import hashlib
import json
import os
import pathlib
import queue
import secrets
import subprocess
import threading
import time
from collections.abc import Callable
from typing import Any, BinaryIO


THREAD_SOURCE_KINDS = (
    "cli",
    "vscode",
    "exec",
    "appServer",
    "subAgent",
    "subAgentReview",
    "subAgentCompact",
    "subAgentThreadSpawn",
    "subAgentOther",
    "unknown",
)
import sys as _sys

_sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import project_config  # noqa: E402

PROJECT = project_config.load()
STATION_SANDBOX = "danger-full-access"
STATION_BOOTSTRAP_TEXT = (
    f"This is the persistent {PROJECT['display_name']} remote station endpoint. "
    "Do not inspect or modify files. Reply exactly STATION READY."
)
TERMINAL_TURN_STATUSES = {"completed", "failed", "interrupted"}
STATION_BOOTSTRAP_TIMEOUT_SECONDS = 120.0


class CodexStationError(Exception):
    """A capability, protocol, or station-identity failure."""

    def __init__(
        self,
        message: str,
        *,
        method: str | None = None,
        rpc_error: dict[str, Any] | None = None,
    ) -> None:
        super().__init__(message)
        self.method = method
        self.rpc_error = rpc_error


class _AppServerRpcClient:
    """Shared JSON-RPC lifecycle over a transport-specific message stream."""

    def __init__(self, timeout: float) -> None:
        self._timeout = timeout
        self._next_id = 0
        self._messages: queue.Queue[dict[str, Any] | None] = queue.Queue()
        self._notifications: queue.Queue[dict[str, Any] | None] = queue.Queue()
        self._stderr: list[str] = []
        self._write_lock = threading.Lock()

    def __enter__(self) -> _AppServerRpcClient:
        try:
            self.request(
                "initialize",
                {
                    "clientInfo": {
                        "name": f"{PROJECT['project'].replace('-', '_')}_remote_station",
                        "title": f"{PROJECT['display_name']} Remote Station",
                        "version": "1.0.0",
                    },
                    "capabilities": {"experimentalApi": True},
                },
            )
            self.notify("initialized", {})
        except BaseException:
            self.close()
            raise
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def _queue_message(self, payload: bytes) -> None:
        try:
            message = json.loads(payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return
        if isinstance(message, dict):
            queue_for_message = (
                self._messages if "id" in message else self._notifications
            )
            queue_for_message.put(message)

    def _transport_closed(self) -> None:
        self._messages.put(None)
        self._notifications.put(None)

    def _read_stderr(self, stream: BinaryIO) -> None:
        for line in stream:
            self._stderr.append(line.decode("utf-8", errors="replace").rstrip())
            del self._stderr[:-20]

    def _send(self, message: dict[str, Any]) -> None:
        raise NotImplementedError

    def request(self, method: str, params: dict[str, Any]) -> dict[str, Any]:
        request_id = self._next_id
        self._next_id += 1
        self._send({"method": method, "id": request_id, "params": params})
        deadline = time.monotonic() + self._timeout

        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise CodexStationError(f"Codex App Server timed out during {method}")
            try:
                message = self._messages.get(timeout=remaining)
            except queue.Empty as error:
                raise CodexStationError(
                    f"Codex App Server timed out during {method}"
                ) from error
            if message is None:
                detail = "; ".join(self._stderr) or "no diagnostic"
                raise CodexStationError(
                    f"Codex App Server transport exited during {method}: {detail}"
                )
            if message.get("id") != request_id:
                continue
            if "error" in message:
                rpc_error = message["error"]
                raise CodexStationError(
                    f"Codex App Server rejected {method}: {rpc_error}",
                    method=method,
                    rpc_error=rpc_error if isinstance(rpc_error, dict) else None,
                )
            result = message.get("result")
            if not isinstance(result, dict):
                raise CodexStationError(
                    f"Codex App Server returned a malformed {method} response"
                )
            return result

    def notify(self, method: str, params: dict[str, Any]) -> None:
        self._send({"method": method, "params": params})

    def wait_for_notification(
        self,
        method: str,
        predicate: Callable[[dict[str, Any]], bool],
        *,
        timeout: float,
    ) -> dict[str, Any]:
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise CodexStationError(
                    f"Codex App Server timed out waiting for {method}"
                )
            try:
                message = self._notifications.get(timeout=remaining)
            except queue.Empty as error:
                raise CodexStationError(
                    f"Codex App Server timed out waiting for {method}"
                ) from error
            if message is None:
                detail = "; ".join(self._stderr) or "no diagnostic"
                raise CodexStationError(
                    f"Codex App Server transport exited while waiting for "
                    f"{method}: {detail}"
                )
            params = message.get("params")
            if (
                message.get("method") == method
                and isinstance(params, dict)
                and predicate(params)
            ):
                return params

    def diagnostic(self) -> str:
        """Return recent transport diagnostics without exposing another stream."""
        return "; ".join(self._stderr)

    def close(self) -> None:
        process = getattr(self, "_process", None)
        if process is None:
            return
        stdin = getattr(self, "_stdin", None)
        if stdin is not None:
            try:
                stdin.close()
            except OSError:
                pass
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        for stream in (process.stdout, process.stderr):
            if stream is not None:
                try:
                    stream.close()
                except OSError:
                    pass
        self._process = None


class AppServerClient(_AppServerRpcClient):
    """Own a portable JSONL App Server child over its stdio transport."""

    def __init__(
        self,
        *,
        cwd: pathlib.Path,
        timeout: float = 30.0,
        popen: Callable[..., subprocess.Popen[bytes]] = subprocess.Popen,
    ) -> None:
        super().__init__(timeout)
        try:
            self._process = popen(
                ["codex", "app-server", "--listen", "stdio://"],
                cwd=cwd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                bufsize=0,
            )
        except OSError as error:
            raise CodexStationError(
                f"could not start Codex App Server: {error}"
            ) from error

        if self._process.stdin is None or self._process.stdout is None:
            self.close()
            raise CodexStationError("Codex App Server did not expose stdio")

        self._stdin: BinaryIO = self._process.stdin
        self._stdout: BinaryIO = self._process.stdout
        threading.Thread(target=self._read_messages, daemon=True).start()
        if self._process.stderr is not None:
            threading.Thread(
                target=self._read_stderr,
                args=(self._process.stderr,),
                daemon=True,
            ).start()

    def _read_messages(self) -> None:
        try:
            for line in self._stdout:
                self._queue_message(line)
        except OSError as error:
            self._stderr.append(str(error))
        finally:
            self._transport_closed()

    def _send(self, message: dict[str, Any]) -> None:
        payload = (
            json.dumps(message, separators=(",", ":")).encode("utf-8") + b"\n"
        )
        try:
            with self._write_lock:
                self._stdin.write(payload)
                self._stdin.flush()
        except (BrokenPipeError, OSError) as error:
            raise CodexStationError(
                f"Codex App Server closed while sending a request: {error}"
            ) from error


class ProxyAppServerClient(_AppServerRpcClient):
    """Connect to the shared App Server through the CLI WebSocket proxy."""

    def __init__(
        self,
        *,
        cwd: pathlib.Path,
        timeout: float = 30.0,
        connect_timeout: float | None = None,
        popen: Callable[..., subprocess.Popen[bytes]] = subprocess.Popen,
    ) -> None:
        super().__init__(timeout)
        self._connect_timeout = (
            timeout if connect_timeout is None else connect_timeout
        )
        self._read_condition = threading.Condition()
        self._read_buffer = bytearray()
        self._read_closed = False
        self._read_error: str | None = None
        try:
            self._process = popen(
                ["codex", "app-server", "proxy"],
                cwd=cwd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                bufsize=0,
            )
        except OSError as error:
            raise CodexStationError(
                f"could not start the Codex App Server proxy: {error}"
            ) from error

        if self._process.stdin is None or self._process.stdout is None:
            self.close()
            raise CodexStationError("Codex App Server proxy did not expose stdio")

        self._stdin: BinaryIO = self._process.stdin
        self._stdout: BinaryIO = self._process.stdout
        threading.Thread(target=self._read_transport, daemon=True).start()
        if self._process.stderr is not None:
            threading.Thread(
                target=self._read_stderr,
                args=(self._process.stderr,),
                daemon=True,
            ).start()
        try:
            self._open_websocket()
        except BaseException:
            self.close()
            raise
        threading.Thread(target=self._read_messages, daemon=True).start()

    def _read_transport(self) -> None:
        try:
            while chunk := self._stdout.read(4096):
                with self._read_condition:
                    self._read_buffer.extend(chunk)
                    self._read_condition.notify_all()
        except OSError as error:
            with self._read_condition:
                self._read_error = str(error)
        finally:
            with self._read_condition:
                self._read_closed = True
                self._read_condition.notify_all()

    def _read_exact(self, length: int, *, deadline: float | None = None) -> bytes:
        with self._read_condition:
            while len(self._read_buffer) < length:
                if self._read_closed:
                    detail = self._read_error or self.diagnostic() or "no diagnostic"
                    raise CodexStationError(
                        f"Codex App Server proxy closed unexpectedly: {detail}"
                    )
                remaining = None if deadline is None else deadline - time.monotonic()
                if remaining is not None and remaining <= 0:
                    raise CodexStationError(
                        "Codex App Server proxy timed out during WebSocket setup"
                    )
                self._read_condition.wait(timeout=remaining)
            value = bytes(self._read_buffer[:length])
            del self._read_buffer[:length]
            return value

    def _read_until(self, marker: bytes, *, deadline: float) -> bytes:
        with self._read_condition:
            while True:
                position = self._read_buffer.find(marker)
                if position >= 0:
                    end = position + len(marker)
                    value = bytes(self._read_buffer[:end])
                    del self._read_buffer[:end]
                    return value
                if self._read_closed:
                    detail = self._read_error or self.diagnostic() or "no diagnostic"
                    raise CodexStationError(
                        f"Codex App Server proxy closed during WebSocket setup: {detail}"
                    )
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise CodexStationError(
                        "Codex App Server proxy timed out during WebSocket setup"
                    )
                self._read_condition.wait(timeout=remaining)

    def _write_bytes(self, payload: bytes) -> None:
        try:
            with self._write_lock:
                self._stdin.write(payload)
                self._stdin.flush()
        except (BrokenPipeError, OSError) as error:
            raise CodexStationError(
                f"Codex App Server proxy closed while sending data: {error}"
            ) from error

    def _open_websocket(self) -> None:
        key = base64.b64encode(secrets.token_bytes(16)).decode("ascii")
        request = (
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        ).encode("ascii")
        self._write_bytes(request)
        response = self._read_until(
            b"\r\n\r\n", deadline=time.monotonic() + self._connect_timeout
        )
        status = response.split(b"\r\n", maxsplit=1)[0]
        if not status.startswith(b"HTTP/1.1 101 "):
            detail = response.decode("utf-8", errors="replace").strip()
            raise CodexStationError(
                f"Codex App Server proxy rejected the WebSocket upgrade: {detail}"
            )
        expected = base64.b64encode(
            hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")
            ).digest()
        ).decode("ascii")
        headers: dict[str, str] = {}
        for line in response.decode("ascii", errors="replace").split("\r\n")[1:]:
            if ":" not in line:
                continue
            name, value = line.split(":", maxsplit=1)
            headers[name.strip().lower()] = value.strip()
        if headers.get("sec-websocket-accept") != expected:
            raise CodexStationError(
                "Codex App Server proxy returned an invalid WebSocket handshake"
            )

    def _write_frame(self, opcode: int, payload: bytes) -> None:
        first = 0x80 | opcode
        length = len(payload)
        if length < 126:
            header = bytes((first, 0x80 | length))
        elif length < 65536:
            header = bytes((first, 0x80 | 126)) + length.to_bytes(2, "big")
        else:
            header = bytes((first, 0x80 | 127)) + length.to_bytes(8, "big")
        mask = secrets.token_bytes(4)
        masked = bytes(
            byte ^ mask[index % len(mask)] for index, byte in enumerate(payload)
        )
        self._write_bytes(header + mask + masked)

    def _read_frame(self) -> tuple[bool, int, bytes]:
        first, second = self._read_exact(2)
        length = second & 0x7F
        if length == 126:
            length = int.from_bytes(self._read_exact(2), "big")
        elif length == 127:
            length = int.from_bytes(self._read_exact(8), "big")
        mask = self._read_exact(4) if second & 0x80 else None
        payload = self._read_exact(length)
        if mask is not None:
            payload = bytes(
                byte ^ mask[index % len(mask)] for index, byte in enumerate(payload)
            )
        return bool(first & 0x80), first & 0x0F, payload

    def _read_messages(self) -> None:
        fragments = bytearray()
        try:
            while True:
                finished, opcode, payload = self._read_frame()
                if opcode == 0x8:
                    return
                if opcode == 0x9:
                    self._write_frame(0xA, payload)
                    continue
                if opcode == 0xA:
                    continue
                if opcode == 0x1:
                    fragments = bytearray(payload)
                elif opcode == 0x0 and fragments:
                    fragments.extend(payload)
                else:
                    continue
                if finished:
                    self._queue_message(bytes(fragments))
                    fragments.clear()
        except (CodexStationError, OSError) as error:
            self._stderr.append(str(error))
        finally:
            self._transport_closed()

    def _send(self, message: dict[str, Any]) -> None:
        payload = json.dumps(message, separators=(",", ":")).encode("utf-8")
        self._write_frame(0x1, payload)


def _normalized_path(path: str | pathlib.Path) -> str:
    return os.path.normcase(str(pathlib.Path(path).resolve()))


def _git_common_dir(path: pathlib.Path) -> str | None:
    completed = subprocess.run(
        [
            "git",
            "-C",
            str(path),
            "rev-parse",
            "--path-format=absolute",
            "--git-common-dir",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if completed.returncode != 0 or not completed.stdout.strip():
        return None
    return _normalized_path(completed.stdout.strip())


def _list_projects(client: _AppServerRpcClient) -> list[dict[str, Any]]:
    projects: list[dict[str, Any]] = []
    cursor: str | None = None
    seen_cursors: set[str] = set()
    while True:
        result = client.request("project/list", {"cursor": cursor, "limit": 100})
        data = result.get("data")
        if not isinstance(data, list):
            raise CodexStationError(
                "Codex App Server returned a malformed project/list response"
            )
        projects.extend(project for project in data if isinstance(project, dict))

        next_cursor = result.get("nextCursor")
        if next_cursor is None:
            return projects
        if not isinstance(next_cursor, str) or next_cursor in seen_cursors:
            raise CodexStationError(
                "Codex App Server returned an invalid project pagination cursor"
            )
        seen_cursors.add(next_cursor)
        cursor = next_cursor


def _project_id_for_cwd(
    client: _AppServerRpcClient,
    cwd: pathlib.Path,
) -> str | None:
    """Resolve one saved Desktop project and register a linked worktree root."""
    normalized_cwd = _normalized_path(cwd)
    common_dir = _git_common_dir(cwd)
    exact_ids: set[str] = set()
    repository_ids: set[str] = set()
    projects_by_id: dict[str, dict[str, Any]] = {}

    for project in _list_projects(client):
        project_id = project.get("id")
        roots = project.get("roots")
        if not isinstance(project_id, str) or not isinstance(roots, list):
            continue
        projects_by_id[project_id] = project
        for root in roots:
            root_path = root.get("path") if isinstance(root, dict) else None
            if not isinstance(root_path, str):
                continue
            if _normalized_path(root_path) == normalized_cwd:
                exact_ids.add(project_id)
            elif common_dir is not None:
                candidate_common_dir = _git_common_dir(pathlib.Path(root_path))
                if candidate_common_dir == common_dir:
                    repository_ids.add(project_id)

    matches = exact_ids or repository_ids
    if len(matches) > 1:
        ids = ", ".join(sorted(matches))
        raise CodexStationError(
            f"multiple Codex projects match checkout {cwd}: {ids}"
        )
    project_id = next(iter(matches), None)
    if project_id is None or project_id in exact_ids:
        return project_id

    project = projects_by_id[project_id]
    roots = project["roots"]
    updated = client.request(
        "project/update",
        {
            "projectId": project_id,
            "roots": [*roots, {"path": str(cwd)}],
        },
    ).get("project")
    if not isinstance(updated, dict) or updated.get("id") != project_id:
        raise CodexStationError(
            "Codex App Server returned a malformed project/update response"
        )
    updated_roots = updated.get("roots")
    if not isinstance(updated_roots, list) or not any(
        isinstance(root, dict)
        and isinstance(root.get("path"), str)
        and _normalized_path(root["path"]) == normalized_cwd
        for root in updated_roots
    ):
        raise CodexStationError(
            f"Codex project {project_id} did not retain worktree root {cwd}"
        )
    return project_id


def _assign_project(
    client: _AppServerRpcClient,
    *,
    thread: dict[str, Any],
    project_id: str | None,
) -> dict[str, Any]:
    if project_id is None or thread.get("projectId") == project_id:
        return thread
    thread_id = str(thread["id"])
    updated = _thread_from_result(
        client.request(
            "thread/metadata/update",
            {"threadId": thread_id, "projectId": project_id},
        ),
        "thread/metadata/update",
    )
    if updated.get("projectId") != project_id:
        raise CodexStationError(
            f"Codex task {thread_id} did not retain project assignment {project_id!r}"
        )
    return updated


def _thread_from_result(result: dict[str, Any], operation: str) -> dict[str, Any]:
    thread = result.get("thread")
    if not isinstance(thread, dict) or not isinstance(thread.get("id"), str):
        raise CodexStationError(
            f"Codex App Server returned a malformed {operation} thread"
        )
    return thread


def _list_named_threads(
    client: _AppServerRpcClient,
    *,
    cwd: pathlib.Path,
    name: str,
    archived: bool = False,
) -> list[dict[str, Any]]:
    matches: list[dict[str, Any]] = []
    normalized_cwd = _normalized_path(cwd)
    cursor: str | None = None
    seen_cursors: set[str] = set()
    while True:
        result = client.request(
            "thread/list",
            {
                "archived": archived,
                "cursor": cursor,
                "cwd": str(cwd),
                "limit": 100,
                "searchTerm": name,
                "sourceKinds": list(THREAD_SOURCE_KINDS),
                "sortDirection": "desc",
                "sortKey": "updated_at",
            },
        )
        data = result.get("data")
        if not isinstance(data, list):
            raise CodexStationError(
                "Codex App Server returned a malformed thread/list response"
            )
        for thread in data:
            if not isinstance(thread, dict):
                continue
            if thread.get("name") != name:
                continue
            thread_cwd = thread.get("cwd")
            if (
                isinstance(thread_cwd, str)
                and _normalized_path(thread_cwd) == normalized_cwd
            ):
                matches.append(thread)

        next_cursor = result.get("nextCursor")
        if next_cursor is None:
            return matches
        if not isinstance(next_cursor, str) or next_cursor in seen_cursors:
            raise CodexStationError(
                "Codex App Server returned an invalid pagination cursor"
            )
        seen_cursors.add(next_cursor)
        cursor = next_cursor


def _read_thread(
    client: _AppServerRpcClient,
    thread_id: str,
    *,
    include_turns: bool = False,
) -> dict[str, Any]:
    return _thread_from_result(
        client.request(
            "thread/read",
            {"threadId": thread_id, "includeTurns": include_turns},
        ),
        "thread/read",
    )


def _thread_turns(thread: dict[str, Any], operation: str) -> list[dict[str, Any]]:
    turns = thread.get("turns")
    if not isinstance(turns, list) or not all(
        isinstance(turn, dict) for turn in turns
    ):
        raise CodexStationError(
            f"Codex App Server returned malformed turns during {operation}"
        )
    return turns


def _turn_from_result(result: dict[str, Any], operation: str) -> dict[str, Any]:
    turn = result.get("turn")
    if not isinstance(turn, dict) or not isinstance(turn.get("id"), str):
        raise CodexStationError(
            f"Codex App Server returned a malformed {operation} turn"
        )
    return turn


def _materialize_thread(
    client: _AppServerRpcClient,
    *,
    cwd: pathlib.Path,
    thread_id: str,
    timeout: float = STATION_BOOTSTRAP_TIMEOUT_SECONDS,
) -> None:
    """Run the first harmless turn so Managed Remote indexes a new task."""
    turn = _turn_from_result(
        client.request(
            "turn/start",
            {
                "approvalPolicy": "never",
                "cwd": str(cwd),
                "input": [{"type": "text", "text": STATION_BOOTSTRAP_TEXT}],
                "sandboxPolicy": {"type": "readOnly", "networkAccess": False},
                "threadId": thread_id,
            },
        ),
        "turn/start",
    )
    turn_id = str(turn["id"])
    status = turn.get("status")
    while status not in TERMINAL_TURN_STATUSES:
        completed = client.wait_for_notification(
            "turn/completed",
            lambda params: params.get("threadId") == thread_id
            and isinstance(params.get("turn"), dict)
            and params["turn"].get("id") == turn_id,
            timeout=timeout,
        )
        turn = _turn_from_result(completed, "turn/completed")
        status = turn.get("status")

    if status != "completed":
        error = turn.get("error")
        detail = error.get("message") if isinstance(error, dict) else None
        raise CodexStationError(
            f"Codex station bootstrap turn {turn_id} ended with status {status!r}"
            + (f": {detail}" if isinstance(detail, str) and detail else "")
        )


def _is_station_bootstrap_turn(turn: dict[str, Any]) -> bool:
    """Recognize the exact non-mutating turn created by this provisioner."""
    if turn.get("status") != "completed" or turn.get("itemsView", "full") != "full":
        return False
    items = turn.get("items")
    if not isinstance(items, list) or not all(
        isinstance(item, dict) for item in items
    ):
        return False

    allowed_types = {"userMessage", "agentMessage", "reasoning", "hookPrompt"}
    if any(item.get("type") not in allowed_types for item in items):
        return False

    user_messages = [item for item in items if item.get("type") == "userMessage"]
    if len(user_messages) != 1:
        return False
    content = user_messages[0].get("content")
    if not isinstance(content, list) or len(content) != 1:
        return False
    user_input = content[0]
    if not isinstance(user_input, dict) or user_input.get("type") != "text":
        return False
    return user_input.get("text") == STATION_BOOTSTRAP_TEXT


def _is_disposable_registered_task(thread: dict[str, Any]) -> bool:
    turns = _thread_turns(thread, "duplicate recovery")
    return not turns or (len(turns) == 1 and _is_station_bootstrap_turn(turns[0]))


def _is_active_writer_error(
    error: CodexStationError, thread_id: str, method: str = "thread/resume"
) -> bool:
    rpc_error = error.rpc_error
    return (
        error.method == method
        and isinstance(rpc_error, dict)
        and rpc_error.get("code") == -32600
        and rpc_error.get("message")
        == f"thread {thread_id} already has an active writer"
    )


def _archive_idle_thread(client: _AppServerRpcClient, thread_id: str) -> None:
    """Archive the reset target or explain how to release its owning writer."""
    params = {"threadId": thread_id}
    try:
        client.request("thread/archive", params)
    except CodexStationError as error:
        if not _is_active_writer_error(error, thread_id, "thread/archive"):
            raise
        thread = _read_thread(client, thread_id, include_turns=True)
        if any(
            turn.get("status") not in TERMINAL_TURN_STATUSES
            for turn in _thread_turns(thread, "archive writer recovery")
        ):
            raise CodexStationError(
                f"Codex task {thread_id} has an active turn; stop it before resetting"
            ) from error
        raise CodexStationError(
            f"Codex task {thread_id} is idle but another App Server owns its writer. "
            "Archive this exact task in Codex Desktop, then rerun with --fresh codex "
            "(or --fresh for both runtimes). The standalone helper cannot release "
            "Desktop's writer with thread/unsubscribe."
        ) from error


def _resume_thread(
    client: _AppServerRpcClient,
    *,
    cwd: pathlib.Path,
    thread: dict[str, Any],
) -> dict[str, Any]:
    """Resume a task, recovering one stale writer when no turn is active."""
    thread_id = str(thread["id"])
    params = {
        "approvalPolicy": "never",
        "cwd": str(cwd),
        "sandbox": STATION_SANDBOX,
        "threadId": thread_id,
    }
    try:
        result = client.request("thread/resume", params)
    except CodexStationError as error:
        if not _is_active_writer_error(error, thread_id):
            raise
        turns = _thread_turns(thread, "stale-writer recovery")
        has_active_turn = any(
            turn.get("status") not in TERMINAL_TURN_STATUSES for turn in turns
        )
        if has_active_turn:
            raise
        client.request("thread/unsubscribe", {"threadId": thread_id})
        result = client.request("thread/resume", params)
    return _thread_from_result(result, "thread/resume")


def _recover_registered_replacement(
    client: _AppServerRpcClient,
    *,
    cwd: pathlib.Path,
    name: str,
    matches: list[dict[str, Any]],
    expected_thread_id: str | None,
) -> tuple[list[dict[str, Any]], bool]:
    """Archive one empty or bootstrap-only registered task for a real replacement."""
    if expected_thread_id is None or len(matches) not in (1, 2):
        return matches, False

    replacements = [
        thread for thread in matches if thread.get("id") != expected_thread_id
    ]
    expected_was_listed = any(
        thread.get("id") == expected_thread_id for thread in matches
    )
    if len(replacements) != 1 or (len(matches) == 2 and not expected_was_listed):
        return matches, False

    try:
        expected_full = _read_thread(client, expected_thread_id, include_turns=True)
    except CodexStationError:
        return matches, False
    replacement_id = replacements[0].get("id")
    if not isinstance(replacement_id, str):
        return matches, False
    replacement_full = _read_thread(client, replacement_id, include_turns=True)

    expected_cwd = expected_full.get("cwd")
    expected_name = expected_full.get("name")
    expected_is_disposable = (
        isinstance(expected_cwd, str)
        and _normalized_path(expected_cwd) == _normalized_path(cwd)
        and expected_name in (None, "", name)
        and _is_disposable_registered_task(expected_full)
    )
    replacement_has_user_history = not _is_disposable_registered_task(
        replacement_full
    )
    if not expected_is_disposable or not replacement_has_user_history:
        return matches, False

    if expected_full.get("archived") is not True:
        client.request("thread/archive", {"threadId": expected_thread_id})
    return [replacement_full], True


def provision_named_thread(
    client: _AppServerRpcClient,
    *,
    cwd: pathlib.Path,
    name: str,
    expected_thread_id: str | None,
    new_session: bool = False,
) -> str:
    """Create or verify the only task bound to ``name`` and ``cwd``."""
    cwd = cwd.resolve()
    project_id = _project_id_for_cwd(client, cwd)
    matches = _list_named_threads(client, cwd=cwd, name=name)
    matches, recovered_registered_task = _recover_registered_replacement(
        client,
        cwd=cwd,
        name=name,
        matches=matches,
        expected_thread_id=None if new_session else expected_thread_id,
    )
    if recovered_registered_task:
        expected_thread_id = None
    if len(matches) > 1:
        ids = ", ".join(str(thread.get("id")) for thread in matches)
        raise CodexStationError(
            f"multiple Codex tasks match station {name!r} in {cwd}: {ids}"
        )

    subscribed = False
    if expected_thread_id is not None:
        if matches and matches[0].get("id") != expected_thread_id:
            raise CodexStationError(
                f"station {name!r} is already used by Codex task "
                f"{matches[0].get('id')}; expected {expected_thread_id}"
            )
        thread = _read_thread(client, expected_thread_id, include_turns=True)
        thread_id = expected_thread_id
    elif matches:
        thread_id = str(matches[0]["id"])
        thread = _read_thread(client, thread_id, include_turns=True)
    else:
        start_params: dict[str, Any] = {
            "approvalPolicy": "never",
            "cwd": str(cwd),
            "ephemeral": False,
            "sandbox": STATION_SANDBOX,
        }
        if project_id is not None:
            start_params["projectId"] = project_id
        thread = _thread_from_result(
            client.request("thread/start", start_params),
            "thread/start",
        )
        thread_id = str(thread["id"])
        thread["turns"] = []
        subscribed = True

    succeeded = False
    try:
        thread_cwd = thread.get("cwd")
        if (
            not isinstance(thread_cwd, str)
            or _normalized_path(thread_cwd) != _normalized_path(cwd)
        ):
            raise CodexStationError(
                f"Codex task {thread_id} belongs to {thread_cwd!r}, not {str(cwd)!r}"
            )

        current_name = thread.get("name")
        if current_name not in (None, "", name):
            raise CodexStationError(
                f"Codex task {thread_id} is named {current_name!r}, not {name!r}; "
                "refusing to rename a task with unrelated history"
            )

        turns = _thread_turns(thread, "thread/read")
        if any(
            turn.get("status") not in TERMINAL_TURN_STATUSES for turn in turns
        ):
            raise CodexStationError(
                f"Codex task {thread_id} has an active turn; refusing to provision it"
            )

        if new_session and not subscribed:
            # Only replace the validated station binding, never an active or
            # unrelated task. Keep its history available in the archive.
            # thread/read does not reliably include an archived flag. Confirm
            # against the archived catalog before trying to archive again.
            archived_matches = _list_named_threads(
                client, cwd=cwd, name=name, archived=True
            )
            already_archived = any(
                match.get("id") == thread_id for match in archived_matches
            )
            if not already_archived:
                _archive_idle_thread(client, thread_id)
            return provision_named_thread(
                client, cwd=cwd, name=name, expected_thread_id=None
            )

        if thread.get("archived") is True:
            client.request("thread/unarchive", {"threadId": thread_id})

        if not turns:
            if not subscribed:
                thread = _resume_thread(client, cwd=cwd, thread=thread)
                subscribed = True
            # A fresh task may not have a rollout until its first turn finishes.
            _materialize_thread(client, cwd=cwd, thread_id=thread_id)
            thread = _read_thread(client, thread_id, include_turns=True)

        thread = _assign_project(client, thread=thread, project_id=project_id)
        # Desktop tasks can be readable even when name/set cannot find a rollout.
        # Avoid that mutation when the persisted name is already correct.
        if thread.get("name") != name:
            client.request("thread/name/set", {"threadId": thread_id, "name": name})
        verified = _read_thread(client, thread_id, include_turns=True)
        if verified.get("name") != name:
            raise CodexStationError(
                f"Codex task {thread_id} did not retain station name {name!r}"
            )
        if _normalized_path(str(verified.get("cwd", ""))) != _normalized_path(cwd):
            raise CodexStationError(
                f"Codex task {thread_id} changed checkout while being provisioned"
            )
        if project_id is not None and verified.get("projectId") != project_id:
            raise CodexStationError(
                f"Codex task {thread_id} changed project while being provisioned"
            )
        if not _thread_turns(verified, "thread/read"):
            raise CodexStationError(
                f"Codex task {thread_id} did not retain its bootstrap turn"
            )
        succeeded = True
        return thread_id
    finally:
        if subscribed:
            try:
                client.request("thread/unsubscribe", {"threadId": thread_id})
            except CodexStationError:
                if succeeded:
                    raise


def release_named_threads(
    client: _AppServerRpcClient,
    *,
    cwd: pathlib.Path,
    name: str,
    recorded_thread_id: str | None,
) -> list[str]:
    """Archive every task that would answer to ``name`` in ``cwd``, and report it.

    Starting a station over has to leave nothing a later launch would adopt:
    the recorded identity, whatever it happens to be named now, and anything
    already carrying the station name. Archiving is Codex's own reversible
    form of forgetting — the task's history stays readable, it simply stops
    being the station — so a fresh start is never a deletion.
    """
    cwd = cwd.resolve()
    candidates: list[str] = []
    if recorded_thread_id:
        candidates.append(recorded_thread_id)
    for thread in _list_named_threads(client, cwd=cwd, name=name):
        thread_id = thread.get("id")
        if isinstance(thread_id, str) and thread_id not in candidates:
            candidates.append(thread_id)

    released: list[str] = []
    for thread_id in candidates:
        try:
            thread = _read_thread(client, thread_id)
        except CodexStationError:
            released.append(f"{thread_id} no longer exists")
            continue
        thread_cwd = thread.get("cwd")
        if (
            not isinstance(thread_cwd, str)
            or _normalized_path(thread_cwd) != _normalized_path(cwd)
        ):
            raise CodexStationError(
                f"Codex task {thread_id} belongs to {thread_cwd!r}, not "
                f"{str(cwd)!r}; refusing to archive another checkout's task"
            )
        if thread.get("archived") is True:
            released.append(f"{thread_id} was already archived")
            continue
        client.request("thread/archive", {"threadId": thread_id})
        released.append(f"{thread_id} archived")
    return released
