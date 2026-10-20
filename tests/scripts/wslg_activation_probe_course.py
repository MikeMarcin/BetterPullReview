"""Probe which window-activation requests WSLg's compositor grants.

BPR-0975, on the owner's direction (decision 35aa7cb0) to find out why WSLg
refuses the application's activation of its Settings window. Controls only: a
toolkit-free Xlib probe and the bare two-window Qt control, no BetterPullReview
code, each an owned PID with fresh mode-0700 XDG and TMP roots.

1. Environment: the WSLg version, the X11 window manager's name and
   `_NET_SUPPORTED`, and the Wayland globals the compositor advertises (one
   registry round trip on its socket; no surface is created).
2. `linux_xlib_activation_probe.cpp`. Window B holds X11 focus and has just
   received a real key. Window A then asks for activation in each way an X11
   client can: `_NET_ACTIVE_WINDOW` with source indication 0, 1 or 2 and
   CurrentTime, the key's time (also written as A's `_NET_WM_USER_TIME`) or a
   fresh server time; `XSetInputFocus` with a fresh server time;
   `XRaiseWindow` followed by the application request; and last, unmapping and
   mapping A again.
3. `linux_settings_focus_control.cpp` on xcb. After a key to B, Qt's own
   `activateWindow()` for A, with the request fields Qt sent; then `remap A`
   (hide, show, raise, activateWindow).
4. The same control on Wayland: `activateWindow()` for A while B is active,
   then `remap A`. Qt's focus window there follows the compositor's
   xdg_toplevel activated state.

Each step records the focus events, the X11 or Qt focus one second later, the
root `_NET_ACTIVE_WINDOW` the window manager publishes and, with the
`wslg-rail` surface, the Windows foreground window (named only when it is an
msrdc RAIL window). It is a diagnosis. It exits 0 when every control ran and
exited 0, whichever requests were granted. Run it with the system
`/usr/bin/python3` under the station lease and the live-state guard.
"""
from __future__ import annotations

import argparse
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import platform
import socket
import struct
import subprocess
import sys
import time

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import linux_settings_focus_control_course as control  # noqa: E402
import linux_settings_inventory as observer  # noqa: E402
from scripts.contained_temporary_support import make_contained_temporary_directory  # noqa: E402

HERE = Path(__file__).resolve().parent
XLIB_PROBE = HERE / "linux_xlib_activation_probe.cpp"
KEY_K = 0x6b
SETTLE = 1.0
XLIB_TITLE = "BPR0975 activation probe "
QT_TITLE = "BPR0018 native focus control "
# Each asks for A's activation while B holds X11 focus; the last one remaps A.
XLIB_REQUESTS = (
    ("net-active-source1-current-time", ["net-active A 1 0"]),
    ("net-active-source1-user-time", ["net-active A 1 user"]),
    ("net-active-source1-server-time", ["net-active A 1 server"]),
    ("net-active-source2-user-time", ["net-active A 2 user"]),
    ("net-active-source2-server-time", ["net-active A 2 server"]),
    ("net-active-source0-current-time", ["net-active A 0 0"]),
    ("set-input-focus-server-time", ["set-focus A server"]),
    ("raise-then-net-active-source1-user-time", ["raise A", "net-active A 1 user"]),
    ("unmap-then-map", ["remap A"]),
)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def wayland_globals():
    """The compositor's globals from one wl_registry round trip; no surface is created."""
    name = os.environ.get("WAYLAND_DISPLAY", "")
    if not name:
        return dict(error="WAYLAND_DISPLAY is not set")
    path = Path(name) if name.startswith("/") else Path(os.environ.get("XDG_RUNTIME_DIR", "")) / name
    found, buffer, done = [], b"", False
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
            connection.settimeout(5)
            connection.connect(str(path))
            # wl_display@1: get_registry (opcode 1, new id 2), then sync (opcode 0, new id 3).
            connection.sendall(struct.pack("<III", 1, (12 << 16) | 1, 2) + struct.pack("<III", 1, (12 << 16) | 0, 3))
            while not done:
                chunk = connection.recv(65536)
                if not chunk:
                    return dict(error="compositor closed the connection", globals=found)
                buffer += chunk
                while len(buffer) >= 8 and not done:
                    sender, word = struct.unpack_from("<II", buffer)
                    size, opcode = word >> 16, word & 0xffff
                    if size < 8 or len(buffer) < size:
                        break
                    body, buffer = buffer[8:size], buffer[size:]
                    if sender == 2 and opcode == 0:  # wl_registry.global(name, interface, version)
                        global_name, length = struct.unpack_from("<II", body)
                        interface = body[8:8 + length - 1].decode("ascii", "replace")
                        (version,) = struct.unpack_from("<I", body, 8 + ((length + 3) & ~3))
                        found.append(dict(name=global_name, interface=interface, version=version))
                    elif sender == 3 and opcode == 0:  # wl_callback.done
                        done = True
                    elif sender == 1 and opcode == 0:  # wl_display.error
                        return dict(error=f"wl_display.error {body.hex()}", globals=found)
    except OSError as failure:
        return dict(error=f"{type(failure).__name__}: {failure}", globals=found)
    interfaces = sorted({entry["interface"] for entry in found})
    return dict(socket=str(path), interfaces=interfaces, xdg_activation_v1="xdg_activation_v1" in interfaces,
                globals=found)


