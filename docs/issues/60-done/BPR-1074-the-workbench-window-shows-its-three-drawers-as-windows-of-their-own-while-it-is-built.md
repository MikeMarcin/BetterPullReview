---
schema: "implement-rfc.card.v1"
id: "BPR-1074"
title: "The workbench window shows its three drawers as windows of their own while it is built, so a probe that took the first window for the application saw it stop answering"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1022's real-key verify runs","found_at":"b9991925","measured_at":"b5cb3b43","introduced_by":"3c9a3f1c (BPR-0721)","related_cards":["BPR-1022","BPR-0721","BPR-0646","BPR-0980"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1072--BPR-1074--bd67ade43931.json"}
acceptance: [{"criterion":"AC1 The cause is named with evidence: whether the application stops answering, what the probe held when it saw no answer, and how often across launches","outcome":"pass","revision":"b5cb3b43","evidence":{"kind":"observation","detail":"bpr-main-win, JaegAW, Windows 11 10.0.26200, real windows platform, build/dev MSVC 19.51 Debug, Qt 6.11.2, cleared foreground 04:44:16Z-04:48:13Z. The probe's PowerShell is medium integrity, in no job and no AppContainer, session 1; no QT_QPA_PLATFORM leaked; no WSLg window held the foreground. In 5 of 5 launches the first window found (0.56-0.69 s) was a 271x251 window titled BetterPullReview that was gone within 80 ms: IsWindow false, SendMessageTimeoutW failing at once with ERROR_INVALID_WINDOW_HANDLE (1400), not a timeout. The workbench window that followed answered every WM_NULL in 0-2 ms and closed with exit 0 on WM_CLOSE. qt.qpa.window logging named the early windows workbenchFilesDrawerWindow, workbenchConversationsDrawerWindow and workbenchRepositoriesDrawerWindow: shown, then destroyed when CentralSplitter took each drawer. Cause: workbench_drawer_panel::present() (3c9a3f1c, BPR-0721) shows any panel for which isHidden() is true, which includes a fresh panel with no parent.","path":"docs/issues/60-done/BPR-1074-the-workbench-window-shows-its-three-drawers-as-windows-of-their-own-while-it-is-built.md#measured-bpr-main-win-2026-09-24-b5cb3b43"}},{"criterion":"AC2 With the fix, direct launches on the real Windows desktop show one top-level application window, the workbench window, from launch until it settles; the first window the probe finds answers WM_NULL throughout, in N of N launches (N >= 5)","outcome":"pass","revision":"0bc65b0c","evidence":{"kind":"observation","detail":"Same probe and desktop, 05:04:28Z-05:05:10Z, the product source of 0bc65b0c: 5 of 5 launches found the workbench window (BetterPullReview - Default profile - BetterPullReview) first, about 1.8 s after launch; it was the only visible application window of each run, answered 13 of 13 WM_NULLs, and exited 0 on WM_CLOSE. Before the fix 5 of 5 launches showed the drawers first.","path":"docs/issues/60-done/BPR-1074-the-workbench-window-shows-its-three-drawers-as-windows-of-their-own-while-it-is-built.md#fixed-in-the-drawer-bpr-main-win-2026-09-24"}},{"criterion":"AC3 An offscreen guard fails without the fix and passes with it; the drawer rows pass on the Windows station: qt_native_window_geometry, qt_workbench_focus_order_scale_{100,125,200}, qt_workbench_title_row_scale_100, qt_workbench_overview, qt_workbench_conversation_reveal and qt_composed_github_scale_100","outcome":"pass","revision":"0bc65b0c","evidence":{"kind":"command","detail":"test_building_the_window_shows_no_window_of_its_own runs in qt_native_window_geometry (offscreen, every platform). Negative controls: without the parent check it fails naming workbenchFilesDrawer, workbenchConversationsDrawer, workbenchRepositoriesDrawer; without the placement's apply_visibility it fails at 'an expanded drawer shows once it is placed'. With the fix, serial: qt_native_window_geometry, qt_workbench_focus_order_scale_100/125/200, qt_workbench_title_row_scale_100, qt_workbench_strip_menu, qt_workbench_conversation_reveal, qt_workbench_overview and qt_composed_github_scale_100 pass 9 of 9; application_workbench_composition_contract passes; the window row's thirteen drawer courses pass through a temporary uncommitted mode (the full row stops earlier at BPR-1073). clang-format clean; clang-tidy clean on the change (one older finding at line 8923 recorded).","path":"docs/issues/60-done/BPR-1074-the-workbench-window-shows-its-three-drawers-as-windows-of-their-own-while-it-is-built.md#fixed-in-the-drawer-bpr-main-win-2026-09-24"}}]
---

# BPR-1074: The workbench window shows its three drawers as windows of their own while it is built, so a probe that took the first window for the application saw it stop answering

## What was reported

