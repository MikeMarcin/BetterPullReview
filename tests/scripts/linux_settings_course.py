"""Run the native Linux Settings course on the tests-disabled application.

BPR-0975. Launches the exact executable of a clean, tests-disabled build as an
owned PID in a fresh mode-0700 root with separate XDG data/config/cache/state
and TMP roots; XCB and AT-SPI are enabled for that child only, and HOME is not
overridden. `linux_settings_inventory.py` then drives the full course by
default, bounded to 180 seconds. `--inventory-only` and
`--bound-refused-activation` are weaker diagnoses, never window-lifetime
acceptance. The application must exit 0 after the course's native Quit. Run it
with the system `/usr/bin/python3`, which has PyGObject.

The capture records the source revision, the build's cache settings and exact
compile flags, the executable digest, the native environment and the observed
PID and image. Run it under the station lease and the live-state guard; it
changes no setting, signs nothing in and performs no maintenance operation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
OBSERVER = HERE / "linux_settings_inventory.py"
CACHE_KEYS = ("CMAKE_BUILD_TYPE", "BUILD_TESTING", "BPR_BUILD_QT", "BPR_DISTRIBUTION_BUILD", "CMAKE_CXX_COMPILER",
              "CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_RELEASE", "CMAKE_HOME_DIRECTORY", "Qt6_DIR")


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise SystemExit(f"LINUX_SETTINGS_COURSE_REFUSED:{message}")


def build_binding(build_dir, revision):
    cache = {}
    for line in (build_dir / "CMakeCache.txt").read_text().splitlines():
        key, _, value = line.partition("=")
        name = key.partition(":")[0]
        if name in CACHE_KEYS:
            cache[name] = value
    require(cache.get("BUILD_TESTING") == "OFF", "the course runs only on a tests-disabled build")
    source = Path(cache["CMAKE_HOME_DIRECTORY"])
    head = subprocess.run(["git", "-C", str(source), "rev-parse", "HEAD"], capture_output=True, text=True, check=True).stdout.strip()
    dirty = subprocess.run(["git", "-C", str(source), "status", "--porcelain"], capture_output=True, text=True, check=True).stdout
    require(head == revision, f"source HEAD {head} is not the named revision {revision}")
    require(not dirty, f"source tree {source} is not clean")
    commands = json.loads((build_dir / "compile_commands.json").read_text())
    main = next(entry for entry in commands if entry["file"].endswith("/src/app/main.cpp"))
    flags = [flag for entry in commands if entry["file"].startswith(str(source / "src"))
             for flag in shlex.split(entry["command"]) if flag.startswith("-Wno-error")]
    compiler = subprocess.run([cache["CMAKE_CXX_COMPILER"], "--version"], capture_output=True, text=True).stdout.splitlines()[0]
    return dict(source=str(source), revision=head, clean=True, cache=cache, compiler=compiler,
                main_compile_command=main["command"], first_party_error_demotions=sorted(set(flags)))


def course(build_dir, revision, work_root, output, inventory_only, focus_surface, bound_refused_activation):
    require(not output.exists(), "capture output already exists")
    executable = build_dir / "src" / "app" / "better_pull_review"
    build = build_binding(build_dir, revision)
    root = Path(tempfile.mkdtemp(prefix="course-", dir=work_root))
    environment = os.environ.copy()
    roots = {}
    for field in ("DATA", "CONFIG", "CACHE", "STATE"):
        location = root / field.lower()
        location.mkdir(mode=0o700)
        environment[f"XDG_{field}_HOME"] = roots[f"XDG_{field}_HOME"] = str(location)
    (root / "tmp").mkdir(mode=0o700)
    environment["TMPDIR"] = roots["TMPDIR"] = str(root / "tmp")
    environment.update(QT_LINUX_ACCESSIBILITY_ALWAYS_ON="1", QT_QPA_PLATFORM="xcb")
    wslg = Path("/mnt/wslg/versions.txt")
    context = dict(
        build=build,
        executable=dict(path=str(executable), sha256=digest(executable)),
        environment=dict(kernel=platform.release(), wslg=wslg.read_text().splitlines()[0] if wslg.exists() else None,
                         display=os.environ.get("DISPLAY"), wayland_display=os.environ.get("WAYLAND_DISPLAY"),
                         qt_qpa_platform="xcb", accessibility="QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1",
                         owned_root=str(root), owned_root_mode=oct(root.stat().st_mode & 0o777),
                         owned_roots={name: dict(path=path, mode=oct(Path(path).stat().st_mode & 0o777))
                                      for name, path in roots.items()},
                         home_overridden=False),
        tools={path.name: digest(path) for path in (Path(__file__).resolve(), OBSERVER, HERE / "wslg_rail_activate.ps1")})
    (root / "context.json").write_text(json.dumps(context, indent=2) + "\n")
    observer = [sys.executable, str(OBSERVER), "", str(executable), revision, str(output),
                "--focus-surface", focus_surface, "--context", str(root / "context.json")]
    if inventory_only:
        observer.append("--inventory-only")
    if bound_refused_activation:
        observer.append("--bound-refused-activation")
    observer_exit = None
    with (root / "app.log").open("w") as log:
        process = subprocess.Popen([str(executable)], env=environment, stdout=log, stderr=subprocess.STDOUT)
        try:
            observer[2] = str(process.pid)
            try:
                observer_exit = subprocess.run(observer, env=environment, timeout=180).returncode
            except subprocess.TimeoutExpired:
                observer_exit = "timeout"
            if observer_exit == 0:
                process.wait(timeout=15)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            print(f"COURSE_ROOT:{root} OBSERVER_EXIT:{observer_exit} APPLICATION_EXIT:{process.returncode}", flush=True)
    return observer_exit == 0 and process.returncode == 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", type=Path, required=True, help="Configured and built tests-disabled build tree")
    parser.add_argument("--revision", required=True, help="Exact clean source commit of that build")
    parser.add_argument("--work-root", type=Path, required=True, help="Existing owned directory for the fresh course root")
    parser.add_argument("--output", type=Path, required=True, help="New JSON capture")
    parser.add_argument("--inventory-only", action="store_true",
                        help="Capture pages without claiming shortcut/modeless/window-lifetime acceptance")
    parser.add_argument("--focus-surface", choices=("x11", "wslg-rail"),
                        default="wslg-rail" if os.environ.get("WSL_DISTRO_NAME") else "x11")
    parser.add_argument("--bound-refused-activation", action="store_true",
                        help="Diagnosis, not acceptance: continue past a window manager that refuses the application's "
                             "own activation request")
    arguments = parser.parse_args()
    passed = course(arguments.build_dir.resolve(), arguments.revision, arguments.work_root.resolve(), arguments.output,
                    arguments.inventory_only, arguments.focus_surface, arguments.bound_refused_activation)
    kind = "inventory-only" if arguments.inventory_only else "full-refused-activation-bounded" if arguments.bound_refused_activation else "full"
    print(f"LINUX_SETTINGS_COURSE_{'PASS' if passed else 'FAIL'}:{kind}", flush=True)
    sys.exit(0 if passed else 1)
