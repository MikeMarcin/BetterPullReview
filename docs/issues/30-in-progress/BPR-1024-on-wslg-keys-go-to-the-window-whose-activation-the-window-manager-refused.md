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
