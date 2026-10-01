#!/usr/bin/env python3
"""Falsifiable process-boundary probes for the automated-test network lane."""

from __future__ import annotations

from pathlib import Path
import errno
import os
import pathlib
import shutil
import signal
import socket
import stat
import subprocess
import sys
import threading


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

MACOS_BACKEND = "macos-sandbox-exec"
LINUX_BACKEND = "linux-network-namespace"
WINDOWS_BACKEND = "windows-appcontainer"

# The Winsock names exist in `errno` only on Windows, while the numbers are
# fixed by Winsock itself, so the Windows shape is spelled with its own name
# where that name is defined and by its fixed value everywhere else. This keeps
# the module importable on macOS and Linux without widening anyone's set.
WSAEACCES = getattr(errno, "WSAEACCES", 10013)

# Each backend refuses an external connection in its own shape, and accepting
# the other one's shape would let a genuine escape pass. sandbox-exec denies
# the operation, so the kernel reports a permission error. A Linux network
# namespace holding only loopback has no route to a global address at all, so
# the kernel reports the network as unreachable before any policy is consulted.
# A Windows AppContainer entered with zero capabilities still has the host's
# routes, so the connection is refused rather than unroutable: the Windows
# Filtering Platform denies it and Winsock reports WSAEACCES.
DENIAL_ERRNOS = {
    MACOS_BACKEND: (errno.EPERM, errno.EACCES),
    LINUX_BACKEND: (errno.ENETUNREACH, errno.EHOSTUNREACH),
    WINDOWS_BACKEND: (WSAEACCES,),
}


def linux_namespace_is_isolated() -> tuple[bool, str]:
    """Prove the namespace caused the unreachability, not a missing network.

    An unreachable errno is only evidence when the boundary produced it. A host
    that is simply offline reports the same errno, so the errno alone cannot
    tell a working boundary from an absent one. The launcher's namespace holds
    exactly one interface and no route; an unsandboxed host keeps its real
    interfaces and routes whether or not they currently carry traffic.
    """
    try:
        interfaces = sorted(
            line.split(":", 1)[0].strip()
            for line in pathlib.Path("/proc/net/dev").read_text().splitlines()[2:]
            if ":" in line
        )
        routes = sorted(
            {
                line.split("\t", 1)[0]
                for line in pathlib.Path("/proc/net/route").read_text().splitlines()[1:]
                if line.strip()
            }
        )
    except OSError as error:
        return False, f"unreadable:{error.errno}"
    if interfaces != ["lo"]:
        return False, "interfaces=" + ",".join(interfaces)
    if routes:
        return False, "routes=" + ",".join(routes)
    return True, "lo-only"


def windows_appcontainer_is_entered() -> tuple[bool, str]:
    """Prove the AppContainer caused the refusal, not a missing network.

    This is the Windows half of the same argument the Linux helper above
    makes. WSAEACCES alone is weak evidence, because a machine whose firewall
    or policy refuses the address reports it too, so the errno cannot tell a
    working boundary from an unrelated refusal. The token can: a process
    entered through the launcher carries an AppContainer token and one that
    was never contained does not, which Windows answers directly.

    `TokenIsAppContainer` is only reachable through the Win32 API, so ctypes
    is required; nothing outside the standard library is. `GetCurrentProcess`
    returns a pseudo-handle, and without an explicit `restype` ctypes marshals
    it as a 32-bit int and silently corrupts it on 64-bit Python, which fails
    as an invalid-handle error rather than as a wrong answer.
    """
    try:
        import ctypes
        from ctypes import wintypes
    except (ImportError, ValueError) as error:  # pragma: no cover - not Windows
        return False, f"unavailable:{error}"

    token_query = 0x0008
    token_is_app_container = 29
    try:
        advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    except (AttributeError, OSError) as error:  # pragma: no cover - not Windows
        return False, f"unavailable:{error}"

    kernel32.GetCurrentProcess.restype = wintypes.HANDLE
    kernel32.GetCurrentProcess.argtypes = []
    advapi32.OpenProcessToken.restype = wintypes.BOOL
    advapi32.OpenProcessToken.argtypes = [
        wintypes.HANDLE,
        wintypes.DWORD,
        ctypes.POINTER(wintypes.HANDLE),
    ]
    advapi32.GetTokenInformation.restype = wintypes.BOOL
    advapi32.GetTokenInformation.argtypes = [
        wintypes.HANDLE,
        ctypes.c_int,
        ctypes.c_void_p,
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
    ]

    token = wintypes.HANDLE()
    if not advapi32.OpenProcessToken(
        kernel32.GetCurrentProcess(), token_query, ctypes.byref(token)
    ):
        return False, f"token-open:{ctypes.get_last_error()}"
    contained = wintypes.DWORD(0)
    written = wintypes.DWORD(0)
    queried = advapi32.GetTokenInformation(
        token,
        token_is_app_container,
        ctypes.byref(contained),
        ctypes.sizeof(contained),
        ctypes.byref(written),
    )
    error = ctypes.get_last_error()
    kernel32.CloseHandle(token)
    if not queried:
        return False, f"token-query:{error}"
    if contained.value != 1:
        return False, f"uncontained:{contained.value}"
    return True, "appcontainer"


