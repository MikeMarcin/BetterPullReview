"""External AT-SPI/X11 Settings course; caller owns an isolated signed-out PID.

Only menu/window/navigation gestures are sent. Editable values, private content,
credentials, setting changes and destructive actions are deliberately absent.
Requires the native XCB surface, libXtst and the Atspi GObject typelib.

`--focus-surface wslg-rail` moves focus between owned windows the way WSLg
does natively: Windows makes the exact RAIL window foreground (BPR-0975). The
X11 path cannot do it there, because Weston's XWayland window manager returns
X11 input focus to the compositor-focused surface. Either way, keys are sent
only after X11 focus is on the exact owned target window.

The observer also watches the application's own `_NET_ACTIVE_WINDOW`
requests. Qt applies such a request to itself at once, so when the window
manager does not honour it, Qt routes keys to the requested window while X11
focus stays elsewhere. The full course fails on such a refusal.
`--bound-refused-activation` is a diagnosis, never acceptance: it records each
refusal, moves native focus through the surface and continues.

`--keys-follow-x11` (BPR-1024) bounds the same refusals, with one difference.
When the refused window is not the next target, and the target already holds
X11 focus, the observer sends the target its keys without first giving the
refused window a real focus-in. So it passes only when the application sends
keys to the window that holds X11 focus, as it does once it no longer applies
its own refused request.
"""
from __future__ import annotations

import argparse
import base64
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time

import gi
gi.require_version("Atspi", "2.0")
from gi.repository import Atspi, GLib


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def pump():
    context = GLib.MainContext.default()
    while context.pending():
        context.iteration(False)


def wait(message, predicate, detail=None):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        pump()
        if predicate():
            return
        time.sleep(0.08)
    raise RuntimeError(f"{message}: {detail()}" if detail else message)


def children(element):
    element.clear_cache()
    return [element.get_child_at_index(i) for i in range(element.get_child_count())]


def walk(element, depth=0):
    require(depth < 32, "Unbounded accessibility depth")
    yield element, depth
    for child in children(element):
        if child is not None:
            yield from walk(child, depth + 1)


def named(root, name):
    matches = [element for element, _ in walk(root) if element.get_name() == name]
    require(matches, f"Missing accessible element: {name}")
    return matches[0]


def press(element):
    action = element.get_action_iface()
    require(action is not None and action.get_n_actions() > 0,
            f"No native action on {element.get_name()}")
    require(action.do_action(0), f"Native action refused: {element.get_name()}")
    time.sleep(0.15)
    pump()


class WslgRail:
    """Ask Windows to make an owned window's exact WSLg RAIL window foreground."""

    SCRIPT = Path(__file__).resolve().with_name("wslg_rail_activate.ps1")
    POWERSHELL = Path("/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe")

    def __init__(self, distribution):
        require(distribution, "WSLg RAIL activation needs WSL_DISTRO_NAME")
        require(self.POWERSHELL.is_file() and self.SCRIPT.is_file(), "WSLg RAIL activation helper unavailable")
        self.distribution = distribution
        self.script = self.SCRIPT.read_text(encoding="utf-8")

    def run(self, caption, handle=None):
        quoted = caption.replace("'", "''")
        command = f"& {{\n{self.script}\n}} -Title '{quoted}'" + (f" -Activate -Handle {handle}" if handle else "")
        encoded = base64.b64encode(command.encode("utf-16-le")).decode("ascii")
        result = subprocess.run([str(self.POWERSHELL), "-NoLogo", "-NoProfile", "-NonInteractive", "-EncodedCommand", encoded],
                                capture_output=True, timeout=30, cwd=self.POWERSHELL.parent)
        require(result.returncode == 0, f"WSLg RAIL helper exited {result.returncode}: {result.stderr[-400:]!r}")
        return json.loads(result.stdout.decode("utf-8"))

    def activate(self, x11, window):
        # WSLg captions a RAIL window with the X11 title and the distribution.
        caption = f"{x11.property(window, '_NET_WM_NAME')} ({self.distribution})"
        candidates = self.run(caption)["candidates"]
        require(candidates, f"No exact WSLg RAIL window for owned X11 window {window}: {caption}")
        # Same-profile siblings share a caption; only X11 focus names the target.
        for handle in candidates:
            answer = self.run(caption, handle)
            require(answer["error"] is None, f"WSLg RAIL activation refused: {answer}")
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                if x11.focused() == window:
                    x11.activations.append(dict(surface="wslg-rail", window=window, caption=caption,
                                                candidates=len(candidates)))
                    return
                time.sleep(0.05)
        raise RuntimeError(f"WSLg RAIL activation did not focus owned X11 window {window}; "
                           f"focus={x11.focused()}, caption={caption}, candidates={candidates}")


