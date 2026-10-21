#!/usr/bin/env python3
"""Capture BetterPullReview promotional screenshots and a demo recording.

BPR-1676. The captures come from the real application window, driven on
macOS through System Events and posted mouse events. The application is the
test-enabled build serving the constructed ``promo-v1`` pull request
(``scripts/build_promo_fixture.py``), so every name, path and post on screen is
made up, and the run uses the fixture's own temporary profile rather than the
owner's.

This takes over the desktop for about a minute: it brings the window to the
front and types into it. AGENTS.md allows that only when the owner has asked
for promotional captures, so the run refuses to start without
``--confirm-desktop``. Only the application's window is ever captured, never a
screen region.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parent.parent
DEFAULT_APP = REPOSITORY / "build/dev/src/app/better_pull_review.app"
DEFAULT_OUTPUT = REPOSITORY / "docs/images/promo"
FIXTURE = "promo-v1"
OPEN_FILE = "src/render/frame_pacer.cpp"
# Finds the thread the review still and the recording land on: your blocking
# issue on a rewritten line, then two replies you have not read.
THREAD_QUERY = "wait_until"
LAUNCH_TIMEOUT_SECONDS = 90
SETTLE_SECONDS = 1.2
# Window-relative point of the title row's Files drawer toggle. The toolbar
# lays out from the left edge, so this holds at any window width.
FILES_DRAWER_TOGGLE = (169, 15)
STILL_WIDTH = 1600
MOVIE_WIDTH = 1280
MOVIE_FPS = 12
REQUIRED_TOOLS = ("osascript", "screencapture", "swiftc", "ffmpeg", "img2webp", "magick")

# Window lookup, real mouse input and window-only recording. Qt's title row
# toggles ignore the accessibility press action, so the drawer toggle needs a
# posted click. The recording captures the one window through
# ScreenCaptureKit, never a screen region, so a window, notification or
# pointer above it cannot reach a frame.
SWIFT_HELPER = r"""
import AppKit
import CoreGraphics
import Foundation
import ImageIO
import ScreenCaptureKit
import UniformTypeIdentifiers

let arguments = CommandLine.arguments
func fail(_ message: String) -> Never {
    FileHandle.standardError.write((message + "\n").data(using: .utf8)!)
    exit(1)
}

func record(windowId: Int, seconds: Double, fps: Double, width: Int, directory: URL) async throws {
    let content = try await SCShareableContent.excludingDesktopWindows(false, onScreenWindowsOnly: true)
    guard let window = content.windows.first(where: { Int($0.windowID) == windowId }) else {
        fail("window \(windowId) is not shareable")
    }
    let filter = SCContentFilter(desktopIndependentWindow: window)
    let configuration = SCStreamConfiguration()
    configuration.width = width
    configuration.height = Int((Double(width) * window.frame.height / window.frame.width).rounded())
    configuration.showsCursor = false
    configuration.ignoreShadowsSingleWindow = true
    let interval = 1.0 / fps
    let start = Date()
    var index = 0
    while Date().timeIntervalSince(start) < seconds {
        let due = start.addingTimeInterval(Double(index) * interval)
        let wait = due.timeIntervalSinceNow
        if wait > 0 { try await Task.sleep(nanoseconds: UInt64(wait * 1_000_000_000)) }
        let image = try await SCScreenshotManager.captureImage(contentFilter: filter, configuration: configuration)
        index += 1
        let url = directory.appendingPathComponent(String(format: "%05d.png", index))
        guard let destination = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil) else {
            fail("cannot write \(url.path)")
        }
        CGImageDestinationAddImage(destination, image, nil)
        CGImageDestinationFinalize(destination)
    }
    print(index)
}