def identity_attempt(backend: str) -> int:
    """Prove the launcher granted no privilege it was not asked for.

    A user namespace is needed only so an unprivileged caller can create a
    network namespace. Mapping the caller to 0 would also hand every test root,
    and root is exempt from the permission checks the product relies on, so a
    test asking whether a mode is refused would quietly get the wrong answer.

    The map itself is the proof and it is readable from inside: each range
    names the id inside the namespace and the id outside it. Equal ends mean
    the caller kept exactly the identity it already had. This holds whoever
    runs the suite, including a genuine root, so it does not double as an
    assertion about who that is.
    """
    if backend != LINUX_BACKEND:
        print(f"NETWORK_ISOLATION_IDENTITY_NOT_APPLICABLE:{backend}", flush=True)
        return 1
    for name in ("uid_map", "gid_map"):
        try:
            text = pathlib.Path(f"/proc/self/{name}").read_text()
        except OSError as error:
            print(f"NETWORK_ISOLATION_IDENTITY_UNREADABLE:{name}:{error.errno}", flush=True)
            return 1
        rows = [line.split() for line in text.splitlines() if line.strip()]
        if not rows:
            print(f"NETWORK_ISOLATION_IDENTITY_UNMAPPED:{name}", flush=True)
            return 1
        for row in rows:
            if len(row) != 3 or row[0] != row[1]:
                print(
                    f"NETWORK_ISOLATION_IDENTITY_REMAPPED:{name}:{' '.join(row)}",
                    flush=True,
                )
                return 1
    print(f"NETWORK_ISOLATION_IDENTITY_PRESERVED:{os.geteuid()}", flush=True)
    return 0


RUNTIME_PARENT = pathlib.Path("/tmp")
RUNTIME_PREFIX = "bpr-run-"


def row_runtime_directory(value: str) -> bool:
    """A directory the Linux launcher made for one row: `/tmp/bpr-run-*`, 0700."""
    path = pathlib.Path(value)
    if not value or not path.is_absolute():
        return False
    if path.parent != RUNTIME_PARENT or not path.name.startswith(RUNTIME_PREFIX):
        return False
    try:
        info = os.lstat(path)
    except OSError:
        return False
    return (
        stat.S_ISDIR(info.st_mode)
        and info.st_uid == os.geteuid()
        and stat.S_IMODE(info.st_mode) == 0o700
    )


def listening_unix_socket(path: pathlib.Path) -> socket.socket:
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    listener.bind(str(path))
    listener.listen(1)
    return listener


# [[BPR-1113]]. What the runtime child's links lead to, in the stand-in for the
# user's runtime directory: a file, and a directory holding one. A removal
# that followed a link out of the row's directory would reach one of them.
ESCAPE_ENVIRONMENT = "BPR_TEST_RUNTIME_ESCAPE"
ESCAPE_FILE = "escape-file"
ESCAPE_DIRECTORY = "escape-directory"
ESCAPE_KEPT = "kept"
ESCAPE_TEXT = "outside the row's runtime directory\n"


