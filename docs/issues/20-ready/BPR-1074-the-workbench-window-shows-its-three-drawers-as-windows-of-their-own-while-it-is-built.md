---
schema: "implement-rfc.card.v1"
id: "BPR-1074"
title: "The workbench window shows its three drawers as windows of their own while it is built, so a probe that took the first window for the application saw it stop answering"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1022's real-key verify runs","found_at":"b9991925","measured_at":"b5cb3b43","introduced_by":"3c9a3f1c (BPR-0721)","related_cards":["BPR-1022","BPR-0721","BPR-0646","BPR-0980"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1072--BPR-1074--bd67ade43931.json"}
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

Pending.
