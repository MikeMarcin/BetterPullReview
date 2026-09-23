"""Close the tests-disabled application natively over a group-writable data folder.

BPR-0817, RFC-BPR-0033's owner-delegated amendment. Launches the exact
executable of a clean, tests-disabled build as an owned PID in a fresh
mode-0700 root. The root's XDG homes are made under the chosen umask, as a
desktop login under that umask makes `~/.local/share`. With `--parent-mode
0775` the folder that holds the Default profile's data root,
`$XDG_DATA_HOME/BetterPullReview`, exists before launch and is group-writable,
as an earlier install under umask 0002 leaves it. With `--parent-mode absent`
the application creates it, which is the fresh-install control.

The window's status line is read from Qt's AT-SPI announcements, not from the
status label. Qt's label exposes no AT-SPI Text interface, and its explicit
accessible name replaces its text, so the first WSLg run read an empty string
both times. Each change to the line makes the workbench frame send the whole
line as an `object:announcement` event. The course registers for those events
before it launches the application, so the application emits them from its
first window, and it keeps every line the owned frame announces.

After the observation, and whatever it found, the course closes the one owned
workbench window natively with WM_DELETE_WINDOW, which is the caption close. It
then waits for the process to exit and records the exit code. The application
is stopped only if that close does not end it.

For the group-writable parent the course passes when all of these hold:

- Home is showing, and a status line the frame announces carries the
  refusal;
- the caption close exits 0;
- the parent keeps mode 0775 and no neighbourhood is created;
- standard error names the refusal.

For the control it passes when all of these hold:

- Home is showing, and recovery has created the neighbourhood;
- the frame has announced at least one status line, so the channel is
  proven to carry the line, and no announced line carries the refusal;
- the caption close exits 0;
- the parent, the root and the neighbourhood are created 0700;
- standard error names no refusal.

Run it with the system `/usr/bin/python3`, which has PyGObject, and with
`GI_TYPELIB_PATH` naming an Atspi 2.0 typelib. Run it under the station lease
and the live-state guard. It changes no setting, signs nothing in and performs
no maintenance operation. The record names the display and its window manager,
so a run on another X server is never mistaken for the WSLg desktop.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import stat
import subprocess
import sys
import time

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from scripts.contained_temporary_support import make_contained_temporary_directory  # noqa: E402
from linux_settings_course import build_binding, digest  # noqa: E402
from linux_settings_inventory import X11, Atspi, children, native_platform, pump, walk  # noqa: E402

REFUSAL = "Disposable agent workspaces are unavailable"
STDERR_REFUSAL = "AGENT_WORKSPACE_RECOVERY_REFUSED:"
ANNOUNCEMENT = "object:announcement"
MAXIMUM_RECORDED_ANNOUNCEMENTS = 64


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def mode_of(path):
    try:
        return oct(stat.S_IMODE(os.lstat(path).st_mode))
    except FileNotFoundError:
        return None


def wait(message, predicate, seconds, detail=None):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        pump()
        if predicate():
            return
        time.sleep(0.1)
    raise RuntimeError(f"{message}: {detail()}" if detail else message)


def settle(seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        pump()
        time.sleep(0.05)


def showing(element):
    return element.get_state_set().contains(Atspi.StateType.SHOWING)


class Announcements:
    """Every AT-SPI announcement, kept with its source's process, role and name."""

    def __init__(self):
        self.events = []
        self.listener = Atspi.EventListener.new(self.receive)
        require(self.listener.register(ANNOUNCEMENT), "AT-SPI refused the announcement listener")

    def receive(self, event):
        try:
            source = event.source
            self.events.append(dict(pid=source.get_process_id() if source is not None else None,
                                    role=source.get_role_name() if source is not None else None,
                                    name=source.get_name() if source is not None else None,
                                    text="" if event.any_data is None else str(event.any_data),
                                    politeness=event.detail1, at=time.monotonic()))
        except Exception as error:  # noqa: BLE001 - a source can vanish while its event is read
            self.events.append(dict(error=f"{type(error).__name__}: {error}", at=time.monotonic()))

    def owned(self, pid):
        return [event for event in self.events if event.get("pid") == pid]

    def status_lines(self, pid, frame):
        return [event["text"] for event in self.owned(pid) if event.get("role") == "frame" and event.get("name") == frame]

    def close(self):
        self.listener.deregister(ANNOUNCEMENT)


def find_frame(pid):
    desktop = Atspi.get_desktop(0)
    found = []

    def application():
        found[:] = [app for app in children(desktop) if app is not None and app.get_process_id() == pid]
        return len(found) == 1
    wait("Application AT-SPI tree absent", application, 20)
    frames = []

    def workbench():
        frames[:] = [frame for frame in children(found[0]) if frame is not None
                     and frame.get_role() == Atspi.Role.FRAME and showing(frame)]
        return len(frames) == 1
    wait("Expected exactly one showing application frame", workbench, 20,
         lambda: [(frame.get_name(), frame.get_role_name()) for frame in children(found[0]) if frame is not None])
    return frames[0]