class ClientMessage(c.Structure):
    _fields_ = [("type", c.c_int), ("serial", c.c_ulong), ("send_event", c.c_int), ("display", c.c_void_p),
                ("window", c.c_ulong), ("message_type", c.c_ulong), ("format", c.c_int), ("data", c.c_long * 5)]


class RootEvent(c.Union):
    _fields_ = [("type", c.c_int), ("message", ClientMessage), ("pad", c.c_long * 24)]


class RootWatch:
    """Every `_NET_ACTIVE_WINDOW` client message sent to the root, with all its fields."""

    def __init__(self, x):
        self.x = x
        self.active = x.x.XInternAtom(x.display, b"_NET_ACTIVE_WINDOW", 0)
        x.x.XSelectInput.argtypes = [c.c_void_p, c.c_ulong, c.c_long]
        x.x.XPending.argtypes = [c.c_void_p]
        x.x.XNextEvent.argtypes = [c.c_void_p, c.c_void_p]
        x.x.XSelectInput(x.display, x.root, (1 << 17) | (1 << 19))  # StructureNotify | SubstructureNotify
        x.x.XSync(x.display, 0)

    def drain(self, names):
        requests = []
        while self.x.x.XPending(self.x.display):
            event = RootEvent()
            self.x.x.XNextEvent(self.x.display, c.byref(event))
            message = event.message
            if event.type == 33 and message.message_type == self.active:
                requests.append(dict(window=names.get(message.window, message.window), source=message.data[0],
                                     time=message.data[1], current=names.get(message.data[2], message.data[2])))
        return requests

    def close(self):
        self.x.x.XSelectInput(self.x.display, self.x.root, 0)
        self.x.x.XSync(self.x.display, 0)


def owned_environment(root, qpa):
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = qpa
    for key in ("DATA", "CONFIG", "CACHE", "STATE"):
        (root / key.lower()).mkdir(mode=0o700)
        environment[f"XDG_{key}_HOME"] = str(root / key.lower())
    (root / "tmp").mkdir(mode=0o700)
    environment["TMPDIR"] = str(root / "tmp")
    return environment


def foreground(rail, captions):
    """The Windows foreground window, named A or B when it is one of the owned RAIL windows."""
    if rail is None:
        return None
    window = rail.run(next(iter(captions.values())))["foreground"]
    if window is not None and window.get("title") is not None:
        window["owned"] = next((name for name, caption in captions.items() if caption == window["title"]), None)
    return window


def stop(process):
    if process.stdin is not None and not process.stdin.closed:
        process.stdin.close()
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    return process.returncode


def wait_focus(x, window, seconds=5):
    deadline = time.monotonic() + seconds
    while x.focused() != window and time.monotonic() < deadline:
        time.sleep(0.05)
    return x.focused() == window


def x11_windows(x, title, seconds=10):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        windows = x.windows()
        first = next((wid for wid, name in windows.items() if name == title + "A"), None)
        second = next((wid for wid, name in windows.items() if name == title + "B"), None)
        if first and second:
            return first, second
        time.sleep(0.1)
    raise RuntimeError(f"Owned windows {title}A/B absent: {x.windows()}")