[[BPR-1022]]'s station could not drive its candidate with real keys. The
harness launched `better_pull_review.exe` directly on the real desktop, found
its first visible, unowned, titled window, and tried to bring it to the front.
Each time that window answered no `WM_NULL`, was not reported hung, and did
not close on `WM_CLOSE`. `HEAD` without BPR-1022's change did the same in 4 of
5 launches. The reserved title said the application "often answers no window
message after its first paint".

The reserved title was a guess at the cause, and it is wrong. The application
answers. The window the harness held was gone. This title says what happens.

## Measured (bpr-main-win, 2026-09-24, `b5cb3b43`)

`build/dev`, MSVC 19.51 Debug, Qt 6.11.2, the real `windows` platform,
Windows 11 10.0.26200, a 4K panel at 150%. Each launch used a fresh
owner-private data root and no `QT_QPA_PLATFORM`. A station probe (not
committed) looked for the first window every 100 ms and then sent `WM_NULL`
with `SendMessageTimeoutW`, reading the error code. It injected no input and
never touched the pointer. Logs are station records under
`build/logs/bpr-1074-main-win/`, not committed.

**The station was ruled out first.** The probe's PowerShell runs at medium
integrity, outside any job and outside any AppContainer, in session 1, as do
Explorer and WSLg's `msrdc.exe`. So UIPI and job UI limits cannot drop its
messages. No `QT_QPA_PLATFORM` leaks into the launch environment. No WSLg
window held the foreground before any run.

**The first window dies; the application does not.** In 5 of 5 launches the
first window the probe found was a 271×251-pixel window titled
`BetterPullReview`, 0.56–0.69 s after launch. Within 80 ms it no longer
existed: `IsWindow` false, and `SendMessageTimeoutW` failed at once with
`ERROR_INVALID_WINDOW_HANDLE` (1400), not with a timeout. A second window at
the same place replaced it and went the same way. About a second after launch
the real workbench window appeared, titled
`BetterPullReview — Default profile - BetterPullReview`. It answered every
`WM_NULL` in 0–2 ms for the rest of the run, and it closed with exit code 0 on
`WM_CLOSE`. The GUI thread's stack, read at the first failed `WM_NULL` of each
launch, was still inside `workbench_window`'s constructor, in
`build_github_header`, building the rest of the window. The earlier
segment's stack, read later in a launch, was idle in `QApplication::exec()`.
Neither is a thread that has stopped.

That explains every earlier observation. `IsHungAppWindow` reports false for
a handle that is not a window. A `WM_CLOSE` posted to a destroyed handle is
dropped, so the process stays open. The earlier harness polled every 250 ms,
so it caught a short-lived window in 4 of 5 launches and missed it once.

**What the windows are.** With `QT_LOGGING_RULES=qt.qpa.window.debug=true`,
Qt names each native window it makes. Before the workbench window, it creates
and shows three top-level windows: `workbenchFilesDrawerWindow`,
`workbenchConversationsDrawerWindow` and `workbenchRepositoriesDrawerWindow`.
Each is shown (`setVisible true`), then hidden, re-flagged and destroyed as
`CentralSplitter` takes its drawer. They are shown in creation order and
destroyed in splitter order.

**The cause, in the code.** `workbench_window::build_central_surface()`
creates the three `workbench_drawer_panel`s with no parent and adds them to
`CentralSplitter` about 500 lines later. The panel's constructor calls
`present()`. Since
[[BPR-0721]] (`3c9a3f1c`), `present()` ends like this:

```cpp
// Only an explicitly hidden panel is shown again: a fresh panel has no
// parent yet, and showing it would make it a window of its own.
if (!(expanded || ControlsShown)) {
    hide();
} else if (isHidden()) {
    show();
}
```

A panel starts expanded. `QWidget::isHidden()` is true for a fresh widget as
well as for an explicitly hidden one, so a fresh panel with no parent is
shown, which makes it a window of its own. That is what the comment meant to
prevent. Every launch on every platform does this. On Windows each drawer is
a real window on the desktop for up to about a second.

## Scope

- Fix `workbench_drawer_panel` so that a panel with no parent is never
  shown, and so that the visibility it decides is applied once it is placed.
  Keep what BPR-0721 made it do once it has a parent: collapsed with the
  controls hidden is absence, and an explicitly hidden panel shows again when
  it expands.
- Add a guard that fails without the fix: offscreen, on every platform, a
  course that builds the workbench window and asserts that nothing is shown
  as a window while it is built, and that a drawer panel with no parent is
  never shown.
- Repair the station's probe as well: it must follow the window it is
  looking at and check that the handle is still a window.

## Acceptance

- AC1 The cause is named with evidence: whether the application stops
  answering, what the probe held when it saw no answer, and how often across
  launches.
- AC2 With the fix, direct launches on the real Windows desktop show one
  top-level application window, the workbench window, from launch until it
  settles. The first window the probe finds answers `WM_NULL` throughout, in
  N of N launches (N ≥ 5).