def home_showing(frame):
    homes = [dict(role=element.get_role_name(), showing=showing(element))
             for element, _ in walk(frame) if element.get_name() == "Home"]
    # The strip's Home button shares the name; only the page itself says Home is showing.
    page = [home for home in homes if not any(word in home["role"] for word in ("button", "tab", "menu"))]
    return any(home["showing"] for home in page), homes


def observe(pid, announcements, refused_expected, neighbourhood, observed):
    """Fills `observed` as it goes, so a failure still leaves what was seen."""
    frame = find_frame(pid)
    observed["frame"] = frame.get_name()
    lines = lambda: announcements.status_lines(pid, observed["frame"])  # noqa: E731
    if refused_expected:
        wait("The frame never announced a status line carrying the refusal",
             lambda: any(REFUSAL in line for line in lines()), 30, lines)
    else:
        # The neighbourhood exists once recovery has run, and a refusal would
        # reach the line on the next decision-lane turn. Wait for both the
        # neighbourhood and a first announced line, then give the lane time.
        wait("Startup recovery never created the neighbourhood", lambda: Path(neighbourhood).is_dir(), 30)
        wait("The frame never announced a status line", lambda: len(lines()) > 0, 30)
        settle(2.0)
    observed["home_showing"], observed["home_elements"] = home_showing(frame)


def owned_workbench(x, frame):
    owned = x.windows()
    if frame:
        candidates = [wid for wid, title in owned.items() if X11.title_matches(title, frame)]
    else:
        candidates = [wid for wid, title in owned.items() if title.startswith("BetterPullReview")]
    require(len(candidates) == 1, f"Expected one owned X11 workbench window for {frame!r}, found {owned}")
    return candidates[0], owned