def plant_escape_targets(user: pathlib.Path) -> None:
    (user / ESCAPE_FILE).write_text(ESCAPE_TEXT, encoding="utf-8")
    (user / ESCAPE_DIRECTORY).mkdir(mode=0o700)
    (user / ESCAPE_DIRECTORY / ESCAPE_KEPT).write_text(ESCAPE_TEXT, encoding="utf-8")


def escape_target_reached(user: pathlib.Path) -> str | None:
    """Which escape target is no longer as it was planted, if any."""
    file = user / ESCAPE_FILE
    directory = user / ESCAPE_DIRECTORY
    kept = directory / ESCAPE_KEPT
    try:
        if file.is_symlink() or file.read_text(encoding="utf-8") != ESCAPE_TEXT:
            return ESCAPE_FILE
        if directory.is_symlink() or sorted(os.listdir(directory)) != [ESCAPE_KEPT]:
            return ESCAPE_DIRECTORY
        if kept.read_text(encoding="utf-8") != ESCAPE_TEXT:
            return f"{ESCAPE_DIRECTORY}/{ESCAPE_KEPT}"
    except OSError as error:
        return f"{error.filename}:{error.errno}"
    return None


def runtime_child() -> int:
    """What an owner a row ends with a signal leaves in its runtime directory.

    It binds a socket there and never unlinks it, adds a directory of its own,
    and then dies by SIGKILL, so no cleanup of its own can run. It also plants
    two links that lead out of the directory ([[BPR-1113]]), one to a file and
    one to a directory, as a hostile or careless row could.
    """
    runtime = os.environ.get("XDG_RUNTIME_DIR", "")
    print(f"RUNTIME_CHILD_DIRECTORY:{runtime}", flush=True)
    print(
        f"RUNTIME_CHILD_BUS:{os.environ.get('DBUS_SESSION_BUS_ADDRESS', '-')}",
        flush=True,
    )
    listening_unix_socket(pathlib.Path(runtime) / "bpr-activate-leftover.sock")
    debris = pathlib.Path(runtime) / "debris"
    debris.mkdir(mode=0o700)
    (debris / "leftover").write_text("leftover\n", encoding="utf-8")
    escape = pathlib.Path(os.environ[ESCAPE_ENVIRONMENT])
    (pathlib.Path(runtime) / ESCAPE_FILE).symlink_to(escape / ESCAPE_FILE)
    (pathlib.Path(runtime) / ESCAPE_DIRECTORY).symlink_to(
        escape / ESCAPE_DIRECTORY, target_is_directory=True
    )
    print(f"RUNTIME_CHILD_ESCAPES:{ESCAPE_FILE},{ESCAPE_DIRECTORY}", flush=True)
    os.kill(os.getpid(), signal.SIGKILL)
    return 1


def nested_run(
    launcher: str, user: pathlib.Path, bus: str | None
) -> tuple[str, str, str] | str:
    """Run the launcher around `runtime_child`, with `user` as its runtime directory.

    Returns the child's directory, the bus address it saw and the launcher's
    standard error, or the reason the run is not the one expected.
    """
    environment = dict(os.environ)
    environment["XDG_RUNTIME_DIR"] = str(user)
    environment[ESCAPE_ENVIRONMENT] = str(user)
    environment.pop("DBUS_SESSION_BUS_ADDRESS", None)
    if bus is not None:
        environment["DBUS_SESSION_BUS_ADDRESS"] = bus
    result = subprocess.run(
        [launcher, sys.executable, __file__, "runtime-child", LINUX_BACKEND],
        env=environment,
        capture_output=True,
        text=True,
        check=False,
        timeout=30,
    )
    lines = dict(
        line.split(":", 1) for line in result.stdout.splitlines() if ":" in line
    )
    # The launcher states the child's status, and a SIGKILL is -9 on its own
    # exit as well as in the line (`_exit` keeps the low eight bits).
    if lines.get("TEST_RUNNER_CHILD_EXIT") != "-9" or result.returncode != 256 - 9:
        return f"launcher:{result.returncode}:{lines.get('TEST_RUNNER_CHILD_EXIT')}"
    if lines.get("RUNTIME_CHILD_ESCAPES") != f"{ESCAPE_FILE},{ESCAPE_DIRECTORY}":
        return f"escapes:{lines.get('RUNTIME_CHILD_ESCAPES')}"
    directory = lines.get("RUNTIME_CHILD_DIRECTORY", "")
    return directory, lines.get("RUNTIME_CHILD_BUS", ""), result.stderr


