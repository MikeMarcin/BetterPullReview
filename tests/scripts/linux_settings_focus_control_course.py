"""Bare two-window native focus controls for the Linux Settings observer.

BPR-0975. Builds `linux_settings_focus_control.cpp` (two QWidgets) against the
given Qt SDK and `linux_xlib_focus_control.cpp` (two plain Xlib windows that
never request focus) in a fresh owned root. Each runs as an owned PID with
fresh mode-0700 XDG and TMP roots; no BetterPullReview code, profile, setting,
provider or storage is used. For each control:

1. X11 only: the original observer request, XSetInputFocus on window A while
   B has focus, with the X11 focus events it produced, then an EWMH
   `_NET_ACTIVE_WINDOW` request. Recorded, not required: on WSLg, Weston's
   XWayland window manager returns focus to the window the compositor focused.
2. Native focus: A while B stays mapped, then B while A stays mapped, through
   the selected focus surface. Each requires X11 focus on the exact owned XID.
3. Native close of B, focus of A, native close of A and process exit 0.

The exit status is 0 only when steps 2 and 3 pass for the Qt control. The
same owned-PID, exact-image and target-focus checks as the Settings course
apply; the observer refuses a window its PID does not own.

`--refused-activation` (BPR-1024) adds a step to each Qt control before step 1.
While B holds X11 focus, the control's window A calls `activateWindow()`, on
a command sent to its standard input. The step records the `_NET_ACTIVE_WINDOW`
requests seen on the root, where X11 focus is afterwards and which window Qt
treats as focused. It then sends one key where X11 focus is, and records the
window Qt delivered it to. The Qt control runs twice: as it is, and with
`--consume-activation-echo`, which drops Qt's own `_NET_ACTIVE_WINDOW` message
before Qt handles it. The step is an observation and does not change the exit
status. A capture with it uses schema v3.
"""
from __future__ import annotations

import argparse
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import linux_settings_inventory as observer  # noqa: E402
from scripts.contained_temporary_support import make_contained_temporary_directory  # noqa: E402

HERE = Path(__file__).resolve().parent
QT_SOURCE = HERE / "linux_settings_focus_control.cpp"
XLIB_SOURCE = HERE / "linux_xlib_focus_control.cpp"
QT_LIBRARIES = ("lib/libQt6Core.so.6", "lib/libQt6Gui.so.6", "lib/libQt6Widgets.so.6",
                "lib/libQt6XcbQpa.so.6", "plugins/platforms/libqxcb.so")


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class FocusEvent(c.Structure):
    _fields_ = [("type", c.c_int), ("serial", c.c_ulong), ("send_event", c.c_int), ("display", c.c_void_p),
                ("window", c.c_ulong), ("mode", c.c_int), ("detail", c.c_int)]


class Event(c.Union):
    _fields_ = [("type", c.c_int), ("focus", FocusEvent), ("pad", c.c_long * 24)]


def x11_only_focus(x, first, second):
    """Request focus of `first` through X11 alone and keep the focus events."""
    x.x.XSelectInput.argtypes = [c.c_void_p, c.c_ulong, c.c_long]
    x.x.XPending.argtypes = [c.c_void_p]
    x.x.XNextEvent.argtypes = [c.c_void_p, c.POINTER(Event)]
    names = {first: "A", second: "B"}

    def settle():
        events = []
        deadline = time.monotonic() + 0.6
        while time.monotonic() < deadline:
            while x.x.XPending(x.display):
                event = Event()
                x.x.XNextEvent(x.display, c.byref(event))
                if event.type in (9, 10):
                    kind = "FocusIn" if event.type == 9 else "FocusOut"
                    events.append(f"{kind}({names.get(event.focus.window, event.focus.window)})")
            time.sleep(0.005)
        return dict(events=events, focus=names.get(x.focused(), x.focused()))

    for window in (first, second):
        x.x.XSelectInput(x.display, window, 1 << 21)  # FocusChangeMask
    x.x.XSync(x.display, 0)
    x.x.XSetInputFocus(x.display, first, 2, 0)  # RevertToParent, CurrentTime
    x.x.XSync(x.display, 0)
    set_input_focus = settle()
    # EWMH activation as a pager would ask for it (source indication 2).
    x.client_message(x.root, first, "_NET_ACTIVE_WINDOW", [2, 0, second, 0, 0], mask=(1 << 19) | (1 << 20))
    net_active_window = settle()
    for window in (first, second):
        x.x.XSelectInput(x.display, window, 0)
    x.x.XSync(x.display, 0)
    return dict(passed=x.focused() == first, set_input_focus=set_input_focus, net_active_window=net_active_window)


def native_focus(x, target, sibling):
    """Focus `target` through the configured surface while `sibling` stays mapped."""
    try:
        x.focus(target)
    except RuntimeError as failure:
        return False, str(failure)
    owned = x.windows()
    state = x.property(sibling, "WM_STATE")
    passed = x.focused() == target and sibling in owned and state[:1] == [1]
    return passed, f"focus={x.focused()} target={target} sibling={sibling} sibling_owned={sibling in owned} sibling_WM_STATE={state}"


