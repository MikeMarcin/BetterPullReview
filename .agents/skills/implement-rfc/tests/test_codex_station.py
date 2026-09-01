from __future__ import annotations

import base64
import hashlib
import io
import json
import socket
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path

SCRIPTS_ROOT = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS_ROOT))

import codex_station  # noqa: E402


class FakeStdioProcess:
    """JSONL-speaking stand-in for `codex app-server --listen stdio://`."""

    def __init__(self, *, malformed_result: bool = False) -> None:
        self._client, self._server = socket.socketpair()
        self.stdin = self._client.makefile("wb", buffering=0)
        self.stdout = self._client.makefile("rb", buffering=0)
        self.stderr = io.BytesIO()
        self.messages: list[dict] = []
        self._malformed_result = malformed_result
        self._returncode: int | None = None
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()

    def _serve(self) -> None:
        stream = self._server.makefile("rwb", buffering=0)
        try:
            for line in stream:
                message = json.loads(line)
                self.messages.append(message)
                if "id" not in message:
                    continue
                if message["method"] == "initialize":
                    result = {"userAgent": "fake"}
                elif self._malformed_result:
                    result = []
                else:
                    result = {"data": [], "nextCursor": None}
                stream.write(
                    json.dumps({"id": message["id"], "result": result}).encode()
                    + b"\n"
                )
        except (OSError, ValueError):
            pass
        finally:
            stream.close()

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0
        try:
            self._server.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self._server.close()

    kill = terminate

    def wait(self, timeout: float | None = None) -> int:
        self._thread.join(timeout)
        self._client.close()
        return self._returncode or 0