# [[BPR-1113]]. The launcher's paths out once its directory exists, each driven
# by the fault shim. A signal CTest forwards may arrive before the launcher has
# a child to pass it to; the three it forwards are each tried. A handler that
# cannot be installed and a wait that fails are the two early failures the
# review named. Each entry is the shim's fault, the child the launcher runs,
# and the launcher stage a failure must name, if it fails.
SLEEPING_CHILD = ("-c", "import time; time.sleep(20)")
PROMPT_CHILD = ("-c", "pass")
# [[BPR-1261]]. Only Linux runs these windows, and Windows' `signal` has no
# SIGHUP: naming it unguarded here failed the module's import there, and with
# it every row that loads this script.
LEAK_WINDOWS = (
    (f"signal:{signal.SIGINT.value}", SLEEPING_CHILD, None),
    (f"signal:{signal.SIGTERM.value}", SLEEPING_CHILD, None),
    *(
        ((f"signal:{signal.SIGHUP.value}", SLEEPING_CHILD, None),)
        if hasattr(signal, "SIGHUP")
        else ()
    ),
    ("handlers", PROMPT_CHILD, "signal-handlers"),
    ("wait", PROMPT_CHILD, "wait"),
)


def leak_window_run(
    launcher: str, shim: str, fault: str, child: tuple[str, ...], stage: str | None
) -> tuple[str | None, str]:
    """Run the launcher with one fault: why the run fails the row, if it does, and what it did.

    The directory the launcher made must be gone afterwards. A handler that
    cannot be installed may stop the launcher before it makes one at all, but
    a signal or a failed wait comes after the directory exists, so for those
    the shim must have seen it made, or the fault never reached the window.
    """
    with contained_temporary_directory(
        prefix="bpr-launcher-fault-", parent=str(RUNTIME_PARENT)
    ) as raw:
        record = pathlib.Path(raw) / "runtime-directory"
        environment = dict(os.environ)
        environment["LD_PRELOAD"] = shim
        environment["BPR_TEST_LAUNCHER_FAULT"] = fault
        environment["BPR_TEST_LAUNCHER_FAULT_RECORD"] = str(record)
        result = subprocess.run(
            [launcher, sys.executable, *child],
            env=environment,
            capture_output=True,
            text=True,
            check=False,
            timeout=30,
        )
        directory = record.read_text(encoding="utf-8") if record.exists() else ""
    reported = [
        line.split(":", 1)[1]
        for line in result.stdout.splitlines()
        if line.startswith("TEST_RUNNER_CHILD_EXIT:")
    ]
    observed = (
        f"exit={result.returncode}:child={','.join(reported) or '-'}"
        f":directory={'made' if directory else 'none'}"
    )
    if directory and os.path.lexists(directory):
        # The row's own leftover, named by the shim: removed by that exact name,
        # and only when it is the launcher's kind of directory.
        if row_runtime_directory(directory):
            shutil.rmtree(directory)
        return f"left:{directory}", observed
    if "BPR_TEST_RUNTIME_DIRECTORY_LEFT" in result.stderr:
        return "removal-failed", observed
    if not directory and stage != "signal-handlers":
        return "not-reached", observed
    if stage is None:
        # The launcher outlived the signal, passed it on and reported its child.
        if result.returncode < 0 or not reported:
            return "signalled", observed
        if result.returncode == 0:
            return "reported-clean", observed
        return None, observed
    if result.returncode != 125 or f"BPR_NETWORK_ISOLATION_UNAVAILABLE:{stage}:" not in result.stderr:
        return f"not-reached:{result.stderr.strip()[-200:]}", observed
    return None, observed


