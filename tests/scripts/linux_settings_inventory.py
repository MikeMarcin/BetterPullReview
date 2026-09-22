"""External AT-SPI/X11 Settings course; caller owns an isolated signed-out PID.

Only menu/window/navigation gestures are sent. Editable values, private content,
credentials, setting changes and destructive actions are deliberately absent.
Requires the native XCB surface, libXtst and the Atspi GObject typelib.
"""
from __future__ import annotations

import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
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


def wait(message, predicate):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        pump()
        if predicate():
            return
        time.sleep(0.08)
    raise RuntimeError(message)


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


class X11:
    def __init__(self, pid):
        self.pid = pid
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

    def focus(self, window):
        require(window in self.windows(), "Focus target is not owned")
        if self.focused() == window:
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


def course(pid, executable, revision, output, inventory_only=False):
    require(Path(f"/proc/{pid}/exe").resolve() == executable.resolve(), "PID executable mismatch")
    require(not output.exists(), "Capture output already exists")
    desktop = Atspi.get_desktop(0)
    selected = []
    def find_application():
        selected[:] = [app for app in children(desktop) if app.get_process_id() == pid]
        return len(selected) == 1
    wait("Application AT-SPI tree absent", find_application)
    app = selected[0]
    x = X11(pid)
    def frames(prefix):
        return [frame for frame in children(app) if frame.get_name().startswith(prefix)
                and frame.get_state_set().contains(Atspi.StateType.SHOWING)]
    def settings():
        return frames("Settings — Default profile")
    def workbenches():
        return frames("BetterPullReview — Default profile")
    def menu(window, menu_name, command):
        x.focus(window)
        # Regular Linux menubar, not the toolbar Settings shortcut.
        active = [frame for frame in children(app) if frame.get_state_set().contains(Atspi.StateType.ACTIVE)]
        require(len(active) == 1, "Expected one active native frame for its menu")
        bar = next(element for element, _ in walk(active[0]) if element.get_role_name() == "menu bar"
                   and element.get_state_set().contains(Atspi.StateType.SHOWING))
        press(named(bar, menu_name))
        # Use a native pointer selection for the actual menu command.
        x.click(named(app, command))

    wait("Initial workbench absent", lambda: len(workbenches()) == 1)
    first = x.window(workbenches()[0].get_name())
    menu(first, "View", "Settings…")
    wait("Native Settings menu failed", lambda: len(settings()) == 1)
    shared = x.window(settings()[0].get_name())
    x.focus(shared)
    checks = ["native-menu-opens-profile-labelled-settings"]
    if not inventory_only:
        x.focus(first)
        require(len(settings()) == 1, "Settings is not modeless")
        x.key(0x2c, control=True)
        wait("Ctrl-comma did not reuse/focus Settings", lambda: len(settings()) == 1 and x.focused() == shared)
        checks.append("ctrl-comma-modeless-singleton")
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
        wait("Sibling retirement lost Settings", lambda: len(workbenches()) == 1 and len(settings()) == 1)
        checks.append("same-profile-sibling-menu-shortcut-and-retirement")
    categories = ["General", "Accounts & hosts", "Reviews & queue", "Appearance", "Notifications", "Agents & context", "Storage & privacy"]
    pages = []
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
    wait("Native Quit did not terminate", lambda: not Path(f"/proc/{pid}").exists() or Path(f"/proc/{pid}/stat").read_text().split()[2] == "Z")
    checks.append("native-window-close-process-terminated" if inventory_only else "native-quit-process-terminated")
    data = dict(schema="bpr.settings-inventory.atspi.v1", build="linux-current-qt6112-release", revision=revision,
                executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                platform="Ubuntu 26.04 WSL2 x86_64, native WSLg X11/XCB", source="AT-SPI accessibility and X11/XTest native gestures",
                course="inventory-only" if inventory_only else "full",
                categories=categories, native_checks=checks, pages=pages)
    output.write_text(json.dumps(data, indent=2) + "\n")
    x.x.XCloseDisplay(x.display)
    print("LINUX_SETTINGS_INVENTORY_OK", [len(page["nodes"]) for page in pages])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    parser.add_argument("executable", type=Path)
    parser.add_argument("revision")
    parser.add_argument("output", type=Path)
    parser.add_argument("--inventory-only", action="store_true",
                        help="Capture pages without claiming shortcut/modeless/window-lifetime acceptance")
    arguments = parser.parse_args()
    course(arguments.pid, arguments.executable, arguments.revision, arguments.output, arguments.inventory_only)