def control_lines(log_path):
    return log_path.read_text(encoding="utf-8", errors="replace").splitlines()


def await_line(log_path, prefix, after):
    """The rest of the first line after index `after` that starts with `prefix`, or None."""
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        line = next((line for line in control_lines(log_path)[after:] if line.startswith(prefix)), None)
        if line is not None:
            return line[len(prefix):]
        time.sleep(0.05)
    return None


def command(process, log_path, text, reply):
    """Send one command to the control and return its reply line, if it gives one."""
    mark = len(control_lines(log_path))
    process.stdin.write(f"{text}\n".encode())
    process.stdin.flush()
    return await_line(log_path, reply, mark)


def refused_activation(x, process, log_path, first, second):
    """While B holds X11 focus, A asks to be activated; one key then goes where X11 focus is."""
    names = {first: "A", second: "B"}
    start = len(control_lines(log_path))
    x.watch_activation_requests()
    x.activation_requests()
    before = x.focused()
    requested = command(process, log_path, "activate A", "ACTIVATE_REQUESTED:")
    time.sleep(1.0)
    requests = [dict(window=names.get(request["window"], request["window"]), source=request["source"])
                for request in x.activation_requests()]
    after = x.focused()
    qt_focus = command(process, log_path, "focus", "QT_FOCUS_WINDOW:")
    key = None
    if after in names:
        mark = len(control_lines(log_path))
        x.key(0x6b)  # "k", to the window that holds X11 focus
        key = await_line(log_path, "KEY:", mark)
    x.x.XSelectInput(x.display, x.root, 0)
    x.x.XSync(x.display, 0)
    x.watching, x.claim = False, None
    received = key.split(":", 1)[0] if key else None
    refused = before == second and after == second
    return dict(xids=dict(A=first, B=second), x11_focus_before=names.get(before, before), requested=requested,
                activation_requests=requests, x11_focus_after=names.get(after, after), refused=refused,
                qt_focus=qt_focus, key_sent_to=names.get(after, after), key_received_by=received,
                diverged=refused and received == "A", control_log=control_lines(log_path)[start:])


def run_control(label, image, title, root, focus_surface, arguments=(), refused=False):
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "xcb"
    for key in ("DATA", "CONFIG", "CACHE", "STATE"):
        (root / key.lower()).mkdir(mode=0o700)
        environment[f"XDG_{key}_HOME"] = str(root / key.lower())
    (root / "tmp").mkdir(mode=0o700)
    environment["TMPDIR"] = str(root / "tmp")
    results = dict(image_sha256=digest(image))
    x = None
    log_path = root / "control.log"
    with log_path.open("w") as log:
        process = subprocess.Popen([str(image), *(("--commands",) if refused else ()), *arguments], env=environment,
                                   stdin=subprocess.PIPE if refused else subprocess.DEVNULL, stdout=log,
                                   stderr=subprocess.STDOUT)
        try:
            observer.require(Path(f"/proc/{process.pid}/exe").resolve() == image.resolve(), "PID executable mismatch")
            results["pid"] = process.pid
            rail = observer.WslgRail(os.environ.get("WSL_DISTRO_NAME", "")) if focus_surface == "wslg-rail" else None
            x = observer.X11(process.pid, rail)
            deadline = time.monotonic() + 10
            while len(x.windows()) < 2 and time.monotonic() < deadline:
                time.sleep(0.1)
            windows = x.windows()
            first = next(wid for wid, name in windows.items() if name == title + "A")
            second = next(wid for wid, name in windows.items() if name == title + "B")
            deadline = time.monotonic() + 5
            while x.focused() != second and time.monotonic() < deadline:
                time.sleep(0.05)
            results["initial_focus"] = "B" if x.focused() == second else x.focused()
            if refused:
                step = results["refused_activation"] = refused_activation(x, process, log_path, first, second)
                outcome = "DIVERGED" if step["diverged"] else "FOLLOWED_X11" if step["refused"] else "NOT_REFUSED"
                print(f"{label}_REFUSED_ACTIVATION:{outcome} requests={step['activation_requests']} "
                      f"x11_focus={step['x11_focus_after']} qt_focus={step['qt_focus']} "
                      f"key_sent_to={step['key_sent_to']} key_received_by={step['key_received_by']}", flush=True)
                if x.focused() != second:  # A window manager that honoured it: start step 1 from B again.
                    x.focus(second)
            results["x11_only"] = x11_only_focus(x, first, second)
            print(f"{label}_X11_ONLY_FOCUS:{'PASS' if results['x11_only']['passed'] else 'FAIL'} "
                  f"{results['x11_only']['set_input_focus']} {results['x11_only']['net_active_window']}", flush=True)
            if x.focused() != second:  # A window manager that honoured X11: start from B again.
                x.focus(second)
            first_ok, first_detail = native_focus(x, first, second)
            second_ok, second_detail = native_focus(x, second, first)
            results["two_window"] = dict(passed=first_ok and second_ok, first=first_detail, second=second_detail)
            print(f"{label}_TWO_WINDOW_FOCUS:{'PASS' if first_ok and second_ok else 'FAIL'} "
                  f"A: {first_detail}; B: {second_detail}", flush=True)
            x.close_window(second)
            deadline = time.monotonic() + 5
            while second in x.windows() and time.monotonic() < deadline:
                time.sleep(0.05)
            try:
                x.focus(first)
                post_close, detail = x.focused() == first, f"focus={x.focused()} target={first}"
            except RuntimeError as failure:
                post_close, detail = False, str(failure)
            results["post_close"] = dict(passed=post_close, detail=detail)
            print(f"{label}_POST_CLOSE_FOCUS:{'PASS' if post_close else 'FAIL'} {detail}", flush=True)
            x.close_window(first)
            results["exit"] = process.wait(timeout=10)
            print(f"{label}_NATIVE_CLOSE_EXIT:{results['exit']}", flush=True)
            results["focus_activations"] = x.activations
        finally:
            if process.stdin is not None:
                process.stdin.close()
            if x is not None:
                x.x.XCloseDisplay(x.display)
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            results["final_exit"] = process.returncode
            print(f"{label}_CONTROL_ROOT:{root} FINAL_EXIT:{process.returncode}", flush=True)
    return results