def ignored_hangup_reaches_child(launcher: str) -> str | None:
    """[[BPR-1113]]. A launcher started with SIGHUP ignored, as under `nohup`, hands that on.

    The launcher holds the three signals it forwards and installs its own
    handlers before it forks, so its child must put back the actions it was
    given. The child reads its own ignored set from `/proc`.
    """
    probe = (
        "import pathlib\n"
        "line = next(l for l in pathlib.Path('/proc/self/status').read_text().splitlines()"
        " if l.startswith('SigIgn:'))\n"
        "print('RUNTIME_CHILD_SIGHUP_IGNORED:' + str(bool(int(line.split()[1], 16) & 1)))\n"
    )
    result = subprocess.run(
        [launcher, sys.executable, "-c", probe],
        capture_output=True,
        text=True,
        check=False,
        timeout=30,
        preexec_fn=lambda: signal.signal(signal.SIGHUP, signal.SIG_IGN),
    )
    if "RUNTIME_CHILD_SIGHUP_IGNORED:True" not in result.stdout or result.returncode != 0:
        return f"{result.returncode}:{result.stdout.strip()[-200:]}"
    return None


def runtime_attempt(backend: str, launcher: str, shim: str) -> int:
    """[[BPR-0906]]. Each row gets a runtime directory of its own.

    A profile owner binds its activation endpoint in `$XDG_RUNTIME_DIR`, and
    only the owner's destructor unlinks it. A row that ended an owner with a
    signal therefore left the socket in the user's `/run/user/<uid>`, where
    nothing removes it. The launcher now names a fresh mode-0700 directory for
    each row and removes it when the row ends.

    This row's own directory is checked first. Then the launcher runs again
    from here, around a child that leaves a socket and a directory behind and
    dies by SIGKILL. A stand-in for the user's runtime directory holds a live
    endpoint and a session bus socket. The child must get a directory of its
    own, which must be gone afterwards. The stand-in must be unchanged and its
    live endpoint must still answer, so the cleanup cannot reach a live owner.
    With no bus address set, the child must still be given the stand-in's bus.
    An address that is already set must reach the child unchanged.

    [[BPR-1113]]. The child also plants links that lead out of its directory,
    to a file and to a directory in the stand-in. Their targets must be as
    they were planted, so the removal cannot follow a link out. Last, the
    launcher runs with the fault shim into each of its ways out after the
    directory exists: a forwarded signal before there is a child, a handler
    that cannot be installed and a wait that fails. None may leave the
    directory behind. Holding those signals must not change what the child
    inherits, so an ignored SIGHUP must still reach it ignored.
    """
    if backend != LINUX_BACKEND:
        print(f"NETWORK_ISOLATION_RUNTIME_NOT_APPLICABLE:{backend}", flush=True)
        return 1
    own = os.environ.get("XDG_RUNTIME_DIR", "")
    if not row_runtime_directory(own):
        print(f"NETWORK_ISOLATION_RUNTIME_NOT_PER_ROW:{own}", flush=True)
        return 1
    with contained_temporary_directory(
        prefix="bpr-user-runtime-", parent=str(RUNTIME_PARENT)
    ) as raw:
        user = pathlib.Path(raw)
        user.chmod(0o700)
        live = listening_unix_socket(user / "bpr-activate-live.sock")
        live.settimeout(2.0)
        bus_socket = listening_unix_socket(user / "bus")
        plant_escape_targets(user)
        try:
            before = sorted(os.listdir(user))
            sentinel = "unix:path=/nonexistent/bpr-0906-sentinel-bus"
            for bus, expected_bus in (
                (None, f"unix:path={user / 'bus'}"),
                (sentinel, sentinel),
            ):
                outcome = nested_run(launcher, user, bus)
                if isinstance(outcome, str):
                    print(f"NETWORK_ISOLATION_RUNTIME_RUN_FAILED:{outcome}", flush=True)
                    return 1
                directory, seen_bus, stderr = outcome
                if directory == str(user) or not directory.startswith(
                    str(RUNTIME_PARENT / RUNTIME_PREFIX)
                ):
                    print(f"NETWORK_ISOLATION_RUNTIME_SHARED:{directory}", flush=True)
                    return 1
                if os.path.lexists(directory) or "BPR_TEST_RUNTIME_DIRECTORY_LEFT" in stderr:
                    print(f"NETWORK_ISOLATION_RUNTIME_LEFT:{directory}", flush=True)
                    return 1
                reached = escape_target_reached(user)
                if reached is not None:
                    print(f"NETWORK_ISOLATION_RUNTIME_ESCAPE_REACHED:{reached}", flush=True)
                    return 1
                if seen_bus != expected_bus:
                    print(
                        f"NETWORK_ISOLATION_RUNTIME_BUS_CHANGED:{seen_bus}:{expected_bus}",
                        flush=True,
                    )
                    return 1
                after = sorted(os.listdir(user))
                if after != before:
                    print(
                        f"NETWORK_ISOLATION_RUNTIME_USER_CHANGED:{before}:{after}",
                        flush=True,
                    )
                    return 1
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                    client.settimeout(2.0)
                    client.connect(str(user / "bpr-activate-live.sock"))
                    connection, _ = live.accept()
                    connection.close()
        finally:
            live.close()
            bus_socket.close()
    hangup = ignored_hangup_reaches_child(launcher)
    if hangup is not None:
        print(f"NETWORK_ISOLATION_RUNTIME_SIGHUP_NOT_INHERITED:{hangup}", flush=True)
        return 1
    # Every window is tried, so one run names each that fails.
    failed = False
    for fault, child, stage in LEAK_WINDOWS:
        failure, observed = leak_window_run(launcher, shim, fault, child, stage)
        if failure is not None:
            print(
                f"NETWORK_ISOLATION_RUNTIME_WINDOW_LEFT:{fault}:{failure}:{observed}",
                flush=True,
            )
            failed = True
        else:
            print(f"NETWORK_ISOLATION_RUNTIME_WINDOW_CLOSED:{fault}:{observed}", flush=True)
    if failed:
        return 1
    print("NETWORK_ISOLATION_RUNTIME_PER_ROW", flush=True)
    return 0


