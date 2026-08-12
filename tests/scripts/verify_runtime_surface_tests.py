#!/usr/bin/env python3
from __future__ import annotations

import ctypes
import os
import platform
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import scripts.verify_runtime_surface as runtime_surface  # noqa: E402
from scripts.verify_runtime_surface import (  # noqa: E402
    DARWIN_CHILD_PID_LIMIT,
    DARWIN_PID_MAX,
    child_processes,
    darwin_child_processes,
    load_darwin_child_pid_query,
    output_lines,
    windows_child_process_query,
    windows_network_query,
)


class RuntimeSurfaceContractTests(unittest.TestCase):
    @staticmethod
    def darwin_query(pids: list[int], result: int | None = None):
        def query(parent_pid: int, buffer: ctypes.Array, buffer_size: int) -> int:
            del parent_pid
            expected_size = (DARWIN_CHILD_PID_LIMIT + 1) * ctypes.sizeof(
                ctypes.c_int32
            )
            if buffer_size != expected_size:
                raise AssertionError(f"unexpected child buffer size: {buffer_size}")
            for index, pid in enumerate(pids):
                buffer[index] = pid
            return len(pids) if result is None else result

        return query

    def test_output_lines_removes_only_empty_lines_and_outer_whitespace(self) -> None:
        self.assertEqual(
            output_lines("\n  123 \r\n tcp:127.0.0.1:8080 \n"),
            ["123", "tcp:127.0.0.1:8080"],
        )

    def test_windows_child_query_is_scoped_to_the_parent_pid(self) -> None:
        query = windows_child_process_query(4815)

        self.assertIn('ParentProcessId = 4815', query)
        self.assertIn("Select-Object -ExpandProperty ProcessId", query)

    def test_windows_network_query_checks_tcp_listeners_and_udp(self) -> None:
        query = windows_network_query(90210)

        self.assertIn("Get-NetTCPConnection -OwningProcess 90210", query)
        self.assertIn("-State Listen", query)
        self.assertIn("Get-NetUDPEndpoint -OwningProcess 90210", query)
        self.assertIn('"tcp:$($_.LocalAddress):$($_.LocalPort)"', query)
        self.assertIn('"udp:$($_.LocalAddress):$($_.LocalPort)"', query)

    def test_darwin_loader_applies_the_exact_proc_listchildpids_signature(self) -> None:
        class Procedure:
            argtypes = None
            restype = None

            def __call__(self, parent_pid, buffer, buffer_size):
                del parent_pid, buffer, buffer_size
                return 0

        class Library:
            proc_listchildpids = Procedure()

        loads: list[tuple[str, bool]] = []

        def loader(name: str, *, use_errno: bool):
            loads.append((name, use_errno))
            return Library()

        query = load_darwin_child_pid_query(loader)

        self.assertEqual(loads, [("libproc.dylib", True)])
        self.assertEqual(
            Library.proc_listchildpids.argtypes,
            [ctypes.c_int32, ctypes.c_void_p, ctypes.c_int],
        )
        self.assertIs(Library.proc_listchildpids.restype, ctypes.c_int)
        self.assertEqual(query(42, (ctypes.c_int32 * 1)(), 4), 0)

    def test_darwin_loader_fails_closed_when_libproc_is_unavailable(self) -> None:
        def loader(name: str, *, use_errno: bool):
            del name, use_errno
            raise OSError("missing")

        with self.assertRaisesRegex(RuntimeError, "libproc is unavailable"):
            load_darwin_child_pid_query(loader)

    def test_darwin_loader_fails_closed_when_api_is_unavailable(self) -> None:
        def loader(name: str, *, use_errno: bool):
            del name, use_errno
            return object()

        with self.assertRaisesRegex(RuntimeError, "no proc_listchildpids API"):
            load_darwin_child_pid_query(loader)

    def test_darwin_child_query_accepts_an_empty_result(self) -> None:
        self.assertEqual(darwin_child_processes(4815, self.darwin_query([])), [])

    def test_darwin_child_query_normalizes_duplicate_race_results(self) -> None:
        self.assertEqual(
            darwin_child_processes(
                4815,
                self.darwin_query([90212, 90210, 90212]),
            ),
            [90210, 90212],
        )

    def test_darwin_child_query_rejects_invalid_parent_and_child_pids(self) -> None:
        for parent_pid in (0, -1, DARWIN_PID_MAX + 1, True):
            with self.subTest(parent_pid=parent_pid):
                with self.assertRaisesRegex(RuntimeError, "invalid macOS parent PID"):
                    darwin_child_processes(parent_pid, self.darwin_query([]))

        for child_pid in (0, -1, DARWIN_PID_MAX + 1, 4815):
            with self.subTest(child_pid=child_pid):
                with self.assertRaisesRegex(RuntimeError, "invalid child PID"):
                    darwin_child_processes(
                        4815,
                        self.darwin_query([child_pid]),
                    )

    def test_darwin_child_query_fails_closed_on_native_error(self) -> None:
        def failing_query(parent_pid, buffer, buffer_size):
            del parent_pid, buffer, buffer_size
            ctypes.set_errno(13)
            return -1

        with self.assertRaisesRegex(RuntimeError, "errno 13"):
            darwin_child_processes(4815, failing_query)

    def test_darwin_child_query_rejects_capacity_truncation(self) -> None:
        overflowing_pids = list(range(1, DARWIN_CHILD_PID_LIMIT + 2))
        with self.assertRaisesRegex(RuntimeError, "exceeded.*capacity"):
            darwin_child_processes(
                DARWIN_PID_MAX,
                self.darwin_query(overflowing_pids),
            )

    def test_darwin_dispatch_never_launches_ps_or_a_shell(self) -> None:
        with (
            mock.patch.object(runtime_surface.platform, "system", return_value="Darwin"),
            mock.patch.object(
                runtime_surface,
                "load_darwin_child_pid_query",
                return_value=self.darwin_query([90210]),
            ),
            mock.patch.object(
                runtime_surface.subprocess,
                "run",
                side_effect=AssertionError("Darwin must not launch ps"),
            ),
        ):
            self.assertEqual(child_processes(4815), [90210])

    @unittest.skipUnless(platform.system() == "Darwin", "requires macOS libproc")
    def test_darwin_native_query_detects_a_real_direct_child(self) -> None:
        child = subprocess.Popen(
            [
                sys.executable,
                "-I",
                "-S",
                "-c",
                "import time; time.sleep(10)",
            ]
        )
        try:
            self.assertIn(child.pid, darwin_child_processes(os.getpid()))
        finally:
            child.terminate()
            child.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