def course(build_dir, revision, work_root, output, parent_mode, umask):
    require(not output.exists(), "capture output already exists")
    executable = build_dir / "src" / "app" / "better_pull_review"
    build = build_binding(build_dir, revision)
    root = make_contained_temporary_directory(prefix="refusal-", parent=work_root)
    announcements = Announcements()
    previous = os.umask(umask)
    try:
        environment = os.environ.copy()
        roots = {}
        for field in ("DATA", "CONFIG", "CACHE", "STATE"):
            location = root / field.lower()
            location.mkdir()
            environment[f"XDG_{field}_HOME"] = roots[f"XDG_{field}_HOME"] = str(location)
        (root / "tmp").mkdir(mode=0o700)
        os.chmod(root / "tmp", 0o700)
        environment["TMPDIR"] = roots["TMPDIR"] = str(root / "tmp")
        data_home = root / "data"
        parent = data_home / "BetterPullReview"
        data_root = parent / "BetterPullReview"
        neighbourhood = parent / "BetterPullReview-agent-workspaces-v1"
        if parent_mode is not None:
            parent.mkdir()
            os.chmod(parent, parent_mode)
        environment.update(QT_LINUX_ACCESSIBILITY_ALWAYS_ON="1", QT_QPA_PLATFORM="xcb")
        paths = dict(xdg_data_home=data_home, parent=parent, data_root=data_root, neighbourhood=neighbourhood)
        before = {name: mode_of(path) for name, path in paths.items()}
        wslg = Path("/mnt/wslg/versions.txt")
        here = Path(__file__).resolve()
        record = dict(
            schema="bpr.bpr0817.agent-workspace-refusal-course.v2",
            revision=revision, build=build,
            executable=dict(path=str(executable), sha256=digest(executable)),
            platform=native_platform(),
            environment=dict(kernel=platform.release(),
                             wslg=wslg.read_text().splitlines()[0] if wslg.exists() else None,
                             display=os.environ.get("DISPLAY"), wayland_display=os.environ.get("WAYLAND_DISPLAY"),
                             qt_qpa_platform="xcb", umask=oct(umask), owned_root=str(root),
                             owned_root_mode=mode_of(root), roots=roots, home_overridden=False),
            tools={path.name: digest(path) for path in (here, here.with_name("linux_settings_inventory.py"),
                                                          here.with_name("linux_settings_course.py"))},
            status_line_source="AT-SPI object:announcement events from the owned workbench frame",
            parent_before_launch=oct(parent_mode) if parent_mode is not None else "absent",
            modes_before=before)
        refused_expected = parent_mode is not None and parent_mode & 0o022 != 0
        log = root / "app.log"
        errors = root / "app.stderr"
        with log.open("w") as stdout, errors.open("w") as stderr:
            process = subprocess.Popen([str(executable)], env=environment, stdout=stdout, stderr=stderr)
    finally:
        os.umask(previous)
    observed = dict(frame=None, status_lines=[], home_showing=False, home_elements=[])
    observation_failure = close_failure = None
    x = None
    try:
        try:
            require(Path(f"/proc/{process.pid}/exe").resolve() == executable.resolve(), "PID executable mismatch")
            record["observed_process"] = dict(pid=process.pid, image=str(Path(f"/proc/{process.pid}/exe").resolve()))
            observe(process.pid, announcements, refused_expected, neighbourhood, observed)
        except Exception as error:  # noqa: BLE001 - recorded; the close below still runs
            observation_failure = f"{type(error).__name__}: {error}"
        record["observed"] = observed
        # The close runs whatever the observation found: the exit code is its own evidence.
        try:
            x = X11(process.pid)
            record["window_manager"] = x.window_manager()
            window, owned = owned_workbench(x, observed["frame"])
            record["closed_window"] = dict(xid=window, title=owned[window], owned=len(owned))
            record["modes_before_close"] = {name: mode_of(path) for name, path in paths.items()}
            started = time.monotonic()
            x.close_window(window)
            process.wait(timeout=30)
            record["exit_after_close_ms"] = round((time.monotonic() - started) * 1000)
        except Exception as error:  # noqa: BLE001 - recorded, then the owned process is stopped
            close_failure = f"{type(error).__name__}: {error}"
    finally:
        if x is not None:
            x.x.XCloseDisplay(x.display)
        if process.poll() is None:
            record["stopped_by_course"] = True
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        settle(0.5)
        announcements.close()
    # Every line the frame announced during the run, read after the close so a
    # failed observation still reports what was said.
    if observed["frame"]:
        observed["status_lines"] = announcements.status_lines(process.pid, observed["frame"])
    record["announcements"] = announcements.owned(process.pid)[:MAXIMUM_RECORDED_ANNOUNCEMENTS]
    record["exit_code"] = process.returncode
    record["closed_by_caption"] = "exit_after_close_ms" in record
    record["modes_after"] = {name: mode_of(path) for name, path in paths.items()}
    record["stderr"] = errors.read_text(errors="replace").splitlines()[-20:]
    record["stdout_bytes"] = log.stat().st_size
    after = record["modes_after"]
    lines = observed["status_lines"]
    refusal_line = any(STDERR_REFUSAL in line for line in record["stderr"])
    closed = [("Home is showing", observed["home_showing"]),
              ("caption close exits 0", record["closed_by_caption"] and process.returncode == 0)]
    if refused_expected:
        checks = [("the frame announces a status line carrying the refusal", any(REFUSAL in line for line in lines)),
                  *closed,
                  ("parent keeps its pre-existing mode", after["parent"] == oct(parent_mode)),
                  ("no neighbourhood is created", after["neighbourhood"] is None),
                  ("standard error names the refusal", refusal_line)]
    else:
        checks = [("the frame announces at least one status line", len(lines) > 0),
                  ("no announced status line carries the refusal", not any(REFUSAL in line for line in lines)),
                  *closed,
                  ("parent is created 0700", after["parent"] == "0o700"),
                  ("data root is created 0700", after["data_root"] == "0o700"),
                  ("neighbourhood is created 0700", after["neighbourhood"] == "0o700"),
                  ("standard error names no refusal", not refusal_line)]
    record["checks"] = [dict(check=name, passed=bool(passed)) for name, passed in checks]
    record["observation_failure"] = observation_failure
    record["close_failure"] = close_failure
    record["status"] = ("passed" if observation_failure is None and close_failure is None
                        and all(passed for _, passed in checks) else "failed")
    output.write_text(json.dumps(record, indent=2) + "\n")
    print(f"COURSE_ROOT:{root} APPLICATION_EXIT:{process.returncode} STATUS:{record['status']} "
          f"OBSERVATION_FAILURE:{observation_failure} CLOSE_FAILURE:{close_failure}", flush=True)
    print(f"STATUS_LINES:{json.dumps(lines)}", flush=True)
    for name, passed in checks:
        print(f"{'PASS' if passed else 'FAIL'}:{name}", flush=True)
    return record["status"] == "passed"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", type=Path, required=True, help="Configured and built tests-disabled build tree")
    parser.add_argument("--revision", required=True, help="Exact clean source commit of that build")
    parser.add_argument("--work-root", type=Path, required=True, help="Existing owned directory for the fresh course root")
    parser.add_argument("--output", type=Path, required=True, help="New JSON capture")
    parser.add_argument("--parent-mode", choices=("absent", "0775"), required=True,
                        help="Whether $XDG_DATA_HOME/BetterPullReview exists before launch, and its mode")
    parser.add_argument("--umask", default="0002", help="Umask for the course's folders and the application")
    arguments = parser.parse_args()
    mode = None if arguments.parent_mode == "absent" else int(arguments.parent_mode, 8)
    passed = course(arguments.build_dir.resolve(), arguments.revision, arguments.work_root.resolve(), arguments.output,
                    mode, int(arguments.umask, 8))
    print(f"LINUX_AGENT_WORKSPACE_REFUSAL_COURSE_{'PASS' if passed else 'FAIL'}:{arguments.parent_mode}", flush=True)
    sys.exit(0 if passed else 1)