def external_attempt(backend: str) -> int:
    accepted = DENIAL_ERRNOS.get(backend)
    if accepted is None:
        print(f"NETWORK_ISOLATION_EXTERNAL_UNKNOWN_BACKEND:{backend}", flush=True)
        return 1
    candidate = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    candidate.settimeout(2.0)
    result = candidate.connect_ex(("1.1.1.1", 443))
    candidate.close()
    if result not in accepted:
        print(f"NETWORK_ISOLATION_EXTERNAL_NOT_DENIED:{result}", flush=True)
        return 1
    if backend == LINUX_BACKEND:
        isolated, detail = linux_namespace_is_isolated()
        if not isolated:
            print(f"NETWORK_ISOLATION_EXTERNAL_NOT_ISOLATED:{detail}", flush=True)
            return 1
    if backend == WINDOWS_BACKEND:
        contained, detail = windows_appcontainer_is_entered()
        if not contained:
            print(f"NETWORK_ISOLATION_EXTERNAL_NOT_ISOLATED:{detail}", flush=True)
            return 1
    print(f"NETWORK_ISOLATION_EXTERNAL_DENIED:{result}", flush=True)
    return 0


def fixture_ports(required: int) -> list[int]:
    raw = os.environ.get("BPR_TEST_LOOPBACK_PORTS", "")
    if not raw:
        return [0] * required
    try:
        ports = [int(value) for value in raw.split(",") if value]
    except ValueError:
        return []
    if len(ports) < required or any(port < 1 or port > 65535 for port in ports):
        return []
    return ports


def fixture_token() -> bytes:
    return os.environ.get("BPR_TEST_SOCKET_TOKEN", "bpr-loopback").encode("ascii")


def one_loopback(
    family: socket.AddressFamily,
    address: str,
    port: int,
) -> bool:
    server = socket.socket(family, socket.SOCK_STREAM)
    server.bind((address, port))
    server.listen(1)
    selected_port = server.getsockname()[1]
    token = fixture_token()

    def serve() -> None:
        connection, _ = server.accept()
        with connection:
            connection.sendall(token)

    worker = threading.Thread(target=serve)
    worker.start()
    with socket.create_connection((address, selected_port), timeout=2.0) as client:
        received = client.recv(len(token) + 1)
    worker.join(timeout=2.0)
    server.close()
    return received == token and not worker.is_alive()


