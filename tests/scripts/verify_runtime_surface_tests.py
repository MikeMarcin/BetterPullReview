#!/usr/bin/env python3
from __future__ import annotations

import contextlib
import ctypes
import io
import os
import platform
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import scripts.verify_runtime_surface as runtime_surface  # noqa: E402
from scripts.contained_temporary_support import make_contained_temporary_directory  # noqa: E402
from skip_report import SkipReportingTestRunner  # noqa: E402
from scripts.verify_runtime_surface import (  # noqa: E402
    DARWIN_CHILD_PID_LIMIT,
    DARWIN_PID_MAX,
    PRIVATE_DATA_ROOT_SWITCH,
    NATIVE_PLATFORM_SWITCH,
    application_command,
    child_processes,
    darwin_child_processes,
    load_darwin_child_pid_query,
    inspect_with_private_data_root,
    output_lines,
    parse_arguments,
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

    def test_windows_network_query_checks_all_owned_tcp_connections_and_udp(self) -> None:
        query = windows_network_query(90210)

        self.assertIn("Get-NetTCPConnection -OwningProcess 90210", query)
        self.assertNotIn("-State Listen", query, "an outbound connection is also part of the runtime surface")
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


    def test_arguments_accept_the_executable_alone_or_after_the_private_root_switch(self) -> None:
        self.assertEqual(parse_arguments(["app"]), (Path("app"), False, False))
        self.assertEqual(
            parse_arguments([PRIVATE_DATA_ROOT_SWITCH, "app"]), (Path("app"), True, False)
        )
        self.assertEqual(parse_arguments([NATIVE_PLATFORM_SWITCH, "app"]), (Path("app"), False, True))
        self.assertEqual(parse_arguments([PRIVATE_DATA_ROOT_SWITCH, NATIVE_PLATFORM_SWITCH, "app"]), (Path("app"), True, True))
        for arguments in (
            [],
            [PRIVATE_DATA_ROOT_SWITCH],
            ["app", PRIVATE_DATA_ROOT_SWITCH],
            [PRIVATE_DATA_ROOT_SWITCH, PRIVATE_DATA_ROOT_SWITCH, "app"],
            ["app", "other"],
        ):
            with self.subTest(arguments=arguments):
                self.assertIsNone(parse_arguments(arguments))

    def test_application_command_names_a_root_only_when_one_is_owned(self) -> None:
        executable = Path("/bin/app")
        data_root = Path("/private/root/data")
        self.assertEqual(application_command(executable, None), [str(executable)])
        # [[BPR-0844]]. The command carries the path's own spelling, which is
        # backslashed on Windows, so it is compared as a path, not a POSIX literal.
        self.assertEqual(
            application_command(executable, data_root),
            [str(executable), "--test-application-data-root", str(data_root)],
        )

    def private_root_run(self, outcome):
        seen: dict[str, object] = {}

        def inspect(command: list[str], *, native_platform: bool = False) -> int:
            self.assertFalse(native_platform)
            self.assertEqual(command[1], "--test-application-data-root")
            data_root = Path(command[2])
            parent = data_root.parent
            seen["parent"] = parent
            seen["leaf_existed"] = data_root.exists()
            seen["resolved"] = parent == parent.resolve(strict=True)
            if os.name == "posix":
                seen["mode"] = parent.stat().st_mode & 0o777
            return outcome(data_root)

        with contextlib.ExitStack() as patches:
            patches.enter_context(mock.patch.object(runtime_surface, "inspect_application", inspect))
            if os.name == "nt":
                # [[BPR-0844]]. Inside the Windows AppContainer this row runs in,
                # a `mkdtemp` directory is unusable (BPR-0350) and CPython's
                # `realpath` cannot resolve a DOS path. Neither belongs to the
                # script under test, so the case substitutes a contained
                # temporary root and records the resolution the script asks for
                # rather than performing it. POSIX keeps the real calls.
                patches.enter_context(mock.patch.object(
                    runtime_surface.tempfile, "mkdtemp",
                    lambda prefix="": str(make_contained_temporary_directory(prefix=prefix))))
                resolved: list[tuple[Path, bool]] = []

                def resolve(path: Path, strict: bool = False) -> Path:
                    resolved.append((Path(path), strict))
                    return Path(os.path.abspath(path))

                patches.enter_context(mock.patch.object(Path, "resolve", resolve))
                seen["resolutions"] = resolved
            try:
                result = inspect_with_private_data_root(Path("/bin/app"))
            except RuntimeError:
                result = None
        if os.name == "nt":
            # The script still asked for a strict resolution of the parent it made.
            self.assertIn((Path(seen["parent"]), True), seen["resolutions"])
        return result, seen

    def test_private_root_is_owner_private_uncreated_and_removed_after_a_pass(self) -> None:
        def composed(data_root: Path) -> int:
            data_root.mkdir()
            return 0

        result, seen = self.private_root_run(composed)
        self.assertEqual(result, 0)
        self.assertFalse(seen["leaf_existed"])
        self.assertTrue(seen["resolved"])
        if os.name == "posix":
            self.assertEqual(seen["mode"], 0o700)
        self.assertFalse(Path(seen["parent"]).exists())

    def test_private_root_that_was_never_composed_fails_and_is_removed(self) -> None:
        result, seen = self.private_root_run(lambda data_root: 0)
        self.assertEqual(result, 1)
        self.assertFalse(Path(seen["parent"]).exists())

    def test_private_root_is_removed_when_inspection_raises(self) -> None:
        def raises(data_root: Path) -> int:
            data_root.mkdir()
            raise RuntimeError("inspection failed")

        result, seen = self.private_root_run(raises)
        self.assertIsNone(result)
        self.assertFalse(Path(seen["parent"]).exists())

    def test_native_environment_selects_only_the_declared_shipped_platform(self) -> None:
        original = {"QT_QPA_PLATFORM": "offscreen", "QT_PLUGIN_PATH": "/hostile"}
        for system, expected in (("Darwin", "cocoa"), ("Windows", "windows"), ("Linux", "xcb")):
            with mock.patch.object(runtime_surface.platform, "system", return_value=system):
                result = runtime_surface.launch_environment(True, original)
                self.assertEqual(result["QT_QPA_PLATFORM"], expected)
                self.assertEqual(result["QT_PLUGIN_PATH"], "/hostile", "audit must not hide a hostile origin from the product")
        self.assertEqual(original["QT_QPA_PLATFORM"], "offscreen")
        self.assertEqual(runtime_surface.launch_environment(False, {})["QT_QPA_PLATFORM"], "offscreen")

    def test_inspection_rejects_early_exit_canary_socket_child_and_startup_failure(self) -> None:
        for case in ("early", "canary", "socket", "child", "startup", "healthy"):
            with self.subTest(case=case):
                process = mock.Mock(pid=4815)
                process.poll.return_value = 0 if case == "early" else None
                output = runtime_surface.CANARY if case in ("early", "canary") else "STARTUP_FAILED" if case == "startup" else ""
                process.communicate.return_value = (output, "")
                errors = io.StringIO()
                with (mock.patch.object(runtime_surface.subprocess, "Popen", return_value=process) as launch,
                      mock.patch.object(runtime_surface.time, "sleep"),
                      mock.patch.object(runtime_surface, "network_sockets", return_value=["tcp:socket"] if case == "socket" else []),
                      mock.patch.object(runtime_surface, "child_processes", return_value=[72] if case == "child" else []),
                      contextlib.redirect_stderr(errors)):
                    result = runtime_surface.inspect_application(["app"], native_platform=True)
                self.assertEqual(result, 0 if case == "healthy" else 1)
                self.assertEqual(launch.call_args.args[0], ["app"], "native audit adds no in-product fixture flag")
                self.assertNotIn(runtime_surface.CANARY, errors.getvalue())
                process.communicate.assert_called_once()
                if case != "early":
                    process.terminate.assert_called_once()

    def test_loader_queries_are_process_scoped_and_require_a_packaged_qt_positive(self) -> None:
        with (mock.patch.object(runtime_surface.platform, "system", return_value="Darwin"),
              mock.patch.object(Path, "resolve", lambda path: path)):
            root = Path("/candidate/App.app")
            qt = root / "Contents/Frameworks/QtCore.framework/Versions/A/QtCore"
            runtime_surface.check_library_origins([qt, Path("/usr/lib/libSystem.B.dylib")], root)
            for paths in ([], [Path("/usr/lib/libSystem.B.dylib")], [qt, Path("/untrusted/libUseful.dylib")]):
                with self.assertRaises(RuntimeError):
                    runtime_surface.check_library_origins(paths, root)
            with mock.patch.object(runtime_surface.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, stdout=f"p4815\nn{qt.as_posix()}\nn/a/profile/database\n")) as query:
                self.assertEqual(runtime_surface.loaded_libraries(4815), [qt])
                self.assertEqual(query.call_args.args[0], ["lsof", "-a", "-p", "4815", "-Fn"])
        with mock.patch.object(runtime_surface.platform, "system", return_value="Windows"), mock.patch.object(runtime_surface, "powershell_output", return_value="C:/Windows/System32/kernel32.dll\n") as query:
            self.assertEqual(len(runtime_surface.loaded_libraries(90210)), 1)
            self.assertIn("Get-Process -Id 90210 -ErrorAction Stop", query.call_args.args[0])


if __name__ == "__main__":
    unittest.main(testRunner=SkipReportingTestRunner)
