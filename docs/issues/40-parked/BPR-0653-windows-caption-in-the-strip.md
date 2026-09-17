---
schema: "implement-rfc.card.v1"
id: "BPR-0653"
title: "Windows: the strip as the title row"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
parked_on: "human-review"
---

# Windows: the strip as the title row

[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) makes the
strip the row the window controls sit in. BPR-0646 delivers the macOS form
through Qt's expanded client area (the traffic lights in the strip's left
inset, the title text hidden, empty strip space dragging and zooming the
window). The Windows form follows Visual Studio Code, the owner's closest
reference on that platform, and is platform work of its own so the portable
strip never carries it.

## Scope

- The workbench window on Windows draws no system title bar. The strip
  carries, at its left, a compact menu control standing in for the menu bar
  rows (the File, View and Review menus the window already builds), then the
  same regions as on macOS; at its right, after settings, the minimise,
  maximise-or-restore and close buttons in the platform's current caption
  style, 46 pixels wide each, in the strip's ink.
- Snap layouts on hover of the maximise button, dragging by empty strip
  space, double-click to maximise and restore, Aero shake and edge snapping
  keep working: the window answers `WM_NCHITTEST` with `HTCAPTION` for empty
  strip space and the caption-button codes for the three buttons, through a
  native event filter in `src/app` (host performs) rather than by moving the
  window by hand.
- The frame extends into the client area so the platform's rounded corners,
  shadow and resize borders remain; the strip's top edge keeps the one-pixel
  resize band usable.
- The strip's reading order, focus order and accessibility names stay the
  ones BPR-0646 pinned; the caption buttons are the last three stops.
- Owned paths: the Windows branch of `src/app` composition, a Windows-only
  presenter for the caption buttons in `src/adapters/qt`, the Windows
  registration inventory and its focused tests.

## Acceptance

- RFC-BPR-0028 AC-01 on Windows at 1280x852 and 1920x1080: the chrome above
  the comparison is the strip and the epoch line, and the strip is the row
  the caption buttons sit in.
- Snap layouts appear on hovering the maximise button; dragging empty strip
  space moves the window; double-click maximises and restores.
- The Windows station's focused Qt selectors and the manifest gate pass; the
  new uncontained registration, if any, is pinned by name in the same commit.
- Attended capture on the Windows station showing the strip with the caption
  buttons and the snap layout flyout.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.

## Windows station, 2026-09-17 (task-20260917-win-0843-compile-0653): parked on the width floor

The base is `014489e3e8bf36c0f1c61b58124bf45ad8143f1f`. Logs and the kept work are
under `build/logs/task-20260917-win-0843-compile-0653/`. Nothing below is committed
except this record.

### The platform mechanism, measured

A standalone probe (`bpr0653-title-probe.cpp`) ran on the real `windows` platform
at 150% display scale on JaegAW. It is a `QMainWindow` with a toolbar row, and
two pieces stand in for the product:

- **Frame.** Qt's own Windows native interface removes the title bar from Qt's
  frame computation:
  `QNativeInterface::Private::QWindowsWindow::setCustomMargins(QMargins{0, -top, 0, 0})`,
  with `top = SM_CYCAPTION + SM_CYSIZEFRAME + SM_CXPADDEDBORDER` for the window's
  DPI (`GetSystemMetricsForDpi`).
- **Hit test.** An application `QAbstractNativeEventFilter` answers `WM_NCHITTEST`.

| Check | Result |
|---|---|
| Title bar | Gone. The window's top edge and the client origin coincide (`window.top == client-origin.y == 270`); Qt's own geometry agrees (`frameGeometry().y() == geometry().y()`). |
| Side resize borders and shadow | Kept. `DefWindowProc` answers `HTLEFT` 11 physical pixels inside the left edge. |
| Hit codes from the filter | Bare toolbar surface `HTCAPTION` (2), maximise button `HTMAXBUTTON` (9), a control `HTCLIENT` (1), the top one-pixel band `HTTOP` (12) |
| Double-click | `WM_NCLBUTTONDBLCLK` with `HTCAPTION` maximises (`IsZoomed` 1, `isMaximized` true); a second one restores to the same rectangle |
| Maximised | With the normal margin the client origin sits at y = -11, above the work area, and 11 physical pixels of the row would be cut off. Re-applying the margin on `windowStateChanged` with the frame kept while zoomed (`top = SM_CYCAPTION` only) puts the client origin at the work area's top (0,0); restoring goes back to the full margin. |