class X11:
    def __init__(self, pid, rail=None):
        self.pid = pid
        self.rail = rail
        self.activations = []
        self.watching = False
        self.bound_refused_activation = False
        self.keys_follow_x11 = False
        self.claim = None
        self.refusals = []
        self.unhopped = []
        self.x = c.CDLL("libX11.so.6")
        self.xtest = c.CDLL("libXtst.so.6")
        signatures = {
            "XOpenDisplay": ([c.c_char_p], c.c_void_p),
            "XDefaultRootWindow": ([c.c_void_p], c.c_ulong),
            "XInternAtom": ([c.c_void_p, c.c_char_p, c.c_int], c.c_ulong),
            "XGetWindowProperty": ([c.c_void_p, c.c_ulong, c.c_ulong, c.c_long, c.c_long, c.c_int, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_int), c.POINTER(c.c_ulong), c.POINTER(c.c_ulong), c.POINTER(c.POINTER(c.c_ubyte))], c.c_int),
            "XQueryTree": ([c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_ulong), c.POINTER(c.POINTER(c.c_ulong)), c.POINTER(c.c_uint)], c.c_int),
            "XGetGeometry": ([c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_uint), c.POINTER(c.c_uint), c.POINTER(c.c_uint), c.POINTER(c.c_uint)], c.c_int),
            "XTranslateCoordinates": ([c.c_void_p, c.c_ulong, c.c_ulong, c.c_int, c.c_int, c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_ulong)], c.c_int),
            "XQueryPointer": ([c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_ulong), c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_uint)], c.c_int),
            "XFree": ([c.c_void_p], c.c_int),
            "XRaiseWindow": ([c.c_void_p, c.c_ulong], c.c_int),
            "XSetInputFocus": ([c.c_void_p, c.c_ulong, c.c_int, c.c_ulong], c.c_int),
            "XGetInputFocus": ([c.c_void_p, c.POINTER(c.c_ulong), c.POINTER(c.c_int)], c.c_int),
            "XKeysymToKeycode": ([c.c_void_p, c.c_ulong], c.c_ubyte),
            "XFlush": ([c.c_void_p], c.c_int),
            "XSync": ([c.c_void_p, c.c_int], c.c_int),
            "XCloseDisplay": ([c.c_void_p], c.c_int),
        }
        for name, (args, result) in signatures.items():
            getattr(self.x, name).argtypes = args
            getattr(self.x, name).restype = result
        self.xtest.XTestFakeKeyEvent.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.c_ulong]
        self.xtest.XTestFakeButtonEvent.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.c_ulong]
        self.xtest.XTestFakeMotionEvent.argtypes = [c.c_void_p, c.c_int, c.c_int, c.c_int, c.c_ulong]
        # A window can be destroyed between XQueryTree and a property read.
        # Xlib's default handler would exit the observer before its cleanup
        # stops the owned process; a failed read returns no owned properties.
        self.errors = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_void_p)(lambda display, event: 0)
        self.x.XSetErrorHandler.argtypes = [c.c_void_p]
        self.x.XSetErrorHandler.restype = c.c_void_p
        self.x.XSetErrorHandler(c.cast(self.errors, c.c_void_p))
        self.display = self.x.XOpenDisplay(None)
        require(self.display, "Native X11 display unavailable")
        self.root = self.x.XDefaultRootWindow(self.display)

    def property(self, window, name):
        atom = self.x.XInternAtom(self.display, name.encode(), 0)
        actual, format_, count, remaining = c.c_ulong(), c.c_int(), c.c_ulong(), c.c_ulong()
        data = c.POINTER(c.c_ubyte)()
        self.x.XGetWindowProperty(self.display, window, atom, 0, 4096, 0, 0,
                                 c.byref(actual), c.byref(format_), c.byref(count), c.byref(remaining), c.byref(data))
        try:
            if format_.value == 32:
                return list(c.cast(data, c.POINTER(c.c_ulong))[:count.value])
            return c.string_at(data, count.value).decode("utf-8", "replace") if data else ""
        finally:
            if data:
                self.x.XFree(data)

    def descendants(self, window, depth=0):
        if depth > 3:
            return
        root, parent, count = c.c_ulong(), c.c_ulong(), c.c_uint()
        children_ = c.POINTER(c.c_ulong)()
        if not self.x.XQueryTree(self.display, window, c.byref(root), c.byref(parent), c.byref(children_), c.byref(count)):
            return
        ids = list(children_[:count.value])
        if children_:
            self.x.XFree(children_)
        for child in ids:
            yield child
            yield from self.descendants(child, depth + 1)

    def windows(self):
        return {wid: self.property(wid, "_NET_WM_NAME") for wid in self.descendants(self.root)
                if self.property(wid, "_NET_WM_PID") == [self.pid]}

    def window(self, name):
        found = [wid for wid, title in self.windows().items() if self.title_matches(title, name)]
        require(len(found) == 1, f"Expected one X11 window: {name}, found {found}; owned={self.windows()}")
        return found[0]

    @staticmethod
    def title_matches(title, accessible_name):
        # XCB adds the application display name; AT-SPI exposes the caption only.
        return title in (accessible_name, accessible_name + " — BetterPullReview")

    def focused(self):
        window, revert = c.c_ulong(), c.c_int()
        self.x.XGetInputFocus(self.display, c.byref(window), c.byref(revert))
        return window.value

    def window_manager(self):
        check = self.property(self.root, "_NET_SUPPORTING_WM_CHECK")
        return self.property(check[0], "_NET_WM_NAME") if check else ""

    def watch_activation_requests(self, bound_refused_activation=False, keys_follow_x11=False):
        # Qt asks an EWMH window manager to activate a window with a
        # _NET_ACTIVE_WINDOW client message to the root; any client that
        # selects StructureNotify on the root also receives it.
        self.watching = True
        self.bound_refused_activation = bound_refused_activation or keys_follow_x11
        self.keys_follow_x11 = keys_follow_x11
        self.x.XSelectInput.argtypes = [c.c_void_p, c.c_ulong, c.c_long]
        self.x.XPending.argtypes = [c.c_void_p]
        self.x.XNextEvent.argtypes = [c.c_void_p, c.c_void_p]
        self.x.XSelectInput(self.display, self.root, (1 << 17) | (1 << 19))
        self.x.XSync(self.display, 0)

    def activation_requests(self):
        """Drain the root's events; return the _NET_ACTIVE_WINDOW requests among them."""
        class Message(c.Structure):
            _fields_ = [("type", c.c_int), ("serial", c.c_ulong), ("send_event", c.c_int), ("display", c.c_void_p),
                        ("window", c.c_ulong), ("message_type", c.c_ulong), ("format", c.c_int), ("data", c.c_long * 5)]
        class Event(c.Union):
            _fields_ = [("type", c.c_int), ("message", Message), ("pad", c.c_long * 24)]
        active = self.x.XInternAtom(self.display, b"_NET_ACTIVE_WINDOW", 0)
        requests = []
        while self.x.XPending(self.display):
            event = Event()
            self.x.XNextEvent(self.display, c.byref(event))
            if event.type == 33 and event.message.message_type == active:
                requests.append(dict(window=event.message.window, source=event.message.data[0]))
                self.claim = event.message.window
        return requests

    def refused_claim(self):
        """The window the application last asked to activate, when the window manager did not."""
        if not self.watching:
            return None
        self.activation_requests()
        deadline = time.monotonic() + 1
        while self.claim is not None and self.focused() != self.claim and time.monotonic() < deadline:
            time.sleep(0.05)
            self.activation_requests()
        if self.claim is not None and self.focused() == self.claim:
            self.claim = None
        return self.claim

    def focus(self, window):
        require(window in self.windows(), "Focus target is not owned")
        refused = self.refused_claim()
        if refused is not None:
            # Qt applies its own _NET_ACTIVE_WINDOW request at once, so after a
            # refusal it routes keys to `refused` while X11 focus is elsewhere.
            event = dict(requested=refused, focus=self.focused(), target=window)
            self.refusals.append(event)
            require(self.bound_refused_activation and self.rail is not None,
                    f"The window manager refused the application's own activation request: {event}")
            if self.focused() == window and refused in self.windows():
                if self.keys_follow_x11:
                    self.unhopped.append(event)  # The target's keys go where X11 focus already is.
                else:
                    self.rail.activate(self, refused)  # A real FocusIn there, then on the target below.
            self.claim = None
        if self.focused() == window:
            return
        if self.rail is not None:
            self.rail.activate(self, window)
            return
        self.x.XRaiseWindow(self.display, window)
        self.x.XSync(self.display, 0)
        time.sleep(0.2)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            self.x.XSetInputFocus(self.display, window, 2, 0)
            self.x.XSync(self.display, 0)
            time.sleep(0.1)
            if self.focused() == window:
                return
        # WSLg can keep the compositor's pointer-selected window active. A
        # real edge click is allowed only when X11 hit testing proves that
        # the pointer is over this exact owned top-level window.
        root, child = c.c_ulong(), c.c_ulong()
        left, top, unused_x, unused_y = c.c_int(), c.c_int(), c.c_int(), c.c_int()
        width, height, border, depth, mask = (c.c_uint() for _ in range(5))
        require(self.x.XGetGeometry(self.display, window, c.byref(root), c.byref(left), c.byref(top),
                                  c.byref(width), c.byref(height), c.byref(border), c.byref(depth)), "Owned geometry unavailable")
        require(self.x.XTranslateCoordinates(self.display, window, self.root, 0, 0,
                                            c.byref(left), c.byref(top), c.byref(child)), "Owned coordinates unavailable")
        for offset in ((4, height.value - 4), (4, 4), (width.value - 4, height.value - 4)):
            self.xtest.XTestFakeMotionEvent(self.display, -1, left.value + offset[0], top.value + offset[1], 0)
            self.x.XSync(self.display, 0)
            self.x.XQueryPointer(self.display, self.root, c.byref(root), c.byref(child), c.byref(unused_x),
                                 c.byref(unused_y), c.byref(unused_x), c.byref(unused_y), c.byref(mask))
            if child.value != window:
                continue
            self.xtest.XTestFakeButtonEvent(self.display, 1, 1, 0)
            self.xtest.XTestFakeButtonEvent(self.display, 1, 0, 0)
            self.x.XSync(self.display, 0)
            time.sleep(0.2)
            if self.focused() == window:
                return
        raise RuntimeError(f"Native X11 input focus refused; target={window}, focus={self.focused()}, "
                           f"WM_STATE={self.property(window, 'WM_STATE')}, owned={self.windows()}")

    def key(self, symbol, control=False):
        require(self.focused() in self.windows(), f"Refuse key outside owned process: focus={self.focused()}, owned={self.windows()}")
        symbols = [0xffe3, symbol] if control else [symbol]
        for sym in symbols:
            self.xtest.XTestFakeKeyEvent(self.display, self.x.XKeysymToKeycode(self.display, sym), 1, 0)
        for sym in reversed(symbols):
            self.xtest.XTestFakeKeyEvent(self.display, self.x.XKeysymToKeycode(self.display, sym), 0, 0)
        self.x.XFlush(self.display)
        time.sleep(0.15)

    def click(self, element):
        require(self.focused() in self.windows(), "Refuse mouse outside owned process")
        rect = element.get_component_iface().get_extents(Atspi.CoordType.SCREEN)
        require(rect.width > 0 and rect.height > 0, "Empty native click target")
        self.xtest.XTestFakeMotionEvent(self.display, -1, rect.x + rect.width // 2, rect.y + rect.height // 2, 0)
        self.xtest.XTestFakeButtonEvent(self.display, 1, 1, 0)
        self.xtest.XTestFakeButtonEvent(self.display, 1, 0, 0)
        self.x.XFlush(self.display)
        time.sleep(0.15)

    def client_message(self, destination, window, message, values, mask=0):
        class Message(c.Structure):
            _fields_ = [("type", c.c_int), ("serial", c.c_ulong), ("send_event", c.c_int), ("display", c.c_void_p), ("window", c.c_ulong), ("message_type", c.c_ulong), ("format", c.c_int), ("data", c.c_long * 5)]
        class Event(c.Union):
            _fields_ = [("message", Message), ("pad", c.c_long * 24)]
        event = Event()
        event.message = Message(33, 0, 1, self.display, window,
                                self.x.XInternAtom(self.display, message.encode(), 0), 32,
                                (c.c_long * 5)(*values))
        self.x.XSendEvent.argtypes = [c.c_void_p, c.c_ulong, c.c_int, c.c_long, c.POINTER(Event)]
        require(self.x.XSendEvent(self.display, destination, 0, mask, c.byref(event)), "Native window message refused")
        self.x.XFlush(self.display)

    def close_window(self, window):
        require(window in self.windows(), "Close target is not owned")
        self.client_message(window, window, "WM_PROTOCOLS", [self.x.XInternAtom(self.display, b"WM_DELETE_WINDOW", 0), 0, 0, 0, 0])


def native_platform():
    release = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines() if "=" in line)
    name = release.get("PRETTY_NAME", "Linux").strip('"')
    wsl = " WSL2" if "microsoft" in platform.release().lower() else ""
    return f"{name}{wsl} {platform.machine()}, native {'WSLg ' if wsl else ''}X11/XCB"


