#!/usr/bin/env python3
from __future__ import annotations

import ctypes
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
from collections.abc import Callable
from pathlib import Path


CANARY = "BPR_CANARY_SECRET_7B1E6D9A"
SOCKET_PATTERN = re.compile(r"socket:\[(\d+)\]")
DARWIN_PID_MAX = (1 << 31) - 1
DARWIN_CHILD_PID_LIMIT = 1024
PRIVATE_DATA_ROOT_SWITCH = "--private-data-root"
NATIVE_PLATFORM_SWITCH = "--native-platform"
USAGE = f"usage: verify_runtime_surface.py [{PRIVATE_DATA_ROOT_SWITCH}] [{NATIVE_PLATFORM_SWITCH}] <executable>"

DarwinChildPidQuery = Callable[[int, ctypes.Array, int], int]


def output_lines(output: str) -> list[str]:
    return [line.strip() for line in output.splitlines() if line.strip()]


def powershell_output(command: str) -> str:
    executable = shutil.which("pwsh") or shutil.which("powershell")
    if executable is None:
        raise RuntimeError("PowerShell is required for the Windows runtime audit")

    completed = subprocess.run(
        [executable, "-NoProfile", "-NonInteractive", "-Command", command],
        check=True,
        capture_output=True,
        text=True,
        timeout=5,
    )
    return completed.stdout


def unix_child_processes(parent_pid: int) -> list[int]:
    completed = subprocess.run(
        ["ps", "-eo", "pid=,ppid="],
        check=True,
        capture_output=True,
        text=True,
        timeout=5,
    )
    children: list[int] = []
    for line in completed.stdout.splitlines():
        fields = line.split()
        if len(fields) == 2 and int(fields[1]) == parent_pid:
            children.append(int(fields[0]))
    return children


def load_darwin_child_pid_query(
    library_loader: Callable[..., object] | None = None,
) -> DarwinChildPidQuery:
    loader = ctypes.CDLL if library_loader is None else library_loader
    try:
        library = loader("libproc.dylib", use_errno=True)
    except OSError as error:
        raise RuntimeError("macOS libproc is unavailable") from error

    try:
        query = library.proc_listchildpids  # type: ignore[attr-defined]
    except AttributeError as error:
        raise RuntimeError("macOS libproc has no proc_listchildpids API") from error

    query.argtypes = [ctypes.c_int32, ctypes.c_void_p, ctypes.c_int]
    query.restype = ctypes.c_int

    def invoke(parent_pid: int, buffer: ctypes.Array, buffer_size: int) -> int:
        return query(parent_pid, buffer, buffer_size)

    return invoke


def darwin_child_processes(
    parent_pid: int,
    query: DarwinChildPidQuery | None = None,
) -> list[int]:
    if type(parent_pid) is not int or not 1 <= parent_pid <= DARWIN_PID_MAX:
        raise RuntimeError(f"invalid macOS parent PID: {parent_pid!r}")

    child_query = load_darwin_child_pid_query() if query is None else query
    query_capacity = DARWIN_CHILD_PID_LIMIT + 1
    child_buffer = (ctypes.c_int32 * query_capacity)()
    ctypes.set_errno(0)
    try:
        child_count = child_query(
            parent_pid,
            child_buffer,
            ctypes.sizeof(child_buffer),
        )
    except Exception as error:
        raise RuntimeError("proc_listchildpids invocation failed") from error

    if type(child_count) is not int:
        raise RuntimeError("proc_listchildpids returned a non-integer count")
    if child_count < 0:
        raise RuntimeError(
            f"proc_listchildpids failed with errno {ctypes.get_errno()}"
        )
    if child_count > DARWIN_CHILD_PID_LIMIT:
        raise RuntimeError(
            "proc_listchildpids exceeded the direct-child capacity "
            f"of {DARWIN_CHILD_PID_LIMIT}"
        )

    children: set[int] = set()
    for child_pid in child_buffer[:child_count]:
        if not 1 <= child_pid <= DARWIN_PID_MAX or child_pid == parent_pid:
            raise RuntimeError(
                f"proc_listchildpids returned an invalid child PID: {child_pid}"
            )
        children.add(child_pid)
    return sorted(children)


def windows_child_process_query(parent_pid: int) -> str:
    return (
        "Get-CimInstance Win32_Process "
        f'-Filter "ParentProcessId = {parent_pid}" '
        "| Select-Object -ExpandProperty ProcessId"
    )


def windows_child_processes(parent_pid: int) -> list[int]:
    return [
        int(line)
        for line in output_lines(
            powershell_output(windows_child_process_query(parent_pid))
        )
    ]