- AC3 An offscreen guard fails without the fix and passes with it. The drawer
  rows pass on the Windows station:
  `qt_native_window_geometry`, `qt_workbench_focus_order_scale_{100,125,200}`,
  `qt_workbench_title_row_scale_100`, `qt_workbench_overview`,
  `qt_workbench_conversation_reveal` and `qt_composed_github_scale_100`.

## Evidence

### Fixed in the drawer (bpr-main-win, 2026-09-24)

`present()` now ends in `apply_visibility()`. Collapsed with the controls
hidden still hides the panel, which is safe with or without a parent. A panel
that should show is shown only if it has a parent. A panel without one is
shown by its placement: `changeEvent` applies the same decision on
`QEvent::ParentChange`. Everything BPR-0721 made the panel do once it is in
the splitter is unchanged. The change is in `workbench_drawer.cpp` and its
header, in the Qt adapter, on every platform.

**The guard** is `test_building_the_window_shows_no_window_of_its_own` in
`tests/adapters_qt/workbench_window_tests.cpp`. It runs in the
`--native-window-geometry` mode, so the existing row `qt_native_window_geometry`
runs it on every platform, offscreen. An application-wide filter names every
widget shown as a window. The test asserts:

- building a `workbench_window` shows no window, and each drawer is placed
  in it, not a window of its own;
- showing the window shows exactly `workbenchWindow`, and the three expanded
  drawers show once a published review replaces Home;
- a drawer panel with no parent is never shown, whatever it adopts: collapsed,
  controls shown, expanded, controls hidden;
- once placed, an expanded panel shows, and a collapsed panel with the
  controls hidden stays absent until it expands;
- a panel hidden and expanded before it was placed shows when it is placed.

**Negative controls.** Each was a temporary edit to `workbench_drawer.cpp`,
rebuilt and run, then restored.

| Removed | `qt_native_window_geometry` fails at |
| --- | --- |
| the parent check (the old `else if (isHidden()) show();`) | `building the workbench window shows no window of its own; shown: workbenchFilesDrawer, workbenchConversationsDrawer, workbenchRepositoriesDrawer` |
| the placement's `apply_visibility()` in `changeEvent` | `an expanded drawer shows once it is placed` |

**On the real desktop, with the fix (AC2).** The same probe, five launches,
05:04:28Z–05:05:10Z. In 5 of 5 the first window it found was the workbench
window, titled `BetterPullReview — Default profile - BetterPullReview`, about
1.8 s after launch. It was the only visible application window of the run,
it answered 13 of 13 `WM_NULL`s, and it closed with exit code 0 on
`WM_CLOSE`. Before the fix, 5 of 5 launches showed the drawers first.

**Rows (AC3)**, `build/dev`, MSVC 19.51 Debug, Qt 6.11.2, serial, with the
change:

- `qt_native_window_geometry`, `qt_workbench_focus_order_scale_100/125/200`,
  `qt_workbench_title_row_scale_100`, `qt_workbench_strip_menu`,
  `qt_workbench_conversation_reveal`, `qt_workbench_overview` and
  `qt_composed_github_scale_100`: 9 of 9 pass.
- `application_workbench_composition_contract` passes (14.18 s).
- The full window row stops earlier on this station at [[BPR-1073]]'s
  failure, so its thirteen drawer courses were run through a temporary mode
  of the same executable, offscreen at 1.0, and not committed:
  `test_drawer_toggles_follow_core_command_projections` through
  `test_repositories_drawer_toggles_through_the_core_and_collapses_to_a_rail`,
  including BPR-0721's `test_hidden_drawer_controls_leave_no_column_and_no_hide_button`.
  All thirteen pass.
- `clang-format` 22.1.8 reports the three touched files clean. `clang-tidy`
  with warnings as errors reports nothing in `workbench_drawer.cpp` or in the
  new test lines. It does report one finding in the test file that is older
  than this change: `wait_for_frame` at line 8923 (`4bbecb48`) breaks
  `readability-identifier-naming`.

**Not this card's, seen on the way.** `architecture_boundaries` fails at
this revision with `Agent process backend duplicates shared protocol logic:
maximum_agent_stderr_bytes`. [[BPR-0886]] (`0ba63b0a`) bounds the parked
standard error in `windows_agent_process.cpp` with
`application::maximum_agent_stderr_bytes`, which the boundary reserves for
the shared protocol loop. It is reported to the coordinator for a card of its
own.

**The station's probe** now re-reads the application's window on every pass
and checks `IsWindow` before it trusts a handle. So an early window that goes
away can no longer pass for a hang. It is a station helper and is not
committed.

**macOS and Linux.** The fix is in shared Qt code, and the guard runs
offscreen in `qt_native_window_geometry` on every platform. It has only been
run here. On macOS, a panel shown without a parent would have been a window
of its own too, so the flash may have been visible there. Wanted on
bpr-main-mac and bpr-main-linux as a post-close confirmation:
`ctest --preset dev -R '^(qt_native_window_geometry|qt_workbench_window_scale_100)$' --output-on-failure`.
