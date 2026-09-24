#!/usr/bin/env python3
"""Runs a Linux Secret Service row on a private session bus this harness starts.

[[BPR-1117]]. The native Secret Service rows write, replace and delete an item
in the collection the `default` alias names. On whatever bus the shell names,
that is the user's own keyring, so the rows refuse every bus that is not
designated for tests (`linux_secret_service_native_tests.cpp`). This is the
harness that designates one:

1. It makes an owner-private root and starts a `dbus-daemon` on a path socket
   inside it. The bus's configuration names no service directory, so the bus
   activates nothing: every process on it is one this harness started.
2. It starts a test-only `gnome-keyring-daemon` in the foreground on that bus,
   under fresh directories, and unlocks it from standard input with a random
   password that is never printed or stored.
3. It waits until the `default` alias names an unlocked collection.
4. It runs the row with `DBUS_SESSION_BUS_ADDRESS` and
   `BPR_DESIGNATED_TEST_SESSION_BUS` both set to the exact address the daemon
   printed, `guid=` included, and fresh `HOME` and XDG directories.
5. It stops the keyring and then the bus by the exact process each `Popen`
   holds, and removes the root.

Both daemons, and the row, are started with `PR_SET_PDEATHSIG`, so a harness
killed outright takes them with it. Its root then stays in the temporary
directory until it is removed by name.

`--refusal-control` pins the refusals instead. It starts the designated bus and
a second one, with no keyring on either, and requires the row executable to
refuse each bus that is not the designated one with its named line, before it
calls the vault.

The daemons come from `/usr/bin:/bin`. Without them the row fails with
`linux-secret-service-private-session:unavailable:<tool>-missing`; nothing
falls back to another bus.
"""

from __future__ import annotations

import argparse
import ctypes
import os
import re
import secrets
import select
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path
from xml.sax.saxutils import escape

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

DESIGNATION_VARIABLE = "BPR_DESIGNATED_TEST_SESSION_BUS"
PREFIX = "linux-secret-service-private-session"
CONTROL_PREFIX = "linux-secret-service-refusal-control"
TOOL_PATH = "/usr/bin:/bin"
ADDRESS = re.compile(r"unix:path=(/[^,;]+),guid=([0-9a-f]{32})")
SECRET_SERVICE = "org.freedesktop.secrets"
# `sun_path` on Linux, terminator included.
SUN_PATH_BYTES = 108
START_SECONDS = 5.0
READY_SECONDS = 10.0
STOP_SECONDS = 3.0
CONTROL_ROW_SECONDS = 10.0
PR_SET_PDEATHSIG = 1

BUS_CONFIGURATION = """<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:path={socket}</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
"""


class Unavailable(Exception):
    """The private session could not be started; the reason is a fixed token."""


def say(line: str) -> None:
    print(line, flush=True)


def tool(name: str) -> str:
    found = shutil.which(name, path=TOOL_PATH)
    if found is None:
        raise Unavailable(f"{name}-missing")
    return found


def private_directory(path: Path) -> Path:
    path.mkdir(mode=0o700, parents=True)
    path.chmod(0o700)
    return path


_LIBC = ctypes.CDLL(None, use_errno=True)
_HARNESS = os.getpid()


def _die_with_harness() -> None:
    """Runs in each child before `exec`: it ends when this harness does."""
    if _LIBC.prctl(PR_SET_PDEATHSIG, signal.SIGKILL, 0, 0, 0) != 0:
        os._exit(125)
    if os.getppid() != _HARNESS:
        os._exit(125)


def _raise_exit(number: int, _frame: object) -> None:
    raise SystemExit(128 + number)


def stop(process: subprocess.Popen[bytes]) -> bool:
    """Stops one process this harness started, by the exact child `Popen` holds."""
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=STOP_SECONDS)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=STOP_SECONDS)
    return process.returncode is not None


def children_of(process: subprocess.Popen[bytes]) -> list[str]:
    try:
        text = Path(f"/proc/{process.pid}/task/{process.pid}/children").read_text()
    except OSError:
        return []
    return text.split()