def window_manager():
    x = observer.X11(os.getpid())
    try:
        x.x.XGetAtomName.argtypes = [c.c_void_p, c.c_ulong]
        x.x.XGetAtomName.restype = c.c_char_p
        supported = sorted(x.x.XGetAtomName(x.display, atom).decode() for atom in x.property(x.root, "_NET_SUPPORTED"))
        return dict(name=x.window_manager(), net_supported=supported)
    finally:
        x.x.XCloseDisplay(x.display)


def run(qt_root, work_root, output, focus_surface, refused=False):
    observer.require(not output.exists(), "Capture output already exists")
    root = make_contained_temporary_directory(prefix="focus-control-", parent=work_root)
    qt_image, xlib_image = root / "qt-focus-control", root / "xlib-focus-control"
    subprocess.run(["g++", "-std=c++20", "-fPIC", str(QT_SOURCE), f"-I{qt_root}/include", f"-I{qt_root}/include/QtCore",
                    f"-I{qt_root}/include/QtGui", f"-I{qt_root}/include/QtWidgets", f"-L{qt_root}/lib",
                    f"-Wl,-rpath,{qt_root}/lib", "-lQt6Widgets", "-lQt6Gui", "-lQt6Core", "-lxcb", "-o", str(qt_image)],
                   check=True)
    subprocess.run(["g++", "-std=c++20", str(XLIB_SOURCE), "-lX11", "-o", str(xlib_image)], check=True)
    names = ("qt", "qt-consume", "xlib") if refused else ("qt", "xlib")
    for name in names:
        (root / name).mkdir(mode=0o700)
    title = "BPR0018 native focus control "
    controls = dict(qt=run_control("BARE_QT", qt_image, title, root / "qt", focus_surface, refused=refused))
    if refused:
        controls["qt_consume"] = run_control("BARE_QT_CONSUME", qt_image, title, root / "qt-consume", focus_surface,
                                             ("--consume-activation-echo",), refused=True)
    controls["xlib"] = run_control("BARE_XLIB", xlib_image, "BPR0975 Xlib focus control ", root / "xlib", focus_surface)
    qt = controls["qt"]
    wslg = Path("/mnt/wslg/versions.txt")
    data = dict(schema="bpr.settings-focus-control.v3" if refused else "bpr.settings-focus-control.v2",
                sources=dict(qt=digest(QT_SOURCE), xlib=digest(XLIB_SOURCE)),
                compiler=subprocess.run(["g++", "--version"], capture_output=True, text=True).stdout.splitlines()[0],
                qt_root=str(qt_root), qt_sha256={name: digest(qt_root / name) for name in QT_LIBRARIES},
                platform=observer.native_platform(), kernel=platform.release(),
                wslg=wslg.read_text().splitlines()[0] if wslg.exists() else None,
                display=os.environ.get("DISPLAY"), window_manager=window_manager(), focus_surface=focus_surface,
                owned_root=str(root), controls=controls)
    output.write_text(json.dumps(data, indent=2) + "\n")
    return qt["two_window"]["passed"] and qt["post_close"]["passed"] and qt["exit"] == 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--qt-root", type=Path, required=True, help="Qt SDK root, for example .../6.12.0/gcc_64")
    parser.add_argument("--work-root", type=Path, required=True, help="Existing owned directory for the fresh control root")
    parser.add_argument("--output", type=Path, required=True, help="New JSON capture")
    parser.add_argument("--focus-surface", choices=("x11", "wslg-rail"),
                        default="wslg-rail" if os.environ.get("WSL_DISTRO_NAME") else "x11")
    parser.add_argument("--refused-activation", action="store_true",
                        help="BPR-1024: ask A to activate while B holds X11 focus, then see where a key sent to B goes")
    arguments = parser.parse_args()
    passed = run(arguments.qt_root.resolve(), arguments.work_root.resolve(), arguments.output, arguments.focus_surface,
                 arguments.refused_activation)
    sys.exit(0 if passed else 1)