def request_step(x, process, log, rail, captions, names, first, second, commands, key_reply, request_reply):
    """While B holds X11 focus and has a fresh key, send A's request; observe for SETTLE seconds."""
    if x.focused() != second:
        x.focus(second)
    mark = len(control.control_lines(log))
    x.key(KEY_K)
    key = control.await_line(log, key_reply, mark)
    before = dict(x11_focus=names.get(x.focused(), x.focused()), foreground=foreground(rail, captions))
    replies = [control.command(process, log, text, request_reply) for text in commands]
    time.sleep(SETTLE)
    after = x.focused()
    active = x.property(x.root, "_NET_ACTIVE_WINDOW")
    return dict(commands=commands, key=key, before=before, replies=replies, x11_focus_after=names.get(after, after),
                root_net_active_window=[names.get(value, value) for value in active],
                foreground_after=foreground(rail, captions), granted=after == first,
                log=control.control_lines(log)[mark:])


def xlib_probe(image, root, focus_surface):
    log = root / "control.log"
    results = dict(image_sha256=digest(image), steps={})
    with log.open("w") as output:
        process = subprocess.Popen([str(image)], env=owned_environment(root, "xcb"), stdin=subprocess.PIPE,
                                   stdout=output, stderr=subprocess.STDOUT)
        x = None
        try:
            observer.require(Path(f"/proc/{process.pid}/exe").resolve() == image.resolve(), "PID executable mismatch")
            results["pid"] = process.pid
            rail = observer.WslgRail(os.environ.get("WSL_DISTRO_NAME", "")) if focus_surface == "wslg-rail" else None
            x = observer.X11(process.pid, rail)
            first, second = x11_windows(x, XLIB_TITLE)
            names = {first: "A", second: "B"}
            captions = {name: f"{XLIB_TITLE}{name} ({os.environ.get('WSL_DISTRO_NAME', '')})" for name in ("A", "B")}
            results.update(xids=dict(A=first, B=second), initial_focus=names.get(x.focused(), x.focused()),
                           initial_focus_on_b=wait_focus(x, second))
            for label, commands in XLIB_REQUESTS:
                step = request_step(x, process, log, rail, captions, names, first, second, commands, "KEY:B:", "SENT:")
                results["steps"][label] = step
                print(f"XLIB_{label}:{'GRANTED' if step['granted'] else 'REFUSED'} x11_focus={step['x11_focus_after']} "
                      f"root_active={step['root_net_active_window']} replies={step['replies']}", flush=True)
            for window in (second, first):
                x.close_window(window)
            results["exit"] = process.wait(timeout=10)
            print(f"XLIB_PROBE_NATIVE_CLOSE_EXIT:{results['exit']}", flush=True)
            results["focus_activations"] = x.activations
        finally:
            if x is not None:
                x.x.XCloseDisplay(x.display)
            results["final_exit"] = stop(process)
    return results


def qt_xcb_probe(image, root, focus_surface):
    log = root / "control.log"
    results = dict(image_sha256=digest(image), steps={})
    with log.open("w") as output:
        process = subprocess.Popen([str(image), "--commands"], env=owned_environment(root, "xcb"),
                                   stdin=subprocess.PIPE, stdout=output, stderr=subprocess.STDOUT)
        x = watch = None
        try:
            observer.require(Path(f"/proc/{process.pid}/exe").resolve() == image.resolve(), "PID executable mismatch")
            results["pid"] = process.pid
            rail = observer.WslgRail(os.environ.get("WSL_DISTRO_NAME", "")) if focus_surface == "wslg-rail" else None
            x = observer.X11(process.pid, rail)
            first, second = x11_windows(x, QT_TITLE)
            names = {first: "A", second: "B"}
            captions = {name: f"{QT_TITLE}{name} ({os.environ.get('WSL_DISTRO_NAME', '')})" for name in ("A", "B")}
            results.update(platform=control.await_line(log, "PLATFORM:", 0), xids=dict(A=first, B=second),
                           initial_focus_on_b=wait_focus(x, second))
            watch = RootWatch(x)
            for label, text, reply in (("qt-activate-window", "activate A", "ACTIVATE_REQUESTED:"),
                                       ("qt-hide-show-activate-window", "remap A", "REMAP_REQUESTED:")):
                watch.drain(names)
                step = request_step(x, process, log, rail, captions, names, first, second, [text], "KEY:B:", reply)
                step["activation_requests"] = watch.drain(names)
                step["qt_focus"] = control.command(process, log, "focus", "QT_FOCUS_WINDOW:")
                results["steps"][label] = step
                print(f"QT_XCB_{label}:{'GRANTED' if step['granted'] else 'REFUSED'} x11_focus={step['x11_focus_after']} "
                      f"qt_focus={step['qt_focus']} requests={step['activation_requests']}", flush=True)
            watch.close()
            watch = None
            for window in (second, first):
                x.close_window(window)
            results["exit"] = process.wait(timeout=10)
            print(f"QT_XCB_NATIVE_CLOSE_EXIT:{results['exit']}", flush=True)
            results["focus_activations"] = x.activations
        finally:
            if watch is not None:
                watch.close()
            if x is not None:
                x.x.XCloseDisplay(x.display)
            results["final_exit"] = stop(process)
    return results


