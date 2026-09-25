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

**2026-09-25, owner amendment.** The owner accepted WSLg's refusal of
application activation as the current behaviour, with no product change
(decision `dce91e77`, recorded on [BPR-0975](../issues/60-done/BPR-0975-complete-linux-native-settings-focus-and-lifetime-course.md)). AC2 and AC3 are
amended to the refused-activation diagnosis below, and BPR-0975 is Done. The
full default course still fails here, and that is not called a pass. The
recheck, carrying the upstream change below, is
[BPR-1122](../issues/50-blocked/BPR-1122-wslg-refuses-application-window-activation-so-a-reused-settings-window-cant-take-focus.md), Blocked on `upstream-wslg`. No upstream report was
prepared (decision `c5d0e284`).

## Why WSLg refuses activation, 2026-09-24

The owner rejected both options offered below (decision `35aa7cb0`) and
directed that the refusal be diagnosed and fixed. **Result: WSLg's compositor
has no activation path an application can use.** It focuses a window that is
already mapped only when Windows activates that window, or on a click. Every
X11 and Wayland activation request is ignored, whatever its source indication
or timestamp. The one thing an application can do that moves focus is to unmap
the window and map it again, which is a workaround rather than an activation
request. No station setting changes this. Switching the application to Wayland
would not help.

### The source

Weston is microsoft/weston-mirror `04d436c7`, the build in WSLg 1.0.73.2 (its
log reports `weston 9.0.0`, `9.0.0-213-g04d436c7`, shell `rdprail-shell.so`).
Qt is `v6.11.2`. Both were read from read-only downloads; neither is vendored.

1. **The X11 window manager drops the request.**
   `weston_wm_handle_client_message` (`xwayland/window-manager.c` 2217–2247)
   handles `_NET_WM_MOVERESIZE`, `_NET_WM_STATE`, `WL_SURFACE_ID` and
   `WM_CHANGE_STATE` only. `_NET_ACTIVE_WINDOW` is interned (2801) and
   advertised in `_NET_SUPPORTED` (3010), but nothing handles it. Nothing
   checks a source indication or timestamp either: `_NET_WM_USER_TIME` is
   interned (2776) and never read. The window manager's interface to the shell
   (`xwayland/xwayland-internal-interface.h` 36–69) has no activation entry,
   so the window manager has no way to pass such a request on.
2. **Focus set by a client is reverted.** `weston_wm_handle_focus_in`
   (2606–2631) sets X11 focus back to the compositor's window whenever a
   client moves it.
3. **The shell activates in four places only** (`rdprail-shell/shell.c`):
   - `map` (2587–2622), when a surface is mapped;
   - pointer and touch presses (3969–3995);
   - `shell_backend_request_window_activate` (3998–4029), when Windows
     activates the RAIL window (`libweston/backend-rdp/rdprail.c` 281–320);
   - `focus_state_surface_destroy` (919–960), which gives focus to the top
     remaining window when the focused surface goes away.

   `activate()` (3883–3925) then tells the RDP backend. The backend sends the
   new active window to Windows (`rdp_rail_sync_window_zorder`, `rdprail.c`
   3087–3156).
4. **Wayland has no activation protocol here.** `xdg_activation_v1` does not
   appear anywhere in weston-mirror. The only shell-specific protocol,
   `weston_rdprail_shell`, offers `set_focus_proxy` alone. Qt's
   `QWaylandXdgSurface::requestActivate` (`qwaylandxdgshell.cpp` 579–617)
   returns without doing anything when that global is absent.
5. **Qt's request is already correct.** `QXcbWindow::requestActivateWindow`
   (`qxcbwindow.cpp` 1462–1513) updates `_NET_WM_USER_TIME`, then sends
   source 1 with the triggering event's time and the current active window, as
   EWMH asks. A different timestamp or source would change nothing.