guard arguments.count >= 2 else { fail("usage: helper window PID | activate PID | click X Y | move X Y | record ID SECONDS FPS WIDTH DIR") }
switch arguments[1] {
case "activate":
    guard arguments.count == 3, let pid = Int32(arguments[2]),
          let application = NSRunningApplication(processIdentifier: pid) else { fail("activate PID") }
    application.activate()
    usleep(250_000)
case "record":
    // ScreenCaptureKit needs the window server connection AppKit opens.
    _ = NSApplication.shared
    guard arguments.count == 7, let id = Int(arguments[2]), let seconds = Double(arguments[3]),
          let fps = Double(arguments[4]), let width = Int(arguments[5]) else { fail("record ID SECONDS FPS WIDTH DIR") }
    do {
        try await record(windowId: id, seconds: seconds, fps: fps, width: width, directory: URL(fileURLWithPath: arguments[6]))
    } catch {
        fail("recording failed: \(error)")
    }
case "window":
    guard arguments.count == 3, let pid = Int(arguments[2]) else { fail("window PID") }
    let options: CGWindowListOption = [.optionOnScreenOnly, .excludeDesktopElements]
    let windows = (CGWindowListCopyWindowInfo(options, kCGNullWindowID) as? [[String: Any]]) ?? []
    var best: (id: Int, x: Int, y: Int, w: Int, h: Int)? = nil
    for window in windows {
        guard (window[kCGWindowOwnerPID as String] as? Int) == pid,
              (window[kCGWindowLayer as String] as? Int) == 0,
              let id = window[kCGWindowNumber as String] as? Int,
              let bounds = window[kCGWindowBounds as String] as? [String: Any] else { continue }
        let x = (bounds["X"] as? Int) ?? 0, y = (bounds["Y"] as? Int) ?? 0
        let w = (bounds["Width"] as? Int) ?? 0, h = (bounds["Height"] as? Int) ?? 0
        if best == nil || w * h > best!.w * best!.h { best = (id, x, y, w, h) }
    }
    guard let found = best else { fail("no on-screen window for pid \(pid)") }
    print("\(found.id) \(found.x) \(found.y) \(found.w) \(found.h)")
case "click", "move":
    guard arguments.count == 4, let x = Double(arguments[2]), let y = Double(arguments[3]) else { fail("click X Y") }
    let point = CGPoint(x: x, y: y)
    CGEvent(mouseEventSource: nil, mouseType: .mouseMoved, mouseCursorPosition: point, mouseButton: .left)?.post(tap: .cghidEventTap)
    if arguments[1] == "click" {
        usleep(80_000)
        CGEvent(mouseEventSource: nil, mouseType: .leftMouseDown, mouseCursorPosition: point, mouseButton: .left)?.post(tap: .cghidEventTap)
        usleep(40_000)
        CGEvent(mouseEventSource: nil, mouseType: .leftMouseUp, mouseCursorPosition: point, mouseButton: .left)?.post(tap: .cghidEventTap)
    }
default:
    fail("unknown command \(arguments[1])")
}
"""


@dataclass(frozen=True)
class Window:
    id: int
    x: int
    y: int
    width: int
    height: int


class Session:
    """One launched fixture application and the means to drive it."""

    def __init__(self, process: subprocess.Popen[bytes], helper: Path) -> None:
        self.process = process
        self.helper = helper

    @property
    def pid(self) -> int:
        return self.process.pid

    def window(self) -> Window:
        output = run([str(self.helper), "window", str(self.pid)]).split()
        return Window(*(int(value) for value in output))

    def system_events(self, command: str) -> None:
        run(["osascript", "-e",
             f'tell application "System Events" to tell (first process whose unix id is {self.pid}) to {command}'])

    def activate(self) -> None:
        """Brings the application to the front, so keys reach it and nothing else."""
        run([str(self.helper), "activate", str(self.pid)])

    def keystroke(self, text: str, command: bool = False, activate: bool = True) -> None:
        if activate:
            self.activate()
        modifier = " using command down" if command else ""
        escaped = text.replace("\\", "\\\\").replace('"', '\\"')
        self.system_events(f'keystroke "{escaped}"{modifier}')

    def key_code(self, code: int) -> None:
        self.activate()
        self.system_events(f"key code {code}")

    def menu(self, menu: str, item: str) -> None:
        self.activate()
        self.system_events(f'click menu item "{item}" of menu "{menu}" of menu bar 1')

    def modality(self, number: int) -> None:
        """Overview, Review, Discuss and Threads are Command-1 to Command-4."""
        self.keystroke(str(number), command=True)
        time.sleep(SETTLE_SECONDS)

    def click_in_window(self, x: int, y: int) -> None:
        self.activate()
        window = self.window()
        run([str(self.helper), "click", str(window.x + x), str(window.y + y)])
        time.sleep(SETTLE_SECONDS)

    def files_drawer_shown(self) -> bool:
        """Reads the Files toggle's checked state; its tooltip names the drawer."""
        script = (
            f'tell application "System Events" to tell (first process whose unix id is {self.pid})\n'
            '  repeat with toggle in (every checkbox of window 1)\n'
            '    if (description of toggle) contains "file drawer" then return value of toggle\n'
            '  end repeat\n'
            'end tell\n'
            'return "missing"')
        value = run(["osascript", "-e", script])
        if value == "missing":
            raise RuntimeError("the Files drawer toggle was not found")
        return value == "1"

    def ensure_files_drawer_hidden(self) -> None:
        """The comparison gets the drawer's width. Review can reopen the drawer,
        so this checks its state rather than toggling blindly."""
        if self.files_drawer_shown():
            self.click_in_window(*FILES_DRAWER_TOGGLE)

    def park_pointer(self) -> None:
        """Moves the pointer off the window so no hover text reaches a capture."""
        window = self.window()
        run([str(self.helper), "move", str(window.x + window.width + 40), str(window.y + window.height // 2)])
        time.sleep(0.6)

    def show_review_thread(self) -> None:
        """Finds the thread in Threads; Return opens it beside its code in Review."""
        self.modality(4)
        self.keystroke("f", command=True)
        time.sleep(0.4)
        self.keystroke(THREAD_QUERY)
        time.sleep(0.8)
        self.key_code(36)
        time.sleep(SETTLE_SECONDS)
        self.ensure_files_drawer_hidden()

    def capture_still(self, path: Path) -> None:
        self.park_pointer()
        run(["screencapture", "-x", "-o", "-l", str(self.window().id), str(path)])


def run(command: list[str]) -> str:
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"{command[0]} failed: {result.stderr.strip() or result.stdout.strip()}")
    return result.stdout.strip()


def build_helper(work: Path) -> Path:
    source = work / "promo_helper.swift"
    source.write_text(SWIFT_HELPER, encoding="utf-8")
    helper = work / "promo_helper"
    run(["swiftc", "-O", "-o", str(helper), str(source)])
    return helper


def launch(app: Path, helper: Path, log: Path) -> Session:
    executable = app / "Contents/MacOS/better_pull_review"
    if not executable.is_file():
        raise RuntimeError(f"no test-enabled application at {app}; build it with cmake --build --preset dev --target app_better_pull_review")
    with log.open("wb") as stream:
        process = subprocess.Popen(
            [str(executable), "--github-fixture", FIXTURE, "--attended-launch",
             "--attended-open-file", OPEN_FILE, "--attended-read-state"],
            stdout=stream, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + LAUNCH_TIMEOUT_SECONDS
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"the application exited during launch; see {log}")
        if f"ATTENDED_FILE_SHOWN:{OPEN_FILE}" in log.read_text(errors="replace"):
            time.sleep(2.0)
            return Session(process, helper)
        time.sleep(0.5)
    process.terminate()
    raise RuntimeError(f"the fixture did not show {OPEN_FILE} within {LAUNCH_TIMEOUT_SECONDS} s; see {log}")


def capture_stills(session: Session, work: Path) -> dict[str, Path]:
    stills: dict[str, Path] = {}
    for name, number in (("overview", 1), ("discuss", 3), ("threads", 4)):
        session.modality(number)
        stills[name] = work / f"{name}.png"
        session.capture_still(stills[name])
    session.show_review_thread()
    stills["review"] = work / "review.png"
    session.capture_still(stills["review"])
    # Each modality looks different. Two equal captures mean a key went to
    # another application and the window never changed.
    contents = {path.read_bytes() for path in stills.values()}
    if len(contents) != len(stills):
        raise RuntimeError("two captures are identical; the window did not follow the keys")
    return stills


def record_demo(session: Session, work: Path) -> tuple[Path, float]:
    """Threads, a thread opened beside its code, two changes, then Discuss."""
    # Start from the whole list: the stills left the thread search filled in.
    session.modality(4)
    session.keystroke("f", command=True)
    time.sleep(0.3)
    session.keystroke("a", command=True)
    session.key_code(51)
    time.sleep(SETTLE_SECONDS)
    session.park_pointer()
    frames = work / "frames"
    frames.mkdir(exist_ok=True)
    duration = 20
    recorder = subprocess.Popen(
        [str(session.helper), "record", str(session.window().id), str(duration),
         str(MOVIE_FPS), str(MOVIE_WIDTH), str(frames)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    time.sleep(2.5)
    session.keystroke("f", command=True)
    time.sleep(0.5)
    session.activate()
    for character in THREAD_QUERY:
        session.keystroke(character, activate=False)
        time.sleep(0.06)
    time.sleep(1.0)
    session.key_code(36)
    time.sleep(0.8)
    session.ensure_files_drawer_hidden()
    time.sleep(2.2)
    session.menu("View", "Next change")
    time.sleep(2.0)
    session.menu("View", "Previous change")
    time.sleep(2.0)
    session.modality(3)
    time.sleep(2.5)
    output, errors = recorder.communicate(timeout=duration + 60)
    if recorder.returncode != 0 or not any(frames.glob("*.png")):
        raise RuntimeError(f"window recording failed: {errors.strip()}")
    # Captures that run late are not dropped, so play back at the rate that
    # was achieved rather than the one requested.
    return frames, int(output.strip()) / duration


def export_stills(stills: dict[str, Path], output: Path) -> list[Path]:
    written = []
    for name, source in stills.items():
        png = output / f"betterpullreview-{name}.png"
        webp = output / f"betterpullreview-{name}.webp"
        run(["magick", str(source), "-resize", f"{STILL_WIDTH}x", "-strip", str(png)])
        run(["magick", str(source), "-resize", f"{STILL_WIDTH}x", "-strip", "-quality", "86", str(webp)])
        written += [png, webp]
    return written


def export_demo(frames: Path, fps: float, output: Path) -> list[Path]:
    frame_files = sorted(str(path) for path in frames.glob("*.png"))
    mp4 = output / "betterpullreview-demo.mp4"
    run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", f"{fps:.3f}", "-i", str(frames / "%05d.png"),
         "-vf", "scale=trunc(iw/2)*2:trunc(ih/2)*2", "-c:v", "libx264", "-preset", "slow", "-crf", "20",
         "-pix_fmt", "yuv420p", "-movflags", "+faststart", "-an", str(mp4)])
    webp = output / "betterpullreview-demo.webp"
    run(["img2webp", "-loop", "0", "-lossy", "-q", "72", "-d", str(round(1000 / fps)), *frame_files, "-o", str(webp)])
    return [mp4, webp]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--app", type=Path, default=DEFAULT_APP, help="test-enabled better_pull_review.app")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="directory for the exported assets")
    parser.add_argument("--site-root", type=Path, default=os.environ.get("BETTERPULLREVIEW_SITE_ROOT"),
                        help="betterpullreview.com checkout; assets are copied to its static/images")
    parser.add_argument("--skip-demo", action="store_true", help="capture the stills only")
    parser.add_argument("--keep-work", action="store_true", help="keep the raw captures and frames")
    parser.add_argument("--confirm-desktop", action="store_true",
                        help="the owner asked for captures and knows the desktop is taken over for about a minute")
    arguments = parser.parse_args()

    if not arguments.confirm_desktop:
        parser.error("this drives the desktop for about a minute; pass --confirm-desktop once the owner has asked for it")
    if sys.platform != "darwin":
        parser.error("promo capture drives the macOS window and runs on macOS only")
    missing = [tool for tool in REQUIRED_TOOLS if shutil.which(tool) is None]
    if missing:
        parser.error(f"missing tools: {', '.join(missing)}")
    if subprocess.run(["pgrep", "-f", f"better_pull_review --github-fixture {FIXTURE}"], capture_output=True).returncode == 0:
        parser.error("a fixture application is already running; quit it first")

    work = Path(tempfile.mkdtemp(prefix="bpr-promo-"))
    arguments.output.mkdir(parents=True, exist_ok=True)
    session = None
    try:
        helper = build_helper(work)
        session = launch(arguments.app.resolve(), helper, work / "application.log")
        session.activate()
        # Hide the files drawer: the comparison then has the width to show
        # whole lines in both panes beside the comments drawer.
        session.ensure_files_drawer_hidden()
        stills = capture_stills(session, work)
        written = export_stills(stills, arguments.output)
        if not arguments.skip_demo:
            written += export_demo(*record_demo(session, work), arguments.output)
    finally:
        if session is not None:
            session.process.terminate()
            try:
                session.process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                session.process.kill()
        if not arguments.keep_work:
            shutil.rmtree(work, ignore_errors=True)

    if arguments.site_root:
        images = Path(arguments.site_root) / "static/images"
        images.mkdir(parents=True, exist_ok=True)
        for path in written:
            shutil.copy2(path, images / path.name)
    for path in written:
        print(path.relative_to(REPOSITORY) if path.is_relative_to(REPOSITORY) else path)
    if arguments.keep_work:
        print(f"raw captures: {work}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