def unix_loopback() -> bool:
    configured_root = os.environ.get("BPR_TEST_SOCKET_ROOT")
    context = (
        contained_temporary_directory(parent=configured_root)
        if configured_root
        else contained_temporary_directory()
    )
    with context as directory:
        path = pathlib.Path(directory) / "fixture.sock"
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(str(path))
        server.listen(1)
        token = fixture_token()

        def serve() -> None:
            connection, _ = server.accept()
            with connection:
                connection.sendall(token)

        worker = threading.Thread(target=serve)
        worker.start()
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.connect(str(path))
        received = client.recv(len(token) + 1)
        client.close()
        worker.join(timeout=2.0)
        server.close()
        return received == token and not worker.is_alive()


def loopback_attempt() -> int:
    ports = fixture_ports(1)
    if len(ports) != 1:
        print("NETWORK_ISOLATION_LOOPBACK_FAILED:ports", flush=True)
        return 1
    ipv6_available = socket.has_ipv6
    ipv4_ok = one_loopback(socket.AF_INET, "127.0.0.1", ports[0])
    ipv6_ok = not ipv6_available or one_loopback(socket.AF_INET6, "::1", ports[0])
    # CPython exposes no AF_UNIX on Windows, so there is no Unix channel to
    # allow or deny there and asking for one is an AttributeError rather than
    # a result. The channel that does not exist is named in the output instead
    # of being silently folded into a pass, so a host that stops offering
    # AF_UNIX cannot look the same as one that proved it.
    unix_available = hasattr(socket, "AF_UNIX")
    unix_ok = unix_loopback() if unix_available else True
    if not (ipv4_ok and ipv6_ok and unix_ok):
        print(
            f"NETWORK_ISOLATION_LOOPBACK_FAILED:{ipv4_ok}:{ipv6_ok}:{unix_ok}",
            flush=True,
        )
        return 1
    channels = "ipv4" + (":ipv6" if ipv6_available else "") + (
        ":unix" if unix_available else ""
    )
    print(f"NETWORK_ISOLATION_LOOPBACK_OK:{channels}", flush=True)
    return 0


def denied_connect(family: socket.AddressFamily, endpoint: object) -> bool:
    candidate = socket.socket(family, socket.SOCK_STREAM)
    candidate.settimeout(2.0)
    try:
        result = candidate.connect_ex(endpoint)
    finally:
        candidate.close()
    return result in (errno.EPERM, errno.EACCES)


def unrelated_attempt() -> int:
    port = os.environ.get("BPR_UNRELATED_LOOPBACK_PORT", "")
    unix_path = os.environ.get("BPR_UNRELATED_UNIX_SOCKET", "")
    if not port.isdigit() or not unix_path:
        print("NETWORK_ISOLATION_UNRELATED_FAILED:missing-endpoints", flush=True)
        return 1
    allowed_ok = loopback_attempt() == 0
    tcp_denied = denied_connect(socket.AF_INET, ("127.0.0.1", int(port)))
    unix_denied = denied_connect(socket.AF_UNIX, unix_path)
    if not (allowed_ok and tcp_denied and unix_denied):
        print(
            "NETWORK_ISOLATION_UNRELATED_FAILED:"
            f"{allowed_ok}:{tcp_denied}:{unix_denied}",
            flush=True,
        )
        return 1
    print("NETWORK_ISOLATION_UNRELATED_DENIED", flush=True)
    return 0


def child_attempt(backend: str) -> int:
    child = subprocess.run(
        [sys.executable, __file__, "external", backend],
        capture_output=True,
        text=True,
        check=False,
        timeout=10,
    )
    if child.returncode != 0 or "NETWORK_ISOLATION_EXTERNAL_DENIED" not in child.stdout:
        print("NETWORK_ISOLATION_CHILD_NOT_DENIED", flush=True)
        print(child.stdout, end="")
        print(child.stderr, end="", file=sys.stderr)
        return 1
    print("NETWORK_ISOLATION_CHILD_DENIED", flush=True)
    return 0


