"""Exercise the built terminal entry without display or test-only application modes."""
import errno
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from scripts.contained_temporary_support import contained_temporary_directory  # noqa: E402

VAULT_REFUSED = "No unlocked Secret Service default collection is reachable"
VAULT_LOCKED = "The Secret Service default collection is locked"
NOTHING_REQUESTED = "Nothing was requested from GitHub."


def terminal_connect(binary, environment, data_root, seconds=20):
    """`bpr tour connect` on a pseudo-terminal, as a person would type it; returns exit and output."""
    import pty
    master, slave = pty.openpty()
    child = subprocess.Popen([binary, "tour", "connect", "--host", "github.com", "--transport", "native",
                              "--data-root", str(data_root)], stdin=slave, stdout=slave, stderr=slave,
                             env=environment)
    os.close(slave)
    output = bytearray()
    deadline = time.monotonic() + seconds
    try:
        while child.poll() is None or select.select([master], [], [], 0)[0]:
            if time.monotonic() > deadline and child.poll() is None:
                child.send_signal(signal.SIGINT)
                deadline = time.monotonic() + 5
            if select.select([master], [], [], 0.05)[0]:
                try:
                    part = os.read(master, 8192)
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    raise
                if not part:
                    break
                output.extend(part)
        child.wait(timeout=10)
    finally:
        os.close(master)
        if child.poll() is None:
            child.kill()
            child.wait()
    return child.returncode, output.decode("utf-8", "replace")


def linux_headless_sign_in(binary):
    """[[BPR-1068]] Native sign-in asks the Secret Service before it shows anyone a code.

    Before, a terminal with no reachable Secret Service started the device flow
    and only failed when it tried to store the grant, after a person had
    authorized it. Each case runs on a bus this test starts, never the user's.
    """
    import linux_secret_service_private_session as session
    with contained_temporary_directory(prefix="bpr-tour-vault-") as temporary:
        root = Path(temporary)

        def environment(name, extra):
            row = session.row_environment(root / name, extra)
            row.update(QT_QPA_PLATFORM="bpr-no-display", NO_COLOR="1")
            return row

        def refused(name, extra, expected):
            code, output = terminal_connect(binary, environment(name, extra), root / name / "data")
            assert code == 5, (name, code, output)
            assert expected in output and NOTHING_REQUESTED in output, (name, output)
            assert "login/device" not in output and "enter code" not in output, (name, output)

        # No bus at all: the address names a socket nobody listens on.
        refused("no-bus", {"DBUS_SESSION_BUS_ADDRESS": f"unix:path={root / 'absent-bus'}"}, VAULT_REFUSED)
        bus = session.PrivateBus(session.tool("dbus-daemon"), root / "bus")
        keyring = None
        try:
            # A live bus with no Secret Service on it, and nothing it could activate.
            refused("bus-without-keyring", {"DBUS_SESSION_BUS_ADDRESS": bus.address}, VAULT_REFUSED)
            keyring = session.start_keyring(session.tool("gnome-keyring-daemon"), session.tool("dbus-send"), bus,
                                            root / "keyring")
            # An unlocked default collection passes into the device flow, whose request
            # then meets the test's network boundary and waits: no code is shown and
            # nothing is sent. The test interrupts it, as a person would.
            code, output = terminal_connect(binary, environment("unlocked", {"DBUS_SESSION_BUS_ADDRESS": bus.address}),
                                            root / "unlocked" / "data", seconds=5)
            assert code == 130 and VAULT_REFUSED not in output and VAULT_LOCKED not in output, (code, output)
            assert "enter code" not in output, output
            # The same collection locked: refused with the unlock instruction.
            alias = session.bus_call(session.tool("dbus-send"), bus.address, session.SECRET_SERVICE,
                                     "/org/freedesktop/secrets", "org.freedesktop.Secret.Service.ReadAlias",
                                     "string:default")
            collection = alias.split('object path "', 1)[1].split('"', 1)[0]
            assert session.bus_call(session.tool("dbus-send"), bus.address, session.SECRET_SERVICE,
                                    "/org/freedesktop/secrets", "org.freedesktop.Secret.Service.Lock",
                                    f"array:objpath:{collection}") is not None
            refused("locked", {"DBUS_SESSION_BUS_ADDRESS": bus.address}, VAULT_LOCKED)
        finally:
            if keyring is not None:
                session.stop(keyring)
            assert not bus.stop(), "the test bus activated a service"
    print("tour-cli-headless-sign-in:pass")


def main():
    binary = str(Path(sys.argv[1]).resolve())
    environment = dict(os.environ, QT_QPA_PLATFORM="bpr-no-display", NO_COLOR="1")
    environment.pop("DISPLAY", None)
    environment.pop("WAYLAND_DISPLAY", None)
    result = subprocess.run([binary, "tour", "capabilities", "--json"], env=environment,
                            capture_output=True, text=True, check=True, timeout=15)
    value = json.loads(result.stdout)
    assert set(value) == {"schema", "command", "request_id", "status", "data", "warnings", "error"}
    assert value["schema"] == "bpr.tour.cli-result.v1"
    assert value["data"]["gui_required"] is False
    assert value["data"]["ai_required"] is False
    assert value["status"] == "ok" and value["error"] is None
    human = subprocess.run([binary, "tour", "capabilities"], env=environment,
                           capture_output=True, text=True, check=True, timeout=15)
    assert human.stdout.startswith("bpr tour") and not human.stdout.startswith("{")
    # [[BPR-1068]] Discovery names its own platform and claims no qualification.
    platform = value["data"]["platform"]
    expected_os = "macos" if sys.platform == "darwin" else "windows" if sys.platform == "win32" else "linux"
    assert platform["os"] == expected_os, platform
    assert platform["qualification"] == "not yet qualified", platform
    assert platform["package"] == "development install; no release package", platform
    assert platform["gh_transport"] == ("explicit opt-in" if expected_os == "macos" else "unavailable"), platform
    assert f"Platform: {expected_os}, not yet qualified." in human.stdout, human.stdout
    refused = subprocess.run([binary, "tour", "capabilities", "--yes", "--json"],
                             env=environment, capture_output=True, text=True, timeout=15)
    assert refused.returncode == 2 and json.loads(refused.stdout)["status"] == "error"
    if sys.platform.startswith("linux"):
        linux_headless_sign_in(binary)
    print("tour-cli-public-entry:pass")


if __name__ == "__main__":
    main()