def course(pid, executable, revision, output, inventory_only=False, focus_surface="x11", context=None,
           bound_refused_activation=False, keys_follow_x11=False):
    require(Path(f"/proc/{pid}/exe").resolve() == executable.resolve(), "PID executable mismatch")
    require(not output.exists(), "Capture output already exists")
    require(not (inventory_only and (bound_refused_activation or keys_follow_x11)), "Choose one weaker course")
    require(not (bound_refused_activation and keys_follow_x11), "Choose one refused-activation course")
    observed_image = str(Path(f"/proc/{pid}/exe").resolve())
    kind = ("inventory-only" if inventory_only else "full-refused-activation-bounded" if bound_refused_activation
            else "full-refused-activation-keys-follow-x11" if keys_follow_x11 else "full")
    desktop = Atspi.get_desktop(0)
    selected = []
    def find_application():
        selected[:] = [app for app in children(desktop) if app.get_process_id() == pid]
        return len(selected) == 1
    wait("Application AT-SPI tree absent", find_application)
    app = selected[0]
    x = X11(pid, WslgRail(os.environ.get("WSL_DISTRO_NAME", "")) if focus_surface == "wslg-rail" else None)
    window_manager = x.window_manager()
    x.watch_activation_requests(bound_refused_activation, keys_follow_x11)
    checks, pages = [], []

    def record(status, failure=None):
        data = dict(schema="bpr.settings-inventory.atspi.v1", build="linux-current-qt6120-release", revision=revision,
                    executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                    platform=native_platform(), source="AT-SPI accessibility and X11/XTest native gestures",
                    course=kind, status=status, failure=failure,
                    observed=dict(pid=pid, image=observed_image, window_manager=window_manager, focus_surface=focus_surface,
                                  focus_activations=x.activations, refused_activations=x.refusals,
                                  keys_followed_x11=x.unhopped),
                    context=context or {},
                    categories=categories, native_checks=checks, pages=pages)
        output.write_text(json.dumps(data, indent=2) + "\n")

    categories = ["General", "Accounts & hosts", "Reviews & queue", "Appearance", "Notifications", "Agents & context", "Storage & privacy"]
    try:
        drive(app, x, pid, inventory_only, bound_refused_activation or keys_follow_x11, categories, checks, pages)
    except Exception as failure:
        record("failed", f"{type(failure).__name__}: {failure}")
        x.x.XCloseDisplay(x.display)
        raise
    record("passed")
    x.x.XCloseDisplay(x.display)
    print("LINUX_SETTINGS_INVENTORY_OK", kind, [len(page["nodes"]) for page in pages])


