"""Close the tests-disabled application natively over a group-writable data folder.

BPR-0817, RFC-BPR-0033's owner-delegated amendment. Launches the exact
executable of a clean, tests-disabled build as an owned PID in a fresh
mode-0700 root. The root's XDG homes are made under the chosen umask, as a
desktop login under that umask makes `~/.local/share`. With `--parent-mode
0775` the folder that holds the Default profile's data root,
`$XDG_DATA_HOME/BetterPullReview`, exists before launch and is group-writable,
as an earlier install under umask 0002 leaves it. With `--parent-mode absent`
the application creates it, which is the fresh-install control.

The course walks the application's AT-SPI tree at Home and reads the
"Workbench status" label. It then closes the one workbench window natively with
WM_DELETE_WINDOW, which is the caption close, and waits for the process to
exit. It never signals the application unless the course has already failed.
It records the exit code, the status text, standard error, and the modes of
the data folders and the agent workspace neighbourhood.

For the group-writable parent the course passes when all of these hold:

- the status bar says the refusal at Home;
- the parent keeps mode 0775 and no neighbourhood is created;
- standard error names the refusal;
- the process exits 0.

For the control it passes when all of these hold:

- the parent and the root are created 0700;
- the neighbourhood is created 0700;
- the status bar does not carry the refusal;
- the process exits 0.

Run it with the system `/usr/bin/python3`, which has PyGObject, and with
`GI_TYPELIB_PATH` naming an Atspi 2.0 typelib. Run it under the station lease
and the live-state guard. It changes no setting, signs nothing in and performs
no maintenance operation.
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


def text_of(element):
    text = element.get_text_iface()
    return text.get_text(0, text.get_character_count()) if text is not None else ""


def showing(element):
    return element.get_state_set().contains(Atspi.StateType.SHOWING)


def observe(pid, refused_expected, neighbourhood):
    desktop = Atspi.get_desktop(0)
    found = []

    def application():
        found[:] = [app for app in children(desktop) if app is not None and app.get_process_id() == pid]
        return len(found) == 1
    wait("Application AT-SPI tree absent", application, 20)
    app = found[0]
    frames = []

    def workbench():
        frames[:] = [frame for frame in children(app) if frame is not None and frame.get_role() == Atspi.Role.FRAME
                     and showing(frame)]
        return len(frames) == 1
    wait("Expected exactly one showing application frame", workbench, 20,
         lambda: [(frame.get_name(), frame.get_role_name()) for frame in children(app) if frame is not None])
    frame = frames[0]
    labels = []

    def status():
        labels[:] = [element for element, _ in walk(frame) if element.get_name() == "Workbench status"]
        return len(labels) == 1
    wait("Workbench status label absent", status, 20)
    label = labels[0]
    if refused_expected:
        wait("The status bar never said the refusal", lambda: REFUSAL in text_of(label), 30, lambda: text_of(label))
    else:
        # The neighbourhood exists once recovery has run, so a refusal would
        # already have reached the status bar. Then allow the decision lane a
        # moment, and require the sentence to be absent.
        wait("Startup recovery never created the neighbourhood", lambda: Path(neighbourhood).is_dir(), 30)
        time.sleep(1.0)
        pump()
        require(REFUSAL not in text_of(label), f"The status bar carries a refusal on a fresh install: {text_of(label)}")
    homes = [dict(role=element.get_role_name(), showing=showing(element))
             for element, _ in walk(frame) if element.get_name() == "Home"]
    # The strip's Home button shares the name; only the page itself says Home is showing.
    home_page = [home for home in homes if not any(word in home["role"] for word in ("button", "tab", "menu"))]
    require(any(home["showing"] for home in home_page), f"Home is not showing: {homes}")
    return dict(frame=frame.get_name(), status_text=text_of(label), status_showing=showing(label), home_elements=homes)


def course(build_dir, revision, work_root, output, parent_mode, umask):
    require(not output.exists(), "capture output already exists")
    executable = build_dir / "src" / "app" / "better_pull_review"
    build = build_binding(build_dir, revision)
    root = make_contained_temporary_directory(prefix="refusal-", parent=work_root)
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
        record = dict(
            schema="bpr.bpr0817.agent-workspace-refusal-course.v1",
            revision=revision, build=build,
            executable=dict(path=str(executable), sha256=digest(executable)),
            platform=native_platform(),
            environment=dict(kernel=platform.release(),
                             wslg=wslg.read_text().splitlines()[0] if wslg.exists() else None,
                             display=os.environ.get("DISPLAY"), wayland_display=os.environ.get("WAYLAND_DISPLAY"),
                             qt_qpa_platform="xcb", umask=oct(umask), owned_root=str(root),
                             owned_root_mode=mode_of(root), roots=roots, home_overridden=False),
            tools={path.name: digest(path) for path in (Path(__file__).resolve(),
                                                          Path(__file__).resolve().with_name("linux_settings_inventory.py"),
                                                          Path(__file__).resolve().with_name("linux_settings_course.py"))},
            parent_before_launch=oct(parent_mode) if parent_mode is not None else "absent",
            modes_before=before)
        refused_expected = parent_mode is not None and parent_mode & 0o022 != 0
        log = root / "app.log"
        errors = root / "app.stderr"
        observed = None
        failure = None
        with log.open("w") as stdout, errors.open("w") as stderr:
            process = subprocess.Popen([str(executable)], env=environment, stdout=stdout, stderr=stderr)
    finally:
        os.umask(previous)
    x = None
    try:
        require(Path(f"/proc/{process.pid}/exe").resolve() == executable.resolve(), "PID executable mismatch")
        record["observed_process"] = dict(pid=process.pid, image=str(Path(f"/proc/{process.pid}/exe").resolve()))
        observed = observe(process.pid, refused_expected, neighbourhood)
        record["observed"] = observed
        x = X11(process.pid)
        owned = x.windows()
        workbench = [wid for wid, title in owned.items() if X11.title_matches(title, observed["frame"])]
        require(len(workbench) == 1, f"Expected one owned X11 window titled {observed['frame']!r}, found {owned}")
        record["closed_window"] = dict(xid=workbench[0], title=owned[workbench[0]], owned=len(owned))
        record["modes_before_close"] = {name: mode_of(path) for name, path in paths.items()}
        started = time.monotonic()
        x.close_window(workbench[0])
        process.wait(timeout=30)
        record["exit_after_close_ms"] = round((time.monotonic() - started) * 1000)
    except Exception as error:  # noqa: BLE001 - recorded, then the owned process is stopped
        failure = f"{type(error).__name__}: {error}"
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
    record["exit_code"] = process.returncode
    record["modes_after"] = {name: mode_of(path) for name, path in paths.items()}
    record["stderr"] = errors.read_text(errors="replace").splitlines()[-20:]
    record["stdout_bytes"] = log.stat().st_size
    checks = []
    if failure is None:
        after = record["modes_after"]
        refusal_line = any(STDERR_REFUSAL in line for line in record["stderr"])
        if refused_expected:
            checks = [("status bar says the refusal at Home", REFUSAL in observed["status_text"]),
                      ("parent keeps its pre-existing mode", after["parent"] == oct(parent_mode)),
                      ("no neighbourhood is created", after["neighbourhood"] is None),
                      ("standard error names the refusal", refusal_line),
                      ("caption close exits 0", process.returncode == 0)]
        else:
            checks = [("status bar carries no refusal", REFUSAL not in observed["status_text"]),
                      ("parent is created 0700", after["parent"] == "0o700"),
                      ("data root is created 0700", after["data_root"] == "0o700"),
                      ("neighbourhood is created 0700", after["neighbourhood"] == "0o700"),
                      ("standard error names no refusal", not refusal_line),
                      ("caption close exits 0", process.returncode == 0)]
    record["checks"] = [dict(check=name, passed=bool(passed)) for name, passed in checks]
    record["failure"] = failure
    record["status"] = "passed" if failure is None and all(passed for _, passed in checks) else "failed"
    output.write_text(json.dumps(record, indent=2) + "\n")
    print(f"COURSE_ROOT:{root} APPLICATION_EXIT:{process.returncode} STATUS:{record['status']} FAILURE:{failure}", flush=True)
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