def child_processes(parent_pid: int) -> list[int]:
    system = platform.system()
    if system == "Darwin":
        return darwin_child_processes(parent_pid)
    if system == "Windows":
        return windows_child_processes(parent_pid)
    return unix_child_processes(parent_pid)


def linux_network_sockets(pid: int) -> list[str]:
    socket_inodes: set[str] = set()
    for descriptor in Path(f"/proc/{pid}/fd").iterdir():
        try:
            target = os.readlink(descriptor)
        except FileNotFoundError:
            continue
        match = SOCKET_PATTERN.fullmatch(target)
        if match:
            socket_inodes.add(match.group(1))

    matches: list[str] = []
    for table in ("tcp", "tcp6", "udp", "udp6"):
        table_path = Path(f"/proc/{pid}/net/{table}")
        if not table_path.exists():
            continue
        for line in table_path.read_text(encoding="utf-8").splitlines()[1:]:
            fields = line.split()
            if len(fields) > 9 and fields[9] in socket_inodes:
                matches.append(f"{table}:{fields[1]}->{fields[2]}:{fields[3]}")
    return matches


def macos_network_sockets(pid: int) -> list[str]:
    completed = subprocess.run(
        ["lsof", "-Pan", "-a", "-p", str(pid), "-i"],
        check=False,
        capture_output=True,
        text=True,
        timeout=5,
    )
    if completed.returncode not in (0, 1):
        raise RuntimeError(f"lsof failed with status {completed.returncode}")
    return completed.stdout.splitlines()[1:]


def windows_network_query(pid: int) -> str:
    return (
        f"$tcp = @(Get-NetTCPConnection -OwningProcess {pid} "
        '-ErrorAction SilentlyContinue | ForEach-Object { '
        '"tcp:$($_.LocalAddress):$($_.LocalPort)" }); '
        f"$udp = @(Get-NetUDPEndpoint -OwningProcess {pid} "
        '-ErrorAction SilentlyContinue | ForEach-Object { '
        '"udp:$($_.LocalAddress):$($_.LocalPort)" }); '
        "$tcp + $udp"
    )


def windows_network_sockets(pid: int) -> list[str]:
    return output_lines(powershell_output(windows_network_query(pid)))


def network_sockets(pid: int) -> list[str]:
    system = platform.system()
    if system == "Darwin":
        return macos_network_sockets(pid)
    if system == "Linux":
        return linux_network_sockets(pid)
    if system == "Windows":
        return windows_network_sockets(pid)
    raise RuntimeError(f"unsupported runtime-audit platform: {system}")


def loaded_libraries(pid: int) -> list[Path]:
    system = platform.system()
    if system == "Darwin":
        result = subprocess.run(["lsof", "-a", "-p", str(pid), "-Fn"], check=True,
                                capture_output=True, text=True, timeout=5)
        names = [line[1:] for line in result.stdout.splitlines() if line.startswith("n/")]
        return sorted({Path(name) for name in names if ".framework/" in name or name.endswith(".dylib")})
    if system == "Linux":
        names = [line.split(maxsplit=5)[-1] for line in Path(f"/proc/{pid}/maps").read_text().splitlines()]
        return sorted({Path(name) for name in names if name.startswith("/") and ".so" in name})
    if system == "Windows":
        return [Path(name) for name in output_lines(powershell_output(
            f"(Get-Process -Id {pid} -ErrorAction Stop).Modules | Select-Object -ExpandProperty FileName")) if name.lower().endswith(".dll")]
    raise RuntimeError("unsupported loader inspection platform")