`src/` uses no private Qt header today, and no gate forbids one. The native
interface above is Qt's own typed hook for this, but it is private API, so the
host would be tied to the pinned Qt 6.10.3 and rechecked on every Qt upgrade.

### The product work, built as far as the question allows

The patch is `bpr0653-windows-title-row-work-in-progress.patch`, 665 lines, not
applied. It builds warning-clean on Windows.

- **`src/adapters/qt/{include/bpr/qt,src}/windows_caption_buttons.*`** (Windows
  only). Minimise, maximise-or-restore and close, each 46 pixels wide and the
  strip's 30 pixels tall.
  - Segoe Fluent Icons, or Segoe MDL2 Assets on Windows 10, glyphs E921, E922,
    E923 and E8BB in the strip's ink.
  - Windows 11 hover and press washes, and the close red.
  - Accessible names Minimize, Maximize or Restore, and Close, each with a
    description. They take strong focus.
  - A press asks the host through a dispatch; without one it does nothing.
  - `part_at()` for the hit test, and `set_native_state()` for the non-client
    hover and press the host reports.
  - The compact menu control's three-line glyph.
- **`workbench_strip`.** A public `empty_surface_at(QPoint)`, the tracked drag's
  own bare-surface rule.
- **`workbench_window`, Windows branches only.**
  - A compact menu control after the platform inset (accessible name
    "Application menu") that opens the menu bar's File, Profiles and View menus,
    read from the bar each time.
  - The caption buttons after Settings.
  - The menu bar hidden, with its Quit shortcut kept live on the window.
  - Focus order: the menu control first; the three caption buttons are the
    strip's last three stops.
  - `title_row_part_at(QPoint)`: caption buttons, bare surface as caption,
    everything else client.
- **`workbench_window_tests`.** A platform-aware lead index for the strip's
  action order.

Not yet written: the `src/app` host (native event filter, margins, hover and press
presentation, dispatch of the three actions), its Windows-platform test and its
registration.

### The question (why this card parks)

With the Windows row as the card describes it, the strip does not fit at the
window's floor:

- `qt_workbench_window_scale_100` stops at "no strip control folds into an
  overflow menu at the width floor".
- A reverted width sweep over the same window found the strip fits with every
  control top-level **from 1080 logical pixels** (toolbar size hint 1078) at
  100% and at 125%.

The floor is 900, and the owner kept it on 2026-09-11 ("leave it be for now",
recorded on [[BPR-0722]]) for the strip without the Windows controls. Those
controls (the menu control plus three 46-pixel caption buttons) add about 180
pixels. The strip's rule is that no control ever folds into an overflow menu.

**On Windows, should the window's minimum width rise from 900 to 1080, or should
a strip control yield below 1080 on Windows (and if so which: the modality
segments' compact form earlier, Next new comment, or the menu control), keeping
the no-overflow rule?**

Either answer is a small change to `setMinimumSize` or `apply_strip_width` in the
Windows branch, and then the host, its test and the registration follow.

### Step 1 of the same task (BPR-0843, reported, not this card)

At the same base, a full windows-dev build succeeds with no warnings.
`src/app/main.cpp` compiles under `/W4 /permissive- /WX` with
`BPR_BUILD_TESTING=1`, and `ctest -N` does not list
`application_epoch_fixture_noncanonical_root_cause`.

### Attended capture (criterion 4), prepared for when the card resumes

On JaegAW, signed in to the station account, at 100% and at 150% display scale:

1. Start the tests-disabled `app-release` build and open any pull request.
2. Capture the window restored at 1280x852: the strip is the first row, with the
   menu control at its left and minimise, maximise and close at its right; no
   system title bar; the epoch line directly beneath.
3. Hover the maximise button for about a second: capture the snap layout flyout.
4. Drag empty strip space and release against the left screen edge: capture the
   snapped window.
5. Double-click empty strip space (maximised), then again (restored).
6. Maximise and capture the strip at 1920x1080: its top row not cut off.