def contained_attempt(backend: str) -> int:
    """Report from inside a launch whether this process holds the container.

    [[BPR-1072]]. The child each concurrent launch below starts. Its exit
    status alone would pass a launcher that ran it uncontained, so it says
    which token it holds.
    """
    if backend != WINDOWS_BACKEND:
        return 2
    contained, detail = windows_appcontainer_is_entered()
    marker = "NETWORK_ISOLATION_CONTAINED" if contained else "NETWORK_ISOLATION_NOT_CONTAINED"
    print(f"{marker}:{detail}", flush=True)
    return 0 if contained else 1


# [[BPR-1072]]. Four launchers at once for eight rounds. Before the launcher
# serialized its profile step, four at once lost 87 of 100 launches on the
# Windows station, and three at once lost a row in half the runs.
CONCURRENT_WIDTH = 4
CONCURRENT_ROUNDS = 8


def concurrent_attempt(backend: str, launcher: str) -> int:
    """Start several launchers at the same moment and require every child contained.

    Every contained Windows row enters the one `BprAutomatedTest` profile
    through the launcher, and under `ctest -j` several enter it at once. Each
    launch must reach its child, and the child must hold the AppContainer
    token; a launch the launcher refuses, or one that ran uncontained, fails
    the row, and the row names what each launch reported.
    """
    if backend != WINDOWS_BACKEND:
        print("NETWORK_ISOLATION_CONCURRENT_UNSUPPORTED", flush=True)
        return 2
    command = [launcher, sys.executable, __file__, "contained", backend]
    launches: list[subprocess.CompletedProcess[str] | BaseException] = []
    record = threading.Lock()
    for _ in range(CONCURRENT_ROUNDS):
        start = threading.Barrier(CONCURRENT_WIDTH)

        def launch() -> None:
            start.wait()
            try:
                outcome: subprocess.CompletedProcess[str] | BaseException = subprocess.run(
                    command, capture_output=True, text=True, check=False, timeout=60
                )
            except (OSError, subprocess.SubprocessError) as error:
                outcome = error
            with record:
                launches.append(outcome)

        threads = [threading.Thread(target=launch) for _ in range(CONCURRENT_WIDTH)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()

    failures: list[str] = []
    for outcome in launches:
        if isinstance(outcome, BaseException):
            failures.append(f"launch-error:{outcome!r}")
            continue
        output = outcome.stdout + outcome.stderr
        refusal = [line for line in output.splitlines() if line.startswith("BPR_NETWORK_ISOLATION_UNAVAILABLE")]
        if outcome.returncode != 0 or refusal or "NETWORK_ISOLATION_CONTAINED:appcontainer" not in outcome.stdout:
            failures.append(f"exit={outcome.returncode} " + (" ".join(refusal) or output.strip()[-200:]))
    if failures:
        print(f"NETWORK_ISOLATION_CONCURRENT_LAUNCH_FAILED:{len(failures)}/{len(launches)}", flush=True)
        for failure in failures:
            print(failure)
        return 1
    print(f"NETWORK_ISOLATION_CONCURRENT_LAUNCHES_CONTAINED:{len(launches)}", flush=True)
    return 0


def main() -> int:
    if len(sys.argv) == 5 and sys.argv[1] == "runtime":
        return runtime_attempt(sys.argv[2], sys.argv[3], sys.argv[4])
    if len(sys.argv) == 4 and sys.argv[1] == "concurrent":
        return concurrent_attempt(sys.argv[2], sys.argv[3])
    if len(sys.argv) != 3:
        return 2
    probe, backend = sys.argv[1], sys.argv[2]
    if probe == "runtime-child":
        return runtime_child()
    if probe == "external":
        return external_attempt(backend)
    if probe == "identity":
        return identity_attempt(backend)
    if probe == "loopback":
        return loopback_attempt()
    if probe == "child":
        return child_attempt(backend)
    if probe == "contained":
        return contained_attempt(backend)
    if probe == "unrelated":
        return unrelated_attempt()
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