def drive(app, x, pid, inventory_only, bound_refused_activation, categories, checks, pages):
    def frames(prefix):
        return [frame for frame in children(app) if frame.get_name().startswith(prefix)
                and frame.get_state_set().contains(Atspi.StateType.SHOWING)]
    def settings():
        return frames("Settings — Default profile")
    def workbenches():
        return frames("BetterPullReview — Default profile")
    def state():
        return dict(workbenches=len(workbenches()), settings=len(settings()), focus=x.focused(), owned=x.windows(),
                    frames=[(frame.get_name(), frame.get_state_set().contains(Atspi.StateType.SHOWING)) for frame in children(app)])
    def menu(window, menu_name, command):
        x.focus(window)
        # Regular Linux menubar, not the toolbar Settings shortcut.
        active = [frame for frame in children(app) if frame.get_state_set().contains(Atspi.StateType.ACTIVE)]
        require(len(active) == 1, "Expected one active native frame for its menu")
        bar = next(element for element, _ in walk(active[0]) if element.get_role_name() == "menu bar"
                   and element.get_state_set().contains(Atspi.StateType.SHOWING))
        opened = named(bar, menu_name)
        press(opened)
        # Use a native pointer selection for the actual menu command, taken
        # from the menu just opened; a sibling window's menus carry the same
        # command names.
        x.click(named(opened, command))

    wait("Initial workbench absent", lambda: len(workbenches()) == 1)
    first = x.window(workbenches()[0].get_name())
    menu(first, "View", "Settings…")
    wait("Native Settings menu failed", lambda: len(settings()) == 1)
    shared = x.window(settings()[0].get_name())
    x.focus(shared)
    checks.append("native-menu-opens-profile-labelled-settings")
    if not inventory_only:
        x.focus(first)
        require(len(settings()) == 1, "Settings is not modeless")
        x.key(0x2c, control=True)
        try:
            wait("Ctrl-comma did not reuse/focus Settings", lambda: len(settings()) == 1 and x.focused() == shared)
            checks.append("ctrl-comma-modeless-singleton")
        except RuntimeError as failure:
            reused = len(settings()) == 1 and x.window(settings()[0].get_name()) == shared
            observed = dict(settings_reused=reused, settings_window=shared, focus=x.focused(), refused=x.refused_claim())
            # Only the exact platform refusal is bounded: the same Settings
            # window asked for its own activation and focus did not move.
            if not (bound_refused_activation and reused and observed["refused"] == shared):
                raise RuntimeError(f"{failure}: {observed}") from None
            x.focus(shared)
            checks.append("ctrl-comma-modeless-singleton-activation-refused-by-window-manager")
        x.key(0xff1b)
        wait("Escape did not close Settings", lambda: not settings())
        x.focus(first)
        x.key(0x2c, control=True)
        wait("Ctrl-comma did not reopen Settings", lambda: len(settings()) == 1)
        shared = x.window(settings()[0].get_name())
        checks.append("escape-and-shortcut-reopen")
        menu(first, "Profiles", "New Window")
        wait("New Window did not create sibling", lambda: len(workbenches()) == 2)
        second = next(wid for wid, title in x.windows().items() if x.title_matches(title, workbenches()[0].get_name()) and wid != first)
        menu(second, "View", "Settings…")
        wait("Sibling menu did not reuse Settings", lambda: len(settings()) == 1 and x.window(settings()[0].get_name()) == shared)
        x.close_window(shared)
        wait("Native close did not hide Settings", lambda: not settings())
        x.focus(second)
        x.key(0x2c, control=True)
        wait("Sibling shortcut did not reopen Settings", lambda: len(settings()) == 1)
        x.close_window(first)
        wait("Sibling retirement lost Settings", lambda: len(workbenches()) == 1 and len(settings()) == 1, state)
        checks.append("same-profile-sibling-menu-shortcut-and-retirement")
        if x.unhopped:
            # BPR-1024: a refused request did not take the sibling's keys.
            checks.append("refused-activation-keys-follow-x11")
    for index, category in enumerate(categories):
        window = settings()[0]
        x.focus(x.window(window.get_name()))
        x.click(named(named(window, "Settings pages"), category))
        pane = named(window, "Settings page")
        def correct_page():
            shown = [child for child in children(pane) if child.get_state_set().contains(Atspi.StateType.SHOWING)]
            return len(shown) == 1 and shown[0].get_name() == category
        wait(f"Selected page is not {category}", correct_page)
        nodes = []
        def capture(element, depth=0):
            state = element.get_state_set()
            if not state.contains(Atspi.StateType.VISIBLE):
                return
            require(depth < 32 and len(nodes) < 2000, "Unbounded page")
            role = element.get_role_name()
            toggle = ("On" if state.contains(Atspi.StateType.CHECKED) else "Off") if role == "check box" else ""
            nodes.append(dict(depth=depth, role=role, name=element.get_name(), enabled=state.contains(Atspi.StateType.ENABLED), state=toggle))
            for child in children(element):
                capture(child, depth + 1)
        capture(pane)
        pages.append(dict(index=index, category=category, nodes=nodes))
    checks.append("seven-categories-with-current-page-heading")
    if inventory_only:
        x.close_window(shared)
        wait("Native Settings close failed", lambda: not settings())
        x.close_window(first)
    else:
        menu(second, "File", "Quit")
    def exited():
        return not Path(f"/proc/{pid}").exists() or Path(f"/proc/{pid}/stat").read_text().split()[2] == "Z"
    def remaining():
        return dict(owned=x.windows(), focus=x.focused(),
                    frames=[(frame.get_name(), frame.get_state_set().contains(Atspi.StateType.SHOWING)) for frame in children(app)])
    wait("Native Quit did not terminate", exited, remaining)
    checks.append("native-window-close-process-terminated" if inventory_only else "native-quit-process-terminated")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    parser.add_argument("executable", type=Path)
    parser.add_argument("revision")
    parser.add_argument("output", type=Path)
    parser.add_argument("--inventory-only", action="store_true",
                        help="Capture pages without claiming shortcut/modeless/window-lifetime acceptance")
    parser.add_argument("--focus-surface", choices=("x11", "wslg-rail"), default="x11",
                        help="How focus moves between owned windows; wslg-rail uses Windows RAIL foreground")
    parser.add_argument("--context", type=Path, help="JSON build/environment binding recorded in the capture")
    parser.add_argument("--bound-refused-activation", action="store_true",
                        help="Diagnosis, not acceptance: continue past a window manager that refuses the application's "
                             "own activation request, recording the refusal and focusing through the surface")
    parser.add_argument("--keys-follow-x11", action="store_true",
                        help="BPR-1024: as --bound-refused-activation, but a target that already holds X11 focus gets "
                             "its keys without a focus-in on the refused window first")
    arguments = parser.parse_args()
    course(arguments.pid, arguments.executable, arguments.revision, arguments.output, arguments.inventory_only,
           arguments.focus_surface, json.loads(arguments.context.read_text()) if arguments.context else None,
           arguments.bound_refused_activation, arguments.keys_follow_x11)