def check_library_origins(paths: list[Path], package_root: Path) -> None:
    package_root = package_root.resolve()
    system = platform.system()
    roots = {"Darwin": [Path("/usr/lib"), Path("/System/Library")],
             "Linux": [Path("/usr/lib"), Path("/lib")],
             "Windows": [Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32"]}[system]
    require_qt = False
    for path in paths:
        resolved = path.resolve()
        packaged = resolved.is_relative_to(package_root)
        if not packaged and not any(resolved.is_relative_to(root.resolve()) for root in roots):
            raise RuntimeError("loaded library outside package and declared system roots")
        require_qt |= packaged and ("QtCore.framework" in str(path) or "Qt6Core" in path.name)
    if not require_qt:
        raise RuntimeError("no packaged QtCore observed in the running process")


def parse_arguments(arguments: list[str]) -> tuple[Path, bool, bool] | None:
    """Flags precede one executable; native selection adds no product test seam."""
    if not arguments or arguments[-1].startswith("--"):
        return None
    switches = arguments[:-1]
    if len(set(switches)) != len(switches) or any(
        value not in (PRIVATE_DATA_ROOT_SWITCH, NATIVE_PLATFORM_SWITCH)
        for value in switches
    ):
        return None
    return (Path(arguments[-1]), PRIVATE_DATA_ROOT_SWITCH in switches,
            NATIVE_PLATFORM_SWITCH in switches)


def launch_environment(native_platform: bool, environment: dict[str, str] | None = None) -> dict[str, str]:
    result = dict(os.environ if environment is None else environment)
    if native_platform:
        selected = {"Darwin": "cocoa", "Windows": "windows", "Linux": "xcb"}
        try:
            result["QT_QPA_PLATFORM"] = selected[platform.system()]
        except KeyError as error:
            raise RuntimeError("unsupported native Qt platform") from error
    else:
        result["QT_QPA_PLATFORM"] = "offscreen"
    result["BPR_TEST_CANARY"] = CANARY
    return result


def application_command(executable: Path, data_root: Path | None) -> list[str]:
    # BPR-0820. The inspected launch is idle and interactive-shaped, so nothing
    # else gives it a root of its own. `--test-application-data-root` exists
    # only in a tests-enabled build; a tests-disabled artifact is inspected
    # without the switch, exactly as before it existed.
    if data_root is None:
        return [str(executable)]
    return [str(executable), "--test-application-data-root", str(data_root)]


def inspect_application(command: list[str], *, native_platform: bool = False,
                        environment: dict[str, str] | None = None,
                        package_root: Path | None = None) -> int:
    process = subprocess.Popen(
        command,
        env=launch_environment(native_platform, environment),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        time.sleep(2 if native_platform else 0.5)
        if process.poll() is not None:
            print("application exited before runtime inspection", file=sys.stderr)
            return 1

        sockets = network_sockets(process.pid)
        children = child_processes(process.pid)
        if sockets:
            print("unexpected application network sockets:", file=sys.stderr)
            print("\n".join(sockets), file=sys.stderr)
            return 1
        if children:
            print(
                "unexpected application child processes: "
                + ", ".join(str(child) for child in children),
                file=sys.stderr,
            )
            return 1
        if package_root is not None:
            libraries = loaded_libraries(process.pid)
            check_library_origins(libraries, package_root)
            print(f"RUNTIME_LIBRARIES_OK:{len(libraries)}")
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            stdout, stderr = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            stdout, stderr = process.communicate(timeout=5)

    combined_output = stdout + stderr
    if CANARY in combined_output:
        print("canary secret appeared in application output", file=sys.stderr)
        return 1
    if any(marker in combined_output for marker in ("STARTUP_FAILED", "INSTALLED_RUNTIME_REFUSED", "PROFILE_ALREADY_OPEN")):
        print("application did not own a usable runtime", file=sys.stderr)
        return 1
    return 0


def inspect_with_private_data_root(executable: Path, *, native_platform: bool = False) -> int:
    # `mkdtemp` creates the parent owner-only. It is resolved because the
    # application refuses a root whose existing ancestor is not spelled
    # canonically, and macOS reaches the temporary directory through links.
    # The `data` leaf is left for the application to create under its own
    # storage-root rule, as it creates a default root.
    parent = Path(tempfile.mkdtemp(prefix="bpr-runtime-surface-")).resolve(strict=True)
    data_root = parent / "data"
    try:
        result = inspect_application(application_command(executable, data_root), native_platform=native_platform)
        if result == 0 and not data_root.is_dir():
            print(f"private data root was not composed: {data_root}", file=sys.stderr)
            result = 1
    finally:
        shutil.rmtree(parent, ignore_errors=True)
    if parent.exists():
        print(f"private data root was not removed: {parent}", file=sys.stderr)
        return 1
    return result


def main(arguments: list[str] | None = None) -> int:
    parsed = parse_arguments(sys.argv[1:] if arguments is None else arguments)
    if parsed is None:
        print(USAGE, file=sys.stderr)
        return 2

    executable, private_data_root, native_platform = parsed
    executable = executable.resolve()
    if not executable.is_file():
        print(f"missing executable: {executable}", file=sys.stderr)
        return 2

    result = (
        inspect_with_private_data_root(executable, native_platform=native_platform)
        if private_data_root
        else inspect_application(application_command(executable, None), native_platform=native_platform)
    )
    if result != 0:
        return result
    print("RUNTIME_SURFACE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