class PrivateBus:
    """A `dbus-daemon` on a path socket, with no service directories."""

    def __init__(self, daemon: str, directory: Path) -> None:
        self.directory = private_directory(directory)
        socket = directory / "bus"
        if len(os.fsencode(socket)) >= SUN_PATH_BYTES:
            raise Unavailable("bus-path-too-long")
        configuration = directory / "bus.conf"
        configuration.write_text(BUS_CONFIGURATION.format(socket=escape(str(socket))), encoding="utf-8")
        read_end, write_end = os.pipe()
        try:
            with (directory / "daemon.log").open("wb") as log:
                self.process = subprocess.Popen(
                    [
                        daemon,
                        f"--config-file={configuration}",
                        "--nofork",
                        "--nopidfile",
                        "--nosyslog",
                        f"--print-address={write_end}",
                    ],
                    env={"PATH": TOOL_PATH, "LANG": "C", "HOME": str(directory)},
                    stdin=subprocess.DEVNULL,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    pass_fds=(write_end,),
                    preexec_fn=_die_with_harness,
                )
        except BaseException:
            os.close(read_end)
            raise
        finally:
            os.close(write_end)
        try:
            line = self._read_address(read_end)
        finally:
            os.close(read_end)
        match = ADDRESS.fullmatch(line)
        if match is None or Path(match.group(1)) != socket:
            stop(self.process)
            raise Unavailable("bus-address-unexpected")
        self.address = line
        self.socket = socket
        self.guid = match.group(2)

    def _read_address(self, descriptor: int) -> str:
        deadline = time.monotonic() + START_SECONDS
        text = b""
        while b"\n" not in text:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([descriptor], [], [], remaining)[0]:
                stop(self.process)
                raise Unavailable("bus-did-not-start")
            chunk = os.read(descriptor, 4096)
            if not chunk:
                stop(self.process)
                raise Unavailable("bus-did-not-start")
            text += chunk
        return text.split(b"\n", 1)[0].decode("ascii", "strict")

    def stop(self) -> list[str]:
        """Stops the daemon; returns what it had activated, which should be nothing."""
        activated = children_of(self.process)
        for pid in activated:
            # A child of this bus is one it activated for this harness alone.
            try:
                os.kill(int(pid), signal.SIGKILL)
            except ProcessLookupError:
                pass
        stop(self.process)
        return activated


def bus_call(dbus_send: str, address: str, destination: str, path: str, method: str, *arguments: str) -> str | None:
    result = subprocess.run(
        [dbus_send, f"--bus={address}", "--print-reply", "--reply-timeout=2000", f"--dest={destination}", path, method, *arguments],
        env={"PATH": TOOL_PATH, "LANG": "C"},
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
        timeout=STOP_SECONDS,
        check=False,
    )
    return result.stdout if result.returncode == 0 else None


def secret_service_ready(dbus_send: str, address: str) -> bool:
    """True once the `default` alias names an unlocked collection."""
    owner = bus_call(dbus_send, address, "org.freedesktop.DBus", "/", "org.freedesktop.DBus.NameHasOwner", f"string:{SECRET_SERVICE}")
    if owner is None or "boolean true" not in owner:
        return False
    alias = bus_call(dbus_send, address, SECRET_SERVICE, "/org/freedesktop/secrets", "org.freedesktop.Secret.Service.ReadAlias", "string:default")
    collection = re.search(r'object path "([^"]+)"', alias or "")
    if collection is None or collection.group(1) == "/":
        return False
    locked = bus_call(dbus_send, address, SECRET_SERVICE, collection.group(1), "org.freedesktop.DBus.Properties.Get", "string:org.freedesktop.Secret.Collection", "string:Locked")
    return locked is not None and "boolean false" in locked