6. **No setting and no newer build.** The compositor's RDP and shell settings
   (`compositor/main.c` 2831–2898, `rdprail-shell/shell.c` 728–846) have no
   activation or focus-stealing option. The weston-mirror `working` branch
   (`7fbf6931`, 2026-09-11) is four commits ahead of `04d436c7`; none touches
   activation. Upstream Weston `main` (`fce7c48a`, 2026-09-24) also ignores
   `_NET_ACTIVE_WINDOW` and has no xdg-activation. microsoft/wslg#1022 (Emacs
   `other-frame` cannot switch frames, open since 2023-03) reports the same
   defect; the only workarounds posted there activate the window from Windows.

### The probe

[`wslg_activation_probe_course.py`](../../tests/scripts/wslg_activation_probe_course.py)
at `d24223f2`, in a desktop hold that bpr-main-win acknowledged. The run went
from 2026-09-24T20:43:58Z to 20:44:34Z, after a 120 s settle starting at
20:41:58Z. The guard passed, with 8/8 live files unchanged, and every control
exited 0. Capture:
[`wslg-activation-probe-d24223f2.json`](BPR-0975-linux-native-settings/wslg-activation-probe-d24223f2.json).
In every step window B holds X11 focus and the Windows foreground, and has
just received a real key. Window A then asks for activation. The result is
read one second later.

The compositor advertises these Wayland globals: `weston_rdprail_shell`,
`weston_screenshooter`, `wl_compositor`, `wl_data_device_manager`,
`wl_output`, `wl_seat`, `wl_shell`, `wl_shm`, `wl_subcompositor`,
`wp_presentation`, `wp_viewporter`, `xdg_wm_base`, `zwp_input_method_v1`,
`zwp_input_panel_v1`, `zwp_input_timestamps_manager_v1`,
`zwp_pointer_constraints_v1`, `zwp_relative_pointer_manager_v1`,
`zwp_text_input_manager_v1`, `zxdg_output_manager_v1` and `zxdg_shell_v6`.
`xdg_activation_v1` is not among them. The X11 window manager, `Weston WM`,
advertises `_NET_ACTIVE_WINDOW`.

| Control | Request for A | X11 or Qt focus after | Windows foreground after |
|---|---|---|---|
| Xlib | `_NET_ACTIVE_WINDOW`, source 1, CurrentTime | B, no focus event | B |
| Xlib | source 1, the key's time, also set as A's `_NET_WM_USER_TIME` | B, no focus event | B |
| Xlib | source 1, a fresh server time | B, no focus event | B |
| Xlib | source 2 (pager), the key's time | B, no focus event | B |
| Xlib | source 2, a fresh server time | B, no focus event | B |
| Xlib | source 0, CurrentTime | B, no focus event | B |
| Xlib | `XSetInputFocus`, a fresh server time | B: FocusOut(B), FocusIn(A), FocusOut(A), FocusIn(B) | B |
| Xlib | `XRaiseWindow`, then source 1 with the key's time | B, no focus event | B |
| Xlib | unmap, then map | **A**: FocusOut(B), FocusIn(A) | **A** |
| Qt on xcb | `activateWindow()`: Qt sent source 1, time 581275477, current B | X11 B; Qt's focus window A | B |
| Qt on xcb | `hide()`, `show()`, `raise()`, `activateWindow()` | **A** (X11 and Qt) | **A** |
| Qt on Wayland | `activateWindow()` | B | B |
| Qt on Wayland | `hide()`, `show()`, `raise()`, `activateWindow()` | **A** | **A** |

On X11 the root `_NET_ACTIVE_WINDOW` property stayed B after every refused
request and became A after both remaps, so the window manager itself reports
the compositor's choice. Windows followed the compositor each time it activated a
window: the RAIL helper only read the foreground and activated nothing (no
RAIL activation in either control). So Windows' foreground rules are not what
blocks activation; the compositor never activates. The bare Qt control has no
[[BPR-1024]] host. Its first step shows Qt taking its own refused request as
granted, and in the next step a key sent to B reached A. That is BPR-1024's
divergence, reproduced.

