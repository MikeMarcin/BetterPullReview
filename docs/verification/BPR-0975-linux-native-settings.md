# BPR-0975 Linux native Settings focus and lifetime course, 2026-09-23

Station `bpr-main-linux` (`stn-5c7459ed`), checkout
`/home/mmarcin/BetterPullReview`, branch `main`, run directly on the JaegAW
WSL2/WSLg host. Mandate relayed by the Mac integrator: "implement available
unattended work, focus on closing out rfcs". Agent-driven evidence; no human
verdict, release approval or distribution qualification is implied.

**Result: AC1 passes; AC2 fails on this platform; AC3 is bound except the
full capture it needs; AC4 holds for every run.** The full default course stops
where the application asks the window manager to focus the Settings window it
reused, and WSLg's window manager refuses every application activation request.
A labelled diagnosis course that records each refusal and continues passes every
other step on the same candidate, including native Quit with exit 0.

## What changed since the recorded failure

| Layer | Then ([BPR-0593 continuation](BPR-0593-native-settings-continuation.md#linux)) | Now |
|---|---|---|
| Station | `bpr-linux`, driven from the Mac through `rfc0018_remote.py` | `bpr-main-linux` on the same host; courses run locally |
| Window manager | WSLg XWayland window manager, not identified | `Weston WM` (`_NET_SUPPORTING_WM_CHECK`); WSLg 1.0.73.2, weston `04d436c7`, the same WSLg instance (started 2026-09-17) |
| Toolkit | Qt 6.11.2 | Qt 6.11.2; Core, Gui, Widgets, XcbQpa and `libqxcb.so` byte-identical to the earlier SDK |
| Observer | X11 `XSetInputFocus`, edge click, and a RAIL bridge that Windows refused | Adds the `wslg-rail` focus surface, refusal tracking, menu-scoped commands and a non-fatal X error handler (`b651554f`) |
| Candidate | `2d3bdbdf`, one `-Wno-error=ignored-attributes` demotion | `b651554f`, no demoted diagnostic (`f863ce1b`, `233e1ebd`, `9903869f`) |

## Attribution

Each finding below is observed on this host and matches the source of the exact
component versions.

1. **The window manager returns X11 focus.** With two mapped windows, an X11
   `XSetInputFocus` on window A produces `FocusOut(B)`, `FocusIn(A)`,
   `FocusOut(A)`, `FocusIn(B)` within 0.6 s. The same sequence occurs with the
   two-QWidget control and with a toolkit-free Xlib control whose windows never
   request focus, so Qt does not cause it. Weston `04d436c7`
   `xwayland/window-manager.c` lines 2606–2631, `weston_wm_handle_focus_in`:
   "Do not let X clients change the focus behind the compositor's back. Reset
   the focus to the old one if it changed."
2. **The window manager ignores activation requests.** `_NET_ACTIVE_WINDOW` is
   advertised in `_NET_SUPPORTED` (line 3010) but
   `weston_wm_handle_client_message` (lines 2217–2246) handles only
   `_NET_WM_MOVERESIZE`, `_NET_WM_STATE`, `WL_SURFACE_ID` and `WM_CHANGE_STATE`.
   Both controls record no focus event after a `_NET_ACTIVE_WINDOW` request.
3. **The compositor follows Windows.** Making the exact `msrdc.exe`
   `RAIL_WINDOW` foreground (the taskbar's and Alt-Tab's path) moves X11 focus to
   the matching owned window, for either sibling, while the other stays mapped.
   Windows granted that request here. The earlier bridge's same request, from a
   scheduled task started over SSH, was refused. Windows' own foreground rules
   decide which process may make that request; nothing here changes them.
4. **Qt applies its own activation request.** Qt 6.11.2 selects
   `StructureNotify` on the root (`qxcbscreen.cpp` line 46), so it receives the
   `_NET_ACTIVE_WINDOW` message it sends (`qxcbwindow.cpp` lines 1488–1507) and
   treats it as focus-in (lines 1783–1784, `doFocusIn`). Keys go to Qt's focus
   window, not the X11 event window (`qxcbkeyboard.cpp` line 845). When the
   window manager refuses, Qt routes keys to the requested window while X11
   focus stays elsewhere.

The observer now watches those requests. After Ctrl-comma the product reused
the same Settings window (`6291484`) and asked for its activation (source 1);
X11 focus stayed on the workbench (`6291474`). The same XIDs appear in the old
record. After Settings closed, its `ReturnFocus` activation of the first
workbench was refused while the sibling kept X11 focus. The accessibility tree
then reported the first workbench active. The next Ctrl-comma, sent with X11
focus on the sibling, was handled by the first workbench: retiring that
workbench hid Settings, as retirement does only for the Settings owner. The
product uses Qt's ordinary `show()`, `raise()` and `activateWindow()`; no
BetterPullReview composition fault is shown.

## AC1 — bare two-window controls

`linux-focus-control-b651554f.json`, guard `passed`, 8/8 live files unchanged:

```text
BARE_QT_X11_ONLY_FOCUS:FAIL   FocusOut(B) FocusIn(A) FocusOut(A) FocusIn(B); _NET_ACTIVE_WINDOW: no event
BARE_QT_TWO_WINDOW_FOCUS:PASS A focused with B mapped ([1, 0]); B focused with A mapped
BARE_QT_POST_CLOSE_FOCUS:PASS
BARE_QT_NATIVE_CLOSE_EXIT:0
BARE_XLIB_X11_ONLY_FOCUS:FAIL the same four events; _NET_ACTIVE_WINDOW: no event
BARE_XLIB_TWO_WINDOW_FOCUS:PASS
BARE_XLIB_POST_CLOSE_FOCUS:PASS
BARE_XLIB_NATIVE_CLOSE_EXIT:0
```

Each control is an owned PID whose `/proc/PID/exe` matches the image it built,
with fresh mode-0700 XDG and TMP roots. Keys and native focus are checked on
the exact owned XID. The X11-only failure is the preserved old failure.

## AC2 — the tests-disabled application

Candidate: clean clone at `b651554ff50bdde1c5a41fd9a77f6c0e075b16c8`,
`app-release` (Release, `BUILD_TESTING=OFF`), clean rebuild 485/485, GCC 15.2
`-Werror` with no `-Wno-error`, executable SHA-256
`13c59e94f66ea52c963cfd290b5664d272473f2d1674b1742481fc2086bcb984` (an
incremental and a clean build produced the same bytes).

| Course | Result |
|---|---|
| Full default, `linux-full-course-b651554f.json` | **Fails.** View → Settings… passes; Ctrl-comma reuses the one Settings window, but focus cannot move to it: `settings_reused: True`, focus `6291474`, refused request `6291484`. Guard `command-failed`, live files unchanged; the launcher stopped and waited the application. |
| Refused-activation diagnosis, `linux-bounded-course-b651554f.json` | Passes, and is not acceptance. Two refusals recorded: Ctrl-comma's (`6291484`) and Settings close's `ReturnFocus` (`6291474` requested, `6291503` focused). Escape and Ctrl-comma reopen, New Window, sibling menu reuse, native close, sibling Ctrl-comma reopen, first-workbench retirement keeping Settings, seven showing pages and native File → Quit all pass, process exit 0. |

### Clear-window reproduction

The Mac integrator reported that `bpr-main-win` took the Windows foreground
around 16:32Z and 16:38Z. The committed runs overlap the second time. By their
run-directory and output mtimes, they ran at 16:37:42–50Z (focus control),
16:37:56–16:38:07Z (full course) and 16:38:15–51Z (diagnosis). These courses
focus windows through Windows' own foreground, so each one was repeated when
nothing else was taking it. The repeats used the same `13c59e94…b984`
executable, lease and guard, and gave the same results:

| Course, repeat | Run | Result |
|---|---|---|
| Focus control, `linux-focus-control-clear-b651554f.json` | 16:56:15–23Z | The same eight results as in AC1 above. The capture is byte-identical to the committed one once the PIDs and owned-root name are normalised. Guard `passed`, 8/8 live files unchanged. |
| Full default, `linux-full-course-clear-b651554f.json` | 16:56:23–34Z | The same failure: "Ctrl-comma did not reuse/focus Settings", `settings_reused: True`, focus `6291474`, refused `6291484`. Apart from the PID and roots, the capture differs only in the recorded `linux_settings_course.py` digest, `80f262ca…`, which is `4a16ebdc`'s script. Guard `command-failed`, live files unchanged. |
| Refused-activation diagnosis, `linux-bounded-course-clear-b651554f.json` | 17:09:26–17:10:37Z, during a Windows-side hold (17:09:25–17:10:37Z, per the integrator) | `LINUX_SETTINGS_COURSE_PASS:full-refused-activation-bounded`, observer and application exit 0. The capture is byte-identical to the committed one once the PID and root name are normalised. The XIDs and both recorded refusals did not change. Guard `passed`, 8/8 live files unchanged. |

The 16:56Z pair is the guarded rerun mentioned at the end of
[Reproducing](#reproducing). The raw logs and guard receipts are in
`/home/mmarcin/bpr0975-native/runs/` (`smoke-focus-control.*`, `smoke-full.*`,
`bounded-course-clear.log`, whose last line is its guard receipt); they are not
committed. None of the conclusions above changes, and AC2 still fails here.

## AC3 — binding and inventory

Both captures record the same revision, clean-source flag, CMake cache
(`Release`, `BUILD_TESTING=OFF`, `BPR_DISTRIBUTION_BUILD=OFF`, Qt6_DIR),
compiler `c++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`, the exact `main.cpp` compile
command (`-O3 … -Werror -Wno-missing-field-initializers`), an empty list of
first-party `-Wno-error` flags, the executable digest, the observed PID and
image, `Weston WM`, WSLg 1.0.73.2, kernel `6.18.33.2-microsoft-standard-WSL2`,
the owned root modes and the tool digests.

The diagnosis's seven pages are node-for-node identical to the
[2d3bdbdf Linux inventory](BPR-0593-native-settings-continuation/linux-native-inventory-2d3bdbdf.json)
(`[21, 26, 66, 334, 12, 15, 24]`). Against the 2d3bdbdf macOS capture, six
pages match exactly, and Accounts differs only by Mac's `Use GitHub CLI`
button. `settings_inventory_comparison_tests.py` reruns that comparison on the
new capture (16 tests). Negative controls fail: a changed control, a claimed
full pass, a `-Wno-error` flag and a missing focus revert. The full capture
AC3 names does not exist until AC2 passes.

## AC4 — isolation and state

Every run used `station_lease.py run --role station-writer --station
bpr-main-linux` and `qualification_guard.py` with allowed root
`/home/mmarcin/bpr0975-native`. The live roots were this checkout and
`/home/mmarcin/bpr-linux`; the live files were each checkout's `.mask.json`,
`.mask.lock.json`, `.remote-station.json` and implement-rfc `project.json`.
The `bpr-linux` checkout, including its existing dirty lock, was only hashed.
Each launch got a fresh mode-0700 root with separate XDG data, config, cache,
state and TMP roots; HOME was not overridden. There was no sign-in, setting
change, maintenance or provider operation. No display, security or
foreground-lock setting was changed: `ForegroundLockTimeout` was only read.
The Atspi and freedesktop typelibs were unpacked into the owned root with
`dpkg-deb -x`, not installed. All application, control and observer processes
exited or were stopped and waited.

## Remaining obligation

AC2's Ctrl-comma step, and the unrefused return of focus after Settings
closes, need a window manager that honours application activation. WSLg's
does not, by design, so no observer change can pass the full default course
here. One of the following is needed:

- a native Linux desktop whose window manager honours EWMH
  `_NET_ACTIVE_WINDOW`, such as GNOME or KDE on Ubuntu 24.04 (the declared
  [[BPR-0961]] baseline would serve), on which to run the unchanged full course;
- an owner amendment that records WSLg's refusal of application activation as
  an explicit Linux/WSLg capability difference. The diagnosis above would then
  be the evidence for every other step. This card does not make that decision.

A product change that re-maps Settings to force focus would work around the
compositor rather than fix a product defect. It is not made here.

## Reproducing

From this checkout, with the candidate built in an owned clean clone:

```sh
W=/home/mmarcin/bpr0975-native
REV=$(git -C $W/source rev-parse HEAD)
$W/guarded.sh /usr/bin/python3 $W/source/tests/scripts/linux_settings_focus_control_course.py --qt-root $W/cache/bprqt/6.11.2/6.11.2/gcc_64 --work-root $W/runs --output $W/runs/linux-focus-control.json
$W/guarded.sh /usr/bin/python3 $W/source/tests/scripts/linux_settings_course.py --build-dir $W/source/build/app-release --revision $REV --work-root $W/runs --output $W/runs/linux-full-course.json
$W/guarded.sh /usr/bin/python3 $W/source/tests/scripts/linux_settings_course.py --build-dir $W/source/build/app-release --revision $REV --work-root $W/runs --output $W/runs/linux-bounded-course.json --bound-refused-activation
python3 /home/mmarcin/BetterPullReview/tests/scripts/settings_inventory_comparison_tests.py
```

`guarded.sh` is the task-local wrapper in the owned root: it exports
`GI_TYPELIB_PATH` for the unpacked typelibs and runs the command under the
lease and the guard with the live roots and files above. The observers are
[`linux_settings_inventory.py`](../../tests/scripts/linux_settings_inventory.py),
[`linux_settings_course.py`](../../tests/scripts/linux_settings_course.py),
[`linux_settings_focus_control_course.py`](../../tests/scripts/linux_settings_focus_control_course.py),
[`linux_settings_focus_control.cpp`](../../tests/scripts/linux_settings_focus_control.cpp),
[`linux_xlib_focus_control.cpp`](../../tests/scripts/linux_xlib_focus_control.cpp)
and [`wslg_rail_activate.ps1`](../../tests/scripts/wslg_rail_activate.ps1).
Ignored logs and guard receipts are in `build/bpr0975/`.

The committed captures were produced with the tools at `b651554f`, the
candidate's own revision. The next commit changes only how the two course
scripts create their owned roots: they use `scripts/contained_temporary_support`,
as `contained_temporary_usage_contract` requires, which on Linux is the same
mode-0700 `mkdtemp`. A guarded rerun with the changed scripts gave the same
control results and the same full-course failure; it is the 16:56Z pair in
[Clear-window reproduction](#clear-window-reproduction).