def qt_focus(process, log, expected=None, seconds=0.0):
    deadline = time.monotonic() + seconds
    while True:
        focus = control.command(process, log, "focus", "QT_FOCUS_WINDOW:")
        if expected is None or focus == expected or time.monotonic() >= deadline:
            return focus
        time.sleep(0.2)


def qt_wayland_probe(image, root, focus_surface):
    log = root / "control.log"
    results = dict(image_sha256=digest(image), steps={})
    rail = observer.WslgRail(os.environ.get("WSL_DISTRO_NAME", "")) if focus_surface == "wslg-rail" else None
    captions = {name: f"{QT_TITLE}{name} ({os.environ.get('WSL_DISTRO_NAME', '')})" for name in ("A", "B")}
    with log.open("w") as output:
        process = subprocess.Popen([str(image), "--commands"], env=owned_environment(root, "wayland"),
                                   stdin=subprocess.PIPE, stdout=output, stderr=subprocess.STDOUT)
        try:
            observer.require(Path(f"/proc/{process.pid}/exe").resolve() == image.resolve(), "PID executable mismatch")
            results["pid"] = process.pid
            results["platform"] = control.await_line(log, "PLATFORM:", 0)
            results["initial_qt_focus"] = qt_focus(process, log, "B", seconds=8)
            for label, text, reply in (("wayland-activate-window", "activate A", "ACTIVATE_REQUESTED:"),
                                       ("wayland-hide-show-activate-window", "remap A", "REMAP_REQUESTED:")):
                mark = len(control.control_lines(log))
                before = dict(qt_focus=qt_focus(process, log), foreground=foreground(rail, captions))
                replied = control.command(process, log, text, reply)
                time.sleep(SETTLE)
                after = qt_focus(process, log, "A", seconds=1.0)
                step = dict(command=text, reply=replied, before=before, qt_focus_after=after,
                            foreground_after=foreground(rail, captions), granted=after == "A",
                            log=control.control_lines(log)[mark:])
                results["steps"][label] = step
                print(f"QT_WAYLAND_{label}:{'GRANTED' if step['granted'] else 'REFUSED'} qt_focus={after} "
                      f"foreground={(step['foreground_after'] or {}).get('owned')}", flush=True)
            process.stdin.write(b"quit\n")
            process.stdin.flush()
            results["exit"] = process.wait(timeout=10)
            print(f"QT_WAYLAND_QUIT_EXIT:{results['exit']}", flush=True)
        finally:
            results["final_exit"] = stop(process)
            results["log_tail"] = control.control_lines(log)[-40:]
    return results


def x11_window_manager():
    x = observer.X11(os.getpid())
    try:
        x.x.XGetAtomName.argtypes = [c.c_void_p, c.c_ulong]
        x.x.XGetAtomName.restype = c.c_char_p
        supported = sorted(x.x.XGetAtomName(x.display, atom).decode() for atom in x.property(x.root, "_NET_SUPPORTED"))
        return dict(name=x.window_manager(), net_supported=supported,
                    advertises_net_active_window="_NET_ACTIVE_WINDOW" in supported)
    finally:
        x.x.XCloseDisplay(x.display)


def weston_log_lines(offset, pids):
    """Lines WSLg's compositor logged since `offset` that name one of the owned PIDs."""
    log = Path("/mnt/wslg/weston.log")
    if not log.exists():
        return None
    with log.open("rb") as handle:
        handle.seek(offset)
        text = handle.read().decode("utf-8", "replace")
    return [line for line in text.splitlines() if any(f"pid:{pid} " in line for pid in pids)]