def start_keyring(keyring: str, dbus_send: str, bus: PrivateBus, directory: Path) -> subprocess.Popen[bytes]:
    home = private_directory(directory / "home")
    environment = {
        "PATH": TOOL_PATH,
        "LANG": "C",
        "HOME": str(home),
        "DBUS_SESSION_BUS_ADDRESS": bus.address,
    }
    for variable, name in (
        ("XDG_RUNTIME_DIR", "runtime"),
        ("XDG_DATA_HOME", "data"),
        ("XDG_CONFIG_HOME", "config"),
        ("XDG_CACHE_HOME", "cache"),
    ):
        environment[variable] = str(private_directory(directory / name))
    with (directory / "keyring.log").open("wb") as log:
        process = subprocess.Popen(
            [keyring, "--foreground", "--unlock", "--components=secrets"],
            env=environment,
            stdin=subprocess.PIPE,
            stdout=log,
            stderr=subprocess.STDOUT,
            preexec_fn=_die_with_harness,
        )
    # The password exists only on this pipe; it is never printed or stored.
    assert process.stdin is not None
    process.stdin.write(secrets.token_urlsafe(24).encode("ascii"))
    process.stdin.close()
    deadline = time.monotonic() + READY_SECONDS
    while not secret_service_ready(dbus_send, bus.address):
        if process.poll() is not None:
            raise Unavailable("keyring-exited")
        if time.monotonic() > deadline:
            raise Unavailable("keyring-not-ready")
        time.sleep(0.05)
    return process


def row_environment(directory: Path, extra: dict[str, str]) -> dict[str, str]:
    environment = {"PATH": TOOL_PATH, "LANG": "C"}
    for variable, name in (
        ("HOME", "home"),
        ("XDG_CONFIG_HOME", "config"),
        ("XDG_DATA_HOME", "data"),
        ("XDG_CACHE_HOME", "cache"),
        ("XDG_STATE_HOME", "state"),
        ("XDG_RUNTIME_DIR", "runtime"),
        ("TMPDIR", "tmp"),
    ):
        environment[variable] = str(private_directory(directory / name))
    boundary = os.environ.get("BPR_AUTOMATED_TEST_BOUNDARY")
    if boundary is not None:
        environment["BPR_AUTOMATED_TEST_BOUNDARY"] = boundary
    environment.update(extra)
    return environment


def exit_code(returncode: int) -> int:
    return 128 - returncode if returncode < 0 else returncode


def run_row(root: Path, command: list[str]) -> int:
    dbus_send = tool("dbus-send")
    bus = PrivateBus(tool("dbus-daemon"), root / "bus")
    keyring_process: subprocess.Popen[bytes] | None = None
    row = 1
    try:
        keyring_process = start_keyring(tool("gnome-keyring-daemon"), dbus_send, bus, root / "keyring")
        say(f"{PREFIX}:started:bus-pid={bus.process.pid}:keyring-pid={keyring_process.pid}")
        environment = row_environment(root / "row", {"DBUS_SESSION_BUS_ADDRESS": bus.address, DESIGNATION_VARIABLE: bus.address})
        row = exit_code(subprocess.run(command, env=environment, stdin=subprocess.DEVNULL, check=False, preexec_fn=_die_with_harness).returncode)
    finally:
        keyring_stopped = keyring_process is None or stop(keyring_process)
        activated = bus.stop()
        clean = keyring_stopped and bus.process.returncode is not None and not activated
        if activated:
            say(f"{PREFIX}:activated:{','.join(activated)}")
    say(f"{PREFIX}:stopped" if clean else f"{PREFIX}:left-running")
    return row if row != 0 or clean else 1


