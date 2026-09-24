---
schema: "implement-rfc.card.v1"
id: "BPR-1024"
title: "On WSLg, keys go to the window whose activation the window manager refused (Settings close with sibling workbenches)"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"found_by":"BPR-0975 Linux native Settings course on bpr-main-linux","found_at":"b651554f","observed_on":"WSLg 1.0.73.2, Weston WM 04d436c7, XWayland, Qt 6.11.2 xcb","platform_scope":"Observed on WSLg only; whether a desktop window manager that declines activation shows it is unverified","related_cards":["BPR-0975"],"evidence":"docs/verification/BPR-0975-linux-native-settings.md","reservation":"docs/issues/_reservations/BPR-1024--BPR-1026--f65f9f9f0854.json"}
---

# BPR-1024: On WSLg, keys go to the window whose activation the window manager refused (Settings close with sibling workbenches)

## Defect

On WSLg, when the application asks for a window's activation and the window
manager refuses, Qt still treats that window as focused. Keys then go to it,
while X11 focus stays on the window the compositor shows as focused.

[[BPR-0975]]'s course shows it after Settings closes with two same-profile
workbenches open. Settings' `closeEvent` asks to activate the window it
returns focus to (`src/adapters/qt/src/settings_window.cpp:244`). The window
manager refuses. The sibling keeps X11 focus, but the next Ctrl-comma, sent
with X11 focus on the sibling, was handled by the first workbench: retiring
that workbench hid Settings, as retirement does only for Settings' owner
([attribution](../../verification/BPR-0975-linux-native-settings.md#attribution)).
The same refusal happens when Ctrl-comma reuses Settings
(`settings_window.cpp:192`): Qt then sends keys to Settings while the
workbench keeps X11 focus.

The chain, with the lines re-read for this card from qtbase `v6.11.2` and
weston-mirror `04d436c7d9a0`:

1. `QXcbWindow::requestActivateWindow` (`qxcbwindow.cpp:1462`) sends
   `_NET_ACTIVE_WINDOW` to the root whenever the window manager advertises it
   (lines 1488–1507). If it does not, Qt calls `xcb_set_input_focus` instead.
2. WSLg's Weston WM advertises it (`xwayland/window-manager.c:3010`), but
   `weston_wm_handle_client_message` (2217–2246) handles only
   `_NET_WM_MOVERESIZE`, `_NET_WM_STATE`, `WL_SURFACE_ID` and
   `WM_CHANGE_STATE`, so X11 focus does not move. Its focus-in handler
   (2606–2631) also resets any focus an X client sets itself.
3. Qt selects `StructureNotify` on the root (`qxcbscreen.cpp:46`), so it
   receives its own message. It handles it as focus-in on the requested window
   (`qxcbwindow.cpp:1783–1784`). `doFocusIn` (`:845`) makes that window the
   connection's focus window.
4. `QXcbKeyboard::handleKeyEvent` sends each key to the connection's focus
   window rather than the X11 window the event came from
   (`qxcbkeyboard.cpp:845`).

Both [[BPR-0975]] diagnosis captures record the refusal:
`observed.refused_activations` holds `6291474` requested while `6291503` kept
focus, in the committed capture and in the clear-window repeat
([clear-window reproduction](../../verification/BPR-0975-linux-native-settings.md#clear-window-reproduction)).
The course now gets past it by activating the refused window through Windows
first, which gives Qt a real focus-in and focus-out
(`tests/scripts/linux_settings_inventory.py`, `focus`). That is how the
diagnosis passes, and it hides the misrouting.

**Not shown yet:**

- The keys in the course are injected X11 key events. A physical keyboard
  reaches X11 the same way, but no human observation is recorded.
- A desktop window manager that declines some activation requests, for
  example under focus-stealing prevention, would reach the same Qt path. Qt's
  handling of its own message does not depend on the window manager, but this
  is not verified.
- The product uses Qt's ordinary `show()`, `raise()` and `activateWindow()`.
  No BetterPullReview composition fault is shown.

## Scope

- Reproduce without BetterPullReview code first. The bare Qt control
  (`tests/scripts/linux_settings_focus_control.cpp`, driven by
  `linux_settings_focus_control_course.py`) activates its second window only
  at startup and records no key delivery. Let it request activation of a
  window on demand, and report which window receives a key.
- Decide the layer from that evidence. One option is a correction in
  `src/adapters/qt` that keeps Qt's focus window equal to X11 focus after a
  refused request, limited to the xcb platform. The other is an owner-recorded
  Linux/WSLg capability difference with an upstream Qt report. Prepare the
  report with the bare reproduction; filing it outside the repository is the
  owner's decision.
- Do not force focus through the compositor or Windows, and do not re-map
  windows, from the product. [[BPR-0975]] records why that works around the
  compositor instead of fixing a defect.
- This card does not unblock [[BPR-0975]]. Its Ctrl-comma step needs an
  activation that WSLg never grants.

## Acceptance

- AC1 On WSLg, a bare Qt control with no BetterPullReview code shows the
  divergence. While window B holds X11 focus, window A's `activateWindow()`
  is refused, and a key delivered to B reaches A. The capture binds the Qt SDK
  digests, owned PIDs, images and XIDs, as [[BPR-0975]]'s control does.
- AC2 The chosen disposition is recorded with its evidence: either a product
  correction, or an owner-recorded capability difference with a prepared
  upstream report.
- AC3 With a correction: on WSLg, after Settings closes while a sibling holds
  X11 focus, a key sent to the sibling reaches the sibling. Ctrl-comma then
  opens or reuses Settings for the sibling, not the first workbench. The
  refused-activation diagnosis still passes every step it passes now, the
  focused Settings suites still pass, and macOS and Windows behaviour is
  unchanged: the change applies only on xcb, or is shown to do nothing
  elsewhere.
- AC4 [[BPR-0975]]'s isolation holds for every native run: fresh mode-0700
  XDG and TMP roots, the station lease and the live-state guard. No display,
  security or foreground-lock setting changes.

## Evidence

- [BPR-0975 Linux native Settings](../../verification/BPR-0975-linux-native-settings.md),
  attribution items 2 and 4, and the clear-window reproduction.
- `docs/verification/BPR-0975-linux-native-settings/linux-bounded-course-b651554f.json`
  and `linux-bounded-course-clear-b651554f.json`,
  `observed.refused_activations`.
- Upstream sources: qtbase `v6.11.2`
  `src/plugins/platforms/xcb/{qxcbwindow,qxcbscreen,qxcbkeyboard}.cpp`;
  microsoft/weston-mirror `04d436c7d9a0cd55fa64b6612c7fa678d6fcd077`
  `xwayland/window-manager.c`. Neither is vendored here.

## Disposition: the application keeps Qt's own activation request from Qt on xcb (bpr-main-linux, 2026-09-23)

Station `bpr-main-linux` (JaegAW: WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2,
`build/dev` Debug with `-Werror`), claimed at `ffab27a5`. Everything below
ran offscreen or without Qt at all. Nothing was shown on the desktop. The
logs are station records under `build/logs/bpr-main-linux-20260924/impl/`
(`bpr-1024-*`) and are not committed.

### The chain, read again at 6.11.2

The Qt sources were read from the `v6.11.2` tag of qt/qtbase
(`qxcbwindow.cpp` SHA-256 `896b18d6…0a7e`). The line numbers above hold.
Three details decide the layer:

- `requestActivateWindow` sends `_NET_ACTIVE_WINDOW` to the root with
  `XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT`
  (`qxcbwindow.cpp:1505-1507`). EWMH asks for SubstructureNotify. Qt selects
  StructureNotify on the root (`qxcbscreen.cpp:46`), so it receives only the
  `_NET_ACTIVE_WINDOW` messages sent with Qt's own mask, which in practice
  are its own requests. Its handler (`:1783-1784`) is therefore Qt taking its
  own request as granted.
- When a window manager grants the request, the X server sends FocusIn, and
  `handleFocusInEvent` (`:2287`) calls the same `doFocusIn`. So the handler
  adds nothing where the request is granted, and is wrong where it is refused.
- `QXcbConnection::handleXcbEvent` offers every event to the application's
  native event filters before it dispatches it (`qxcbconnection.cpp:540`). A
  filter that returns `true` stops the event there.

### What changed

`bpr::app::xcb_activation_host` (`src/app/src/xcb_activation_host.hpp`,
`.cpp`) is a native event filter. `main.cpp` creates it once, right after
`QApplication`, and only on Linux. It does nothing unless the platform is
`xcb`. There it keeps from Qt every `_NET_ACTIVE_WINDOW` client message
(format 32) except one that names the application's modal window. Qt then
changes its focus window only on the X server's FocusIn.

- **Where the window manager grants the request**, the FocusIn follows, so
  the end state is the same. Qt changes focus a few milliseconds later, when
  the grant arrives.
- **Where it refuses** (WSLg always; any window manager with focus-stealing
  prevention sometimes), Qt keeps its focus window on the window that holds
  X11 focus. Keys go where the compositor shows focus.
- **The modal window.** Qt relays focus-in on a blocked window to the modal
  window by asking for the modal window's activation (`relayFocusToModalWindow`).
  That request still reaches Qt, so keys typed while a modal window is up
  still go to it, as they do now. The limit that follows: a request for a
  window-modal dialog, made while an unrelated window holds X11 focus, still
  sends that window's keys to the dialog. No product path makes that request.
- The host performs, as `windows_title_row_host` does on Windows: it
  filters the platform's events. No widget or model changes.
  Windows and macOS do not compile it. The offscreen tests run it inactive.
- **The one new dependency is libxcb.** The host names one atom on Qt's own
  connection (`QNativeInterface::QX11Application`), so the composition links
  libxcb through `pkg-config` (`PkgConfig::BPR_XCB`, recorded in the
  dependency audit). libxcb is already in the runtime closure of Qt's xcb
  plugin, which the Linux package ships as its only platform plugin.
  `README.md`, `docs/development/build-and-test.md`,
  `docs/development/release-packages.md`,
  `docs/architecture/system-architecture.md` and
  `docs/architecture/release-readiness.md` name it.

**Why this layer.**

- Not asking for activation on Linux would lose the return of focus where
  window managers grant it.
- Forcing focus through X11, the compositor or Windows is out of scope, and
  Weston resets X11 focus anyway ([[BPR-0975]], attribution 1).
- Correcting Qt after the fact would need Qt's private window-system
  interface. It would also need a time bound, because nothing tells a slow
  grant from a refusal.
- An upstream report alone leaves keys misrouted until a Qt release that
  changes the handler. A report is still worth filing: the handler, and the
  StructureNotify mask that feeds it, are Qt's. After AC1's bare capture it
  can be prepared from that capture. Filing it is the owner's decision.

This departs from the scope's order, which was to reproduce first and then
decide. The source at 6.11.2 and [[BPR-0975]]'s two captures already show
the mechanism. AC1's bare run on WSLg still has to confirm it. If it does
not, the host is reverted.

### Composition pin reseal: the review

The pins matched at `b60e04b0`: the full-application digest was `6e76dd7c…`
and the manifest digest was `8e5bc591…`. So the only change inside the
pinned scope is this card's. In `main.cpp`, one include and one object after
`QApplication` are added, both `#if defined(__linux__)`. The inventories gain
`src/xcb_activation_host.cpp` and `.hpp`. The host reads one atom and the
modal window and writes nothing. No credential, provider, vault, transport or
mutation path changes. The full-application and mutation-branch pins are
unchanged. The manifest pin is resealed to `a61473dd…`.

### Verification, offscreen

| Check | Result |
| --- | --- |
| `cmake --preset dev` and `cmake --build --preset dev` | Exit 0, no `warning:` line. The application now needs `libxcb.so.1` directly |
| `application_xcb_activation_host` (new, Linux only, offscreen) | Passes. The request is kept from Qt, with or without the sent bit, and also while another window is modal. It reaches Qt when it names the modal window, and nothing is kept without the atom. Another client message, a format-8 message, the X server's FocusIn, and a FocusIn with the request's bytes all reach Qt. The host is inactive offscreen and keeps nothing |
| Negative controls, each a temporary edit, rebuilt and run, then undone (`bpr-1024-negctl.log`) | The type check removed: `another client message reaches Qt`. The modal exception removed: `a request for the modal window reaches Qt`. The event-code check removed: `only a client message is ever kept from Qt`. Nothing kept: `Qt's own activation request is kept from Qt`. Two of them first failed to build under `-Werror`, and the log says so. They were rerun with the variable still used. The host's SHA-256 is back to `27ca6ffe…` before the reseal |
| Focused rows (`bpr-1024-rows-2.log`, `-rows-3.log`) | 16 of 16 pass: the new row, `application_review_round_trip_composition_contract` (resealed), `dependency_modernization_contract`, `test_network_isolation_manifest`, `documentation_structure`, `core_boundary_contract`, both conditional-branch rows, `application_workbench_composition_contract`, `application_profile_second_window_close_public_path`, `qt_workbench_focus_order_scale_100`, `application_profile_startup_public_path`, `application_logging`, `contained_temporary_usage_contract`, `coding_format_check` and `coding_standard_format_gate_contract` |
| `architecture_boundaries` | Red at `b60e04b0` without this change, as recorded for `0ba63b0a`. It stops at its first failure, so it was also run with every `FATAL_ERROR` turned into a warning, at `b60e04b0` and with this change: the same 14 findings, and no difference (`bpr-1024-arch-differential.log`) |
| `/usr/bin/clang-format` 22.1.8 | Clean on every touched C++ file |
| `_coding-standard/scripts/tidy.py` (clang-tidy 21.1.8) on the host and its test | No finding |

### The WSLg evidence the tools now take

- **AC1.** `linux_settings_focus_control.cpp` takes `--commands`: standard
  input asks for `activate A` or `activate B` (that window's
  `activateWindow()`), `focus` (the window Qt treats as focused) or `quit`.
  Each window prints the keys Qt delivers to it. `--consume-activation-echo`
  installs the same rule in the bare control, with no BetterPullReview code.
  `linux_settings_focus_control_course.py --refused-activation` adds one step
  to each Qt control while B holds X11 focus. A asks for activation, and the
  step records the `_NET_ACTIVE_WINDOW` requests seen on the root, X11 focus
  afterwards, Qt's focus window, and which window got one key sent where X11
  focus is. The Qt control runs twice: as it is, and with
  `--consume-activation-echo`. Offscreen, the command channel was exercised
  through the course's own helpers: `focus` answered `B`, `activate A`
  answered `A`, and then `focus` answered `A`, because the offscreen plugin
  grants activation. `--consume-activation-echo` refuses to start off xcb.
  Its first build crashed there, because the offscreen plugin also answers
  the X11 interface without a connection, and that is now checked.
- **AC3.** `linux_settings_course.py --keys-follow-x11` is the
  refused-activation course with one difference. When the refused window is
  not the next target, and the target already holds X11 focus, the observer
  sends the target its keys without first giving the refused window a real
  focus-in. The case is Settings closing with the sibling focused. It passes
  only if the sibling's Ctrl-comma opens Settings for the sibling, which
  "Sibling retirement lost Settings" then checks. The capture records each
  such refusal under `keys_followed_x11` and adds the check
  `refused-activation-keys-follow-x11`.
- The course scripts parse, and `settings_inventory_comparison_tests.py`
  passes (16 tests). The courses themselves need the WSLg display.

### Checkpoint: waiting for a desktop hold

The candidate is this card's product commit on `main`, named in the commit
that follows it. Every criterion needs the real WSLg display, so each is
open:

- **AC1: pending.** On a head that contains the tools, with
  [[BPR-0975]]'s owned root, lease and guard:
  `$W/guarded.sh /usr/bin/python3 tests/scripts/linux_settings_focus_control_course.py --qt-root $W/cache/bprqt/6.11.2/6.11.2/gcc_64 --work-root $W/runs --output $W/runs/linux-focus-control-refused.json --refused-activation`.
  Expected: `BARE_QT_REFUSED_ACTIVATION:DIVERGED` (a key sent to B reaches
  A) and `BARE_QT_CONSUME_REFUSED_ACTIVATION:FOLLOWED_X11` (it reaches B).
- **AC2: recorded here.** The disposition is a product correction. AC1's
  capture completes its evidence.
- **AC3: pending.** On a clean clone at the candidate, build `app-release`
  and run `linux_settings_course.py --keys-follow-x11`, which is expected to
  pass. Run `--bound-refused-activation` too, which must still pass every
  step. The negative control is the same `--keys-follow-x11` course on the
  candidate's parent, which is expected to fail at "Sibling retirement lost
  Settings". macOS and Windows do not compile the host.
- **AC4: pending, with the runs.** Fresh mode-0700 XDG and TMP roots, the
  station lease and the live-state guard. No display, security or
  foreground-lock setting changes.