def run(qt_root, work_root, output, focus_surface, skip_wayland=False):
    observer.require(not output.exists(), "Capture output already exists")
    root = make_contained_temporary_directory(prefix="activation-probe-", parent=work_root)
    qt_image, xlib_image = root / "qt-focus-control", root / "xlib-activation-probe"
    subprocess.run(["g++", "-std=c++20", "-fPIC", str(control.QT_SOURCE), f"-I{qt_root}/include",
                    f"-I{qt_root}/include/QtCore", f"-I{qt_root}/include/QtGui", f"-I{qt_root}/include/QtWidgets",
                    f"-L{qt_root}/lib", f"-Wl,-rpath,{qt_root}/lib", "-lQt6Widgets", "-lQt6Gui", "-lQt6Core", "-lxcb",
                    "-o", str(qt_image)], check=True)
    subprocess.run(["g++", "-std=c++20", str(XLIB_PROBE), "-lX11", "-o", str(xlib_image)], check=True)
    weston_log = Path("/mnt/wslg/weston.log")
    weston_offset = weston_log.stat().st_size if weston_log.exists() else 0
    wslg = Path("/mnt/wslg/versions.txt")
    environment = dict(platform=observer.native_platform(), kernel=platform.release(),
                       wslg=wslg.read_text().splitlines() if wslg.exists() else None,
                       display=os.environ.get("DISPLAY"), wayland_display=os.environ.get("WAYLAND_DISPLAY"),
                       x11_window_manager=x11_window_manager(), wayland=wayland_globals())
    print(f"ENVIRONMENT: window_manager={environment['x11_window_manager']['name']} "
          f"advertises_net_active_window={environment['x11_window_manager']['advertises_net_active_window']} "
          f"xdg_activation_v1={environment['wayland'].get('xdg_activation_v1')} "
          f"wayland_interfaces={environment['wayland'].get('interfaces')}", flush=True)
    for name in ("xlib", "qt-xcb", "qt-wayland"):
        (root / name).mkdir(mode=0o700)
    controls = dict(xlib=xlib_probe(xlib_image, root / "xlib", focus_surface),
                    qt_xcb=qt_xcb_probe(qt_image, root / "qt-xcb", focus_surface))
    if not skip_wayland:
        controls["qt_wayland"] = qt_wayland_probe(qt_image, root / "qt-wayland", focus_surface)
    pids = [entry["pid"] for entry in controls.values() if "pid" in entry]
    checkout = Path(__file__).resolve().parents[2]
    revision = subprocess.run(["git", "-C", str(checkout), "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    dirty = subprocess.run(["git", "-C", str(checkout), "status", "--porcelain", "--", "tests/scripts"],
                           capture_output=True, text=True).stdout.splitlines()
    data = dict(schema="bpr.wslg-activation-probe.v1", revision=revision, tools_dirty=dirty,
                sources={path.name: digest(path) for path in (XLIB_PROBE, control.QT_SOURCE, Path(__file__).resolve(),
                                                              HERE / "linux_settings_inventory.py",
                                                              HERE / "wslg_rail_activate.ps1")},
                compiler=subprocess.run(["g++", "--version"], capture_output=True, text=True).stdout.splitlines()[0],
                qt_root=str(qt_root), qt_sha256={name: digest(qt_root / name) for name in
                                                 (*control.QT_LIBRARIES, "lib/libQt6WaylandClient.so.6",
                                                  "plugins/platforms/libqwayland.so")},
                environment=environment, focus_surface=focus_surface, owned_root=str(root), controls=controls,
                weston_log_owned_lines=weston_log_lines(weston_offset, pids))
    output.write_text(json.dumps(data, indent=2) + "\n")
    return all(entry.get("exit") == 0 for entry in controls.values())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--qt-root", type=Path, required=True, help="Qt SDK root, for example .../6.12.0/gcc_64")
    parser.add_argument("--work-root", type=Path, required=True, help="Existing owned directory for the fresh root")
    parser.add_argument("--output", type=Path, required=True, help="New JSON capture")
    parser.add_argument("--focus-surface", choices=("x11", "wslg-rail"),
                        default="wslg-rail" if os.environ.get("WSL_DISTRO_NAME") else "x11")
    parser.add_argument("--skip-wayland", action="store_true", help="Leave out the Qt Wayland control")
    arguments = parser.parse_args()
    passed = run(arguments.qt_root.resolve(), arguments.work_root.resolve(), arguments.output,
                 arguments.focus_surface, arguments.skip_wayland)
    print(f"WSLG_ACTIVATION_PROBE_{'COMPLETE' if passed else 'INCOMPLETE'}", flush=True)
    sys.exit(0 if passed else 1)