### The upstream change that would fix it

The fix belongs in microsoft/weston-mirror, and in upstream Weston, which has
the same X11 window manager. Nothing has been filed.

1. `xwayland/window-manager.c`: in `weston_wm_handle_client_message`, add a
   branch for `wm->atom.net_active_window`. It hands the window's shell
   surface, the source indication (`data32[0]`) and the timestamp
   (`data32[1]`) to the shell. That makes the advertisement in
   `_NET_SUPPORTED` true.
2. `xwayland/xwayland-internal-interface.h`: add a `request_activate(shsurf,
   source, timestamp)` entry. `libweston-desktop/xwayland.c` (the table at
   462–480) implements it with a new optional `activation_requested` callback
   in `struct weston_desktop_api`
   (`include/libweston-desktop/libweston-desktop.h`), the way `set_minimized`
   reaches `minimized_requested` (`libweston-desktop.c` 239–244).
3. `rdprail-shell/shell.c`: register `.activation_requested` in
   `shell_desktop_api` (3403–3420). Implement it as `activate(shell,
   shsurf->view, seat, WESTON_ACTIVATE_FLAG_CONFIGURE)` for each seat, behind
   a focus-stealing rule. The rule grants a pager request (source 2), and an
   application request (source 1) when the requesting client owns the focused
   surface or its timestamp is no older than that surface's last user input.
   `activate()` already informs Windows, and the probe shows Windows following.
4. For Wayland clients: add an `xdg_activation_v1` global to the RAIL shell
   that issues a token for a recent input serial and calls the same
   `activate()`. Qt 6.11.2 uses it with no application change.

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

Superseded again on 2026-09-25 by the owner amendment at the top of this note.

Superseded on 2026-09-24. The owner rejected both options below, and
[Why WSLg refuses activation](#why-wslg-refuses-activation-2026-09-24) records
the diagnosis. The decision now open is on the card: a WSLg-only remap in the
product, or waiting for the upstream compositor change. The earlier text
follows unchanged.

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
$W/guarded.sh /usr/bin/python3 $W/source/tests/scripts/wslg_activation_probe_course.py --qt-root $W/cache/bprqt/6.11.2/6.11.2/gcc_64 --work-root $W/runs --output $W/runs/wslg-activation-probe.json
```

`guarded.sh` is the task-local wrapper in the owned root: it exports
`GI_TYPELIB_PATH` for the unpacked typelibs and runs the command under the
lease and the guard with the live roots and files above. The observers are
[`linux_settings_inventory.py`](../../tests/scripts/linux_settings_inventory.py),
[`linux_settings_course.py`](../../tests/scripts/linux_settings_course.py),
[`linux_settings_focus_control_course.py`](../../tests/scripts/linux_settings_focus_control_course.py),
[`linux_settings_focus_control.cpp`](../../tests/scripts/linux_settings_focus_control.cpp),
[`linux_xlib_focus_control.cpp`](../../tests/scripts/linux_xlib_focus_control.cpp),
[`linux_xlib_activation_probe.cpp`](../../tests/scripts/linux_xlib_activation_probe.cpp),
[`wslg_activation_probe_course.py`](../../tests/scripts/wslg_activation_probe_course.py)
and [`wslg_rail_activate.ps1`](../../tests/scripts/wslg_rail_activate.ps1).
Ignored logs and guard receipts are in `build/bpr0975/`.

The committed captures were produced with the tools at `b651554f`, the
candidate's own revision. The next commit changes only how the two course
scripts create their owned roots: they use `scripts/contained_temporary_support`,
as `contained_temporary_usage_contract` requires, which on Linux is the same
mode-0700 `mkdtemp`. A guarded rerun with the changed scripts gave the same
control results and the same full-course failure; it is the 16:56Z pair in
[Clear-window reproduction](#clear-window-reproduction).