class FakeProxyProcess:
    """WebSocket-speaking stand-in for `codex app-server proxy`."""

    def __init__(self, *, malformed_result: bool = False) -> None:
        self._client, self._server = socket.socketpair()
        self.stdin = self._client.makefile("wb", buffering=0)
        self.stdout = self._client.makefile("rb", buffering=0)
        self.stderr = io.BytesIO()
        self.messages: list[dict] = []
        self._malformed_result = malformed_result
        self._returncode: int | None = None
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()

    @staticmethod
    def _read_exact(stream: socket.socket, length: int) -> bytes:
        value = bytearray()
        while len(value) < length:
            chunk = stream.recv(length - len(value))
            if not chunk:
                raise OSError("proxy closed")
            value.extend(chunk)
        return bytes(value)

    def _read_frame(self) -> bytes:
        first, second = self._read_exact(self._server, 2)
        self.assert_text_frame(first)
        length = second & 0x7F
        if length == 126:
            length = int.from_bytes(self._read_exact(self._server, 2), "big")
        elif length == 127:
            length = int.from_bytes(self._read_exact(self._server, 8), "big")
        mask = self._read_exact(self._server, 4)
        payload = self._read_exact(self._server, length)
        return bytes(
            byte ^ mask[index % len(mask)] for index, byte in enumerate(payload)
        )

    @staticmethod
    def assert_text_frame(first: int) -> None:
        if first & 0x0F != 0x1:
            raise OSError("expected text frame")

    def _write_frame(self, payload: bytes) -> None:
        length = len(payload)
        if length < 126:
            header = bytes((0x81, length))
        elif length < 65536:
            header = bytes((0x81, 126)) + length.to_bytes(2, "big")
        else:
            header = bytes((0x81, 127)) + length.to_bytes(8, "big")
        self._server.sendall(header + payload)

    def _serve(self) -> None:
        try:
            request = bytearray()
            while b"\r\n\r\n" not in request:
                request.extend(self._server.recv(4096))
            headers = request.decode("ascii").split("\r\n")
            key = next(
                line.split(":", maxsplit=1)[1].strip()
                for line in headers
                if line.lower().startswith("sec-websocket-key:")
            )
            accept = base64.b64encode(
                hashlib.sha1(
                    (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode(
                        "ascii"
                    )
                ).digest()
            ).decode("ascii")
            self._server.sendall(
                (
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    f"Sec-WebSocket-Accept: {accept}\r\n\r\n"
                ).encode("ascii")
            )
            while True:
                message = json.loads(self._read_frame())
                self.messages.append(message)
                if "id" not in message:
                    continue
                if message["method"] == "initialize":
                    result = {"userAgent": "fake"}
                elif self._malformed_result:
                    result = []
                else:
                    result = {"data": [], "nextCursor": None}
                self._write_frame(
                    json.dumps({"id": message["id"], "result": result}).encode()
                )
        except (OSError, ValueError, StopIteration):
            pass

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0
        try:
            self._server.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self._server.close()

    kill = terminate

    def wait(self, timeout: float | None = None) -> int:
        self._thread.join(timeout)
        self._client.close()
        return self._returncode or 0


class AppServerClientTests(unittest.TestCase):
    def test_malformed_rpc_result_is_refused(self) -> None:
        process = FakeStdioProcess(malformed_result=True)
        with codex_station.AppServerClient(
            cwd=Path.cwd(),
            timeout=1,
            popen=lambda *args, **kwargs: process,
        ) as client:
            with self.assertRaisesRegex(
                codex_station.CodexStationError, "malformed"
            ):
                client.request("thread/list", {})

    def test_portable_stdio_transport_initializes_and_sends_jsonl(self) -> None:
        process = FakeStdioProcess()
        commands: list[list[str]] = []

        def start(command: list[str], **_: object) -> FakeStdioProcess:
            commands.append(command)
            return process

        with codex_station.AppServerClient(
            cwd=Path.cwd(),
            timeout=1,
            popen=start,
        ) as client:
            result = client.request("thread/list", {})

        self.assertEqual(
            commands,
            [["codex", "app-server", "--listen", "stdio://"]],
        )
        self.assertEqual(result["data"], [])
        initialize = process.messages[0]
        self.assertTrue(initialize["params"]["capabilities"]["experimentalApi"])


class ProxyAppServerClientTests(unittest.TestCase):
    def test_malformed_rpc_result_is_refused(self) -> None:
        process = FakeProxyProcess(malformed_result=True)
        with codex_station.ProxyAppServerClient(
            cwd=Path.cwd(),
            timeout=1,
            popen=lambda *args, **kwargs: process,
        ) as client:
            with self.assertRaisesRegex(codex_station.CodexStationError, "malformed"):
                client.request("thread/list", {})

    def test_app_server_proxy_uses_websocket_frames(self) -> None:
        process = FakeProxyProcess()
        commands: list[list[str]] = []

        def start(command: list[str], **_: object) -> FakeProxyProcess:
            commands.append(command)
            return process

        with codex_station.ProxyAppServerClient(
            cwd=Path.cwd(),
            timeout=1,
            popen=start,
        ) as client:
            result = client.request("thread/list", {})

        self.assertEqual(commands, [["codex", "app-server", "proxy"]])
        self.assertEqual(result["data"], [])
        self.assertEqual(process.messages[0]["method"], "initialize")


class FakeClient:
    def __init__(
        self,
        threads: list[dict] | None = None,
        *,
        projects: list[dict] | None = None,
        turn_status: str = "completed",
        stale_writer_thread_ids: set[str] | None = None,
    ) -> None:
        self.threads = {
            str(thread["id"]): {"turns": [], **dict(thread)}
            for thread in (threads or [])
        }
        self.calls: list[tuple[str, dict]] = []
        self.next_id = 1
        self.next_turn_id = 1
        self.turn_status = turn_status
        self.projects = list(projects or [])
        self.stale_writer_thread_ids = set(stale_writer_thread_ids or ())

    def request(self, method: str, params: dict) -> dict:
        self.calls.append((method, params))
        if method == "project/list":
            return {"data": list(self.projects), "nextCursor": None}
        if method == "project/update":
            project = next(
                project
                for project in self.projects
                if project["id"] == params["projectId"]
            )
            project["roots"] = list(params["roots"])
            return {"project": dict(project)}
        if method == "thread/list":
            cwd = params["cwd"]
            term = params["searchTerm"]
            data = [
                dict(thread)
                for thread in self.threads.values()
                if thread.get("cwd") == cwd
                and term in str(thread.get("name", ""))
                and not thread.get("archived", False)
            ]
            return {"data": data, "nextCursor": None}
        if method == "thread/start":
            thread_id = f"thr-new-{self.next_id:04d}"
            self.next_id += 1
            thread = {
                "id": thread_id,
                "cwd": params["cwd"],
                "name": None,
                "archived": False,
                "projectId": params.get("projectId"),
                "turns": [],
            }
            self.threads[thread_id] = thread
            return {"thread": dict(thread)}
        if method == "thread/read":
            if params["threadId"] not in self.threads:
                raise codex_station.CodexStationError("thread not found")
            return {"thread": dict(self.threads[params["threadId"]])}
        if method == "thread/resume":
            thread_id = params["threadId"]
            if thread_id in self.stale_writer_thread_ids:
                rpc_error = {
                    "code": -32600,
                    "message": f"thread {thread_id} already has an active writer",
                }
                raise codex_station.CodexStationError(
                    f"Codex App Server rejected thread/resume: {rpc_error}",
                    method="thread/resume",
                    rpc_error=rpc_error,
                )
            return {"thread": dict(self.threads[params["threadId"]])}
        if method == "thread/name/set":
            self.threads[params["threadId"]]["name"] = params["name"]
            return {}
        if method == "thread/metadata/update":
            thread = self.threads[params["threadId"]]
            thread["projectId"] = params["projectId"]
            return {"thread": dict(thread)}
        if method == "thread/unarchive":
            self.threads[params["threadId"]]["archived"] = False
            return {"thread": dict(self.threads[params["threadId"]])}
        if method == "thread/archive":
            self.threads[params["threadId"]]["archived"] = True
            return {"thread": dict(self.threads[params["threadId"]])}
        if method == "thread/unsubscribe":
            self.stale_writer_thread_ids.discard(params["threadId"])
            return {"status": "unsubscribed"}
        if method == "turn/start":
            turn = {
                "id": f"turn-new-{self.next_turn_id:04d}",
                "status": self.turn_status,
            }
            self.next_turn_id += 1
            self.threads[params["threadId"]]["turns"].append(turn)
            return {"turn": dict(turn)}
        raise AssertionError(f"unexpected method: {method}")

    def wait_for_notification(
        self,
        method: str,
        predicate: object,
        *,
        timeout: float,
    ) -> dict:
        self.calls.append(("wait_for_notification", {"method": method}))
        turn = next(
            turn
            for thread in self.threads.values()
            for turn in thread["turns"]
            if turn["status"] == "inProgress"
        )
        turn["status"] = "completed"
        thread_id = next(
            thread_id
            for thread_id, thread in self.threads.items()
            if turn in thread["turns"]
        )
        return {"threadId": thread_id, "turn": dict(turn)}


def station_bootstrap_turn(
    *,
    agent_text: str = "STATION READY",
    extra_items: list[dict] | None = None,
) -> dict:
    items = [
        {
            "id": "item-user",
            "type": "userMessage",
            "content": [
                {
                    "type": "text",
                    "text": codex_station.STATION_BOOTSTRAP_TEXT,
                    "text_elements": [],
                }
            ],
        },
        {
            "id": "item-agent",
            "type": "agentMessage",
            "text": agent_text,
        },
    ]
    items.extend(extra_items or [])
    return {
        "id": "turn-bootstrap",
        "status": "completed",
        "items": items,
        "itemsView": "full",
    }


class ProvisionNamedThreadTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temporary = tempfile.TemporaryDirectory()
        self.cwd = Path(self._temporary.name).resolve()
        self.name = "fx-main-mac"

    def tearDown(self) -> None:
        self._temporary.cleanup()

    def test_creates_names_and_verifies_a_fresh_task(self) -> None:
        client = FakeClient()

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id=None,
        )

        self.assertEqual(thread_id, "thr-new-0001")
        self.assertEqual(client.threads[thread_id]["name"], self.name)
        self.assertEqual(
            [method for method, _ in client.calls],
            [
                "project/list",
                "thread/list",
                "thread/start",
                "thread/name/set",
                "thread/read",
                "turn/start",
                "thread/name/set",
                "thread/read",
                "thread/unsubscribe",
            ],
        )
        start = client.calls[2][1]
        self.assertEqual(start["approvalPolicy"], "never")
        self.assertEqual(start["sandbox"], "danger-full-access")
        self.assertFalse(start["ephemeral"])
        bootstrap = next(
            params for method, params in client.calls if method == "turn/start"
        )
        self.assertIn("STATION READY", bootstrap["input"][0]["text"])
        self.assertEqual(
            bootstrap["sandboxPolicy"],
            {"type": "readOnly", "networkAccess": False},
        )
        self.assertNotIn("thread/resume", [method for method, _ in client.calls])

    def test_assigns_exact_saved_project_and_repairs_existing_task(self) -> None:
        project_id = "project-fixture"
        client = FakeClient(
            [
                {
                    "id": "thr-existing",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "projectId": None,
                    "turns": [{"id": "turn-existing", "status": "completed"}],
                }
            ],
            projects=[
                {
                    "id": project_id,
                    "name": "fixture",
                    "roots": [{"path": str(self.cwd)}],
                }
            ],
        )

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id="thr-existing",
        )

        self.assertEqual(thread_id, "thr-existing")
        self.assertEqual(client.threads[thread_id]["projectId"], project_id)
        assignment = next(
            params
            for method, params in client.calls
            if method == "thread/metadata/update"
        )
        self.assertEqual(assignment["projectId"], project_id)

    def test_fresh_task_is_created_inside_the_saved_project(self) -> None:
        client = FakeClient(
            projects=[
                {
                    "id": "project-fixture",
                    "name": "fixture",
                    "roots": [{"path": str(self.cwd)}],
                }
            ]
        )

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id=None,
        )

        start = next(params for method, params in client.calls if method == "thread/start")
        self.assertEqual(start["projectId"], "project-fixture")
        self.assertEqual(client.threads[thread_id]["projectId"], "project-fixture")
        self.assertFalse(
            any(method == "thread/metadata/update" for method, _ in client.calls)
        )

    def test_assigns_saved_project_to_a_linked_worktree(self) -> None:
        main = self.cwd / "main"
        acta = self.cwd / "acta"
        main.mkdir()
        subprocess.run(["git", "init", str(main)], check=True, capture_output=True)
        subprocess.run(
            ["git", "-C", str(main), "config", "user.email", "test@example.com"],
            check=True,
        )
        subprocess.run(
            ["git", "-C", str(main), "config", "user.name", "Station Test"],
            check=True,
        )
        subprocess.run(
            ["git", "-C", str(main), "commit", "--allow-empty", "-m", "init"],
            check=True,
            capture_output=True,
        )
        subprocess.run(
            ["git", "-C", str(main), "worktree", "add", "-b", "acta", str(acta)],
            check=True,
            capture_output=True,
        )
        client = FakeClient(
            projects=[
                {
                    "id": "project-fixture",
                    "name": "fixture",
                    "roots": [{"path": str(main)}],
                }
            ]
        )

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=acta,
            name="fx-acta-win",
            expected_thread_id=None,
        )

        self.assertEqual(client.threads[thread_id]["projectId"], "project-fixture")
        self.assertEqual(
            client.projects[0]["roots"],
            [{"path": str(main)}, {"path": str(acta.resolve())}],
        )
        self.assertIn("project/update", [method for method, _ in client.calls])

    def test_multiple_saved_projects_for_one_checkout_fail_closed(self) -> None:
        client = FakeClient(
            projects=[
                {"id": "project-one", "roots": [{"path": str(self.cwd)}]},
                {"id": "project-two", "roots": [{"path": str(self.cwd)}]},
            ]
        )

        with self.assertRaisesRegex(
            codex_station.CodexStationError,
            "multiple Codex projects",
        ):
            codex_station.provision_named_thread(
                client,
                cwd=self.cwd,
                name=self.name,
                expected_thread_id=None,
            )

    def test_reuses_the_only_materialized_exact_match_without_resuming(self) -> None:
        client = FakeClient(
            [
                {
                    "id": "thr-existing",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [{"id": "turn-existing", "status": "completed"}],
                }
            ]
        )

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id=None,
        )

        self.assertEqual(thread_id, "thr-existing")
        methods = [method for method, _ in client.calls]
        self.assertNotIn("thread/resume", methods)
        self.assertNotIn("thread/unsubscribe", methods)

    def test_reuses_desktop_loaded_materialized_task_without_writer_collision(
        self,
    ) -> None:
        client = FakeClient(
            [
                {
                    "id": "thr-existing",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [{"id": "turn-existing", "status": "completed"}],
                }
            ],
            stale_writer_thread_ids={"thr-existing"},
        )

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id="thr-existing",
        )

        self.assertEqual(thread_id, "thr-existing")
        methods = [method for method, _ in client.calls]
        self.assertNotIn("thread/resume", methods)
        self.assertNotIn("thread/unsubscribe", methods)

    def test_recovers_one_stale_writer_when_an_empty_task_needs_bootstrap(self) -> None:
        client = FakeClient(
            [
                {
                    "id": "thr-existing",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [],
                }
            ],
            stale_writer_thread_ids={"thr-existing"},
        )

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id="thr-existing",
        )

        self.assertEqual(thread_id, "thr-existing")
        methods = [method for method, _ in client.calls]
        self.assertEqual(methods.count("thread/resume"), 2)
        self.assertEqual(methods.count("thread/unsubscribe"), 2)

    def test_refuses_an_in_progress_turn_without_acquiring_a_writer(self) -> None:
        client = FakeClient(
            [
                {
                    "id": "thr-existing",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [{"id": "turn-existing", "status": "inProgress"}],
                }
            ],
            stale_writer_thread_ids={"thr-existing"},
        )

        with self.assertRaisesRegex(
            codex_station.CodexStationError, "has an active turn"
        ):
            codex_station.provision_named_thread(
                client,
                cwd=self.cwd,
                name=self.name,
                expected_thread_id="thr-existing",
            )

        methods = [method for method, _ in client.calls]
        self.assertNotIn("thread/resume", methods)
        self.assertNotIn("thread/unsubscribe", methods)

    def test_failed_bootstrap_turn_is_not_reported_ready(self) -> None:
        client = FakeClient(turn_status="failed")

        with self.assertRaisesRegex(codex_station.CodexStationError, "failed"):
            codex_station.provision_named_thread(
                client,
                cwd=self.cwd,
                name=self.name,
                expected_thread_id=None,
            )

        self.assertEqual(client.calls[-1][0], "thread/unsubscribe")

    def test_bootstrap_waits_for_completion_notification(self) -> None:
        client = FakeClient(turn_status="inProgress")

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id=None,
        )

        self.assertEqual(thread_id, "thr-new-0001")
        self.assertIn("wait_for_notification", [method for method, _ in client.calls])

    def test_registry_thread_with_another_name_is_not_hijacked(self) -> None:
        client = FakeClient(
            [{"id": "thr-existing", "cwd": str(self.cwd), "name": "fx-main-mac-old"}]
        )

        with self.assertRaisesRegex(
            codex_station.CodexStationError, "refusing to rename"
        ):
            codex_station.provision_named_thread(
                client,
                cwd=self.cwd,
                name=self.name,
                expected_thread_id="thr-existing",
            )

        self.assertEqual(client.threads["thr-existing"]["name"], "fx-main-mac-old")

    def test_empty_registered_duplicate_is_archived_and_real_task_adopted(self) -> None:
        client = FakeClient(
            [
                {
                    "id": "thr-empty",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [],
                },
                {
                    "id": "thr-real",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [{"id": "turn-existing", "status": "completed"}],
                },
            ]
        )

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id="thr-empty",
        )

        self.assertEqual(thread_id, "thr-real")
        self.assertTrue(client.threads["thr-empty"]["archived"])

    def test_empty_registered_task_omitted_from_search_is_recovered(self) -> None:
        client = FakeClient(
            [
                {
                    "id": "thr-empty",
                    "cwd": str(self.cwd),
                    "name": None,
                    "turns": [],
                },
                {
                    "id": "thr-real",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [{"id": "turn-existing", "status": "completed"}],
                },
            ]
        )

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id="thr-empty",
        )

        self.assertEqual(thread_id, "thr-real")
        self.assertTrue(client.threads["thr-empty"]["archived"])

    def test_bootstrap_only_registered_task_is_replaced_by_real_task(self) -> None:
        client = FakeClient(
            [
                {
                    "id": "thr-bootstrap",
                    "cwd": str(self.cwd),
                    "name": None,
                    "turns": [
                        station_bootstrap_turn(agent_text="bootstrap acknowledged")
                    ],
                },
                {
                    "id": "thr-real",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [{"id": "turn-real", "status": "completed"}],
                },
            ]
        )

        thread_id = codex_station.provision_named_thread(
            client,
            cwd=self.cwd,
            name=self.name,
            expected_thread_id="thr-bootstrap",
        )

        self.assertEqual(thread_id, "thr-real")
        self.assertTrue(client.threads["thr-bootstrap"]["archived"])

    def test_bootstrap_lookalike_with_tool_activity_is_not_replaced(self) -> None:
        client = FakeClient(
            [
                {
                    "id": "thr-tool-bearing",
                    "cwd": str(self.cwd),
                    "name": None,
                    "turns": [
                        station_bootstrap_turn(
                            extra_items=[
                                {
                                    "id": "item-command",
                                    "type": "commandExecution",
                                }
                            ]
                        )
                    ],
                },
                {
                    "id": "thr-real",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [{"id": "turn-real", "status": "completed"}],
                },
            ]
        )

        with self.assertRaisesRegex(codex_station.CodexStationError, "already used"):
            codex_station.provision_named_thread(
                client,
                cwd=self.cwd,
                name=self.name,
                expected_thread_id="thr-tool-bearing",
            )

        self.assertFalse(client.threads["thr-tool-bearing"].get("archived", False))

    def test_nonempty_registered_task_omitted_from_search_is_not_replaced(self) -> None:
        client = FakeClient(
            [
                {
                    "id": "thr-registered",
                    "cwd": str(self.cwd),
                    "name": None,
                    "turns": [{"id": "turn-registered", "status": "completed"}],
                },
                {
                    "id": "thr-real",
                    "cwd": str(self.cwd),
                    "name": self.name,
                    "turns": [{"id": "turn-real", "status": "completed"}],
                },
            ]
        )

        with self.assertRaisesRegex(codex_station.CodexStationError, "already used"):
            codex_station.provision_named_thread(
                client,
                cwd=self.cwd,
                name=self.name,
                expected_thread_id="thr-registered",
            )

        self.assertFalse(client.threads["thr-registered"].get("archived", False))

    def test_duplicate_exact_matches_fail_closed(self) -> None:
        client = FakeClient(
            [
                {"id": "thr-one", "cwd": str(self.cwd), "name": self.name},
                {"id": "thr-two", "cwd": str(self.cwd), "name": self.name},
            ]
        )

        with self.assertRaisesRegex(codex_station.CodexStationError, "multiple"):
            codex_station.provision_named_thread(
                client,
                cwd=self.cwd,
                name=self.name,
                expected_thread_id=None,
            )

    def test_stored_thread_in_a_different_checkout_is_refused(self) -> None:
        other = self.cwd / "other"
        other.mkdir()
        client = FakeClient(
            [{"id": "thr-existing", "cwd": str(other), "name": self.name}]
        )

        with self.assertRaisesRegex(codex_station.CodexStationError, "belongs to"):
            codex_station.provision_named_thread(
                client,
                cwd=self.cwd,
                name=self.name,
                expected_thread_id="thr-existing",
            )

    def test_conflicting_task_for_registry_name_is_refused(self) -> None:
        client = FakeClient(
            [{"id": "thr-other", "cwd": str(self.cwd), "name": self.name}]
        )

        with self.assertRaisesRegex(codex_station.CodexStationError, "already used"):
            codex_station.provision_named_thread(
                client,
                cwd=self.cwd,
                name=self.name,
                expected_thread_id="thr-expected",
            )


if __name__ == "__main__":
    unittest.main()