# Each case: its name, the mode, the environment the row sees, and the refusal it must print.
def refusal_cases(designated: PrivateBus, other: PrivateBus) -> list[tuple[str, str, dict[str, str], str]]:
    unguarded = f"unix:path={designated.socket}"
    impostor = f"{unguarded},guid={other.guid}"
    return [
        ("no-designation", "--fixture-setup", {"DBUS_SESSION_BUS_ADDRESS": other.address}, "undesignated"),
        ("other-bus-by-address", "--fixture-setup", {"DBUS_SESSION_BUS_ADDRESS": other.address, DESIGNATION_VARIABLE: designated.address}, "not-designated"),
        ("other-bus-by-runtime-directory", "--fixture-setup", {"XDG_RUNTIME_DIR": str(other.directory), DESIGNATION_VARIABLE: designated.address}, "not-designated"),
        ("designation-without-guid", "--fixture-setup", {"DBUS_SESSION_BUS_ADDRESS": unguarded, DESIGNATION_VARIABLE: unguarded}, "designation-malformed"),
        ("designated-path-other-server", "--fixture-setup", {"DBUS_SESSION_BUS_ADDRESS": impostor, DESIGNATION_VARIABLE: impostor}, "bus-identity-mismatch"),
        ("native-row-on-other-bus", "", {"DBUS_SESSION_BUS_ADDRESS": other.address, DESIGNATION_VARIABLE: designated.address}, "not-designated"),
        ("unavailable-with-runtime-bus", "--expect-unavailable", {"XDG_RUNTIME_DIR": str(other.directory)}, "session-bus-named"),
    ]


def control_run(root: Path, name: str, executable: str, mode: str, extra: dict[str, str]) -> tuple[int, str]:
    environment = row_environment(root / "rows" / name, extra)
    if "XDG_RUNTIME_DIR" not in extra:
        # The refusals must not depend on a runtime directory the row happens to have.
        environment.pop("XDG_RUNTIME_DIR")
    command = [executable, mode] if mode else [executable]
    result = subprocess.run(command, env=environment, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=CONTROL_ROW_SECONDS, check=False, preexec_fn=_die_with_harness)
    return result.returncode, result.stdout


def refusal_control(root: Path, executable: str) -> int:
    daemon = tool("dbus-daemon")
    designated = PrivateBus(daemon, root / "designated")
    other: PrivateBus | None = None
    failures: list[str] = []
    try:
        other = PrivateBus(daemon, root / "other")
        for name, mode, extra, reason in refusal_cases(designated, other):
            code, stdout = control_run(root, name, executable, mode, extra)
            expected = f"linux-secret-service-bus:refused:{reason}\n"
            passed = code != 0 and stdout == expected
            say(f"{CONTROL_PREFIX}:{name}:{'refused:' + reason if passed else 'FAILED'}")
            if not passed:
                failures.append(f"{name}: exit {code}, output {stdout!r}, expected {expected!r}")
        # The designated bus is let through to the vault, which finds no Secret Service on it.
        code, stdout = control_run(root, "designated", executable, "--fixture-setup", {"DBUS_SESSION_BUS_ADDRESS": designated.address, DESIGNATION_VARIABLE: designated.address})
        passed = code != 0 and stdout == "linux-secret-service-setup:fail\n"
        say(f"{CONTROL_PREFIX}:designated:{'reached-the-vault' if passed else 'FAILED'}")
        if not passed:
            failures.append(f"designated: exit {code}, output {stdout!r}")
    finally:
        activated = designated.stop() + (other.stop() if other is not None else [])
        if activated:
            failures.append(f"a control bus activated {activated!r}")
    for failure in failures:
        print(f"{CONTROL_PREFIX}: {failure}", file=sys.stderr, flush=True)
    say(f"{CONTROL_PREFIX}:{'pass' if not failures else 'fail'}")
    return 0 if not failures else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--refusal-control", action="store_true")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    arguments = parser.parse_args()
    command = arguments.command[1:] if arguments.command[:1] == ["--"] else arguments.command
    if not command or (arguments.refusal_control and len(command) != 1):
        parser.error("expected -- EXECUTABLE [ARGUMENT ...]")
    for number in (signal.SIGTERM, signal.SIGHUP):
        signal.signal(number, _raise_exit)
    try:
        with contained_temporary_directory(prefix="bpr-secret-service-") as root:
            if arguments.refusal_control:
                return refusal_control(Path(root), command[0])
            return run_row(Path(root), command)
    except Unavailable as reason:
        say(f"{PREFIX}:unavailable:{reason}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
