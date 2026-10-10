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
acceptance: [{"criterion":"AC1 RFC-BPR-0028 AC-01, as amended by the owner on 2026-09-24, on Windows at 1280x852 and 1920x1080: the chrome above the comparison is the strip, the review surface's entry rows and the epoch line, and the strip is the row the caption buttons sit in","outcome":"pass","revision":"38cc26d7","evidence":{"kind":"command","detail":"Criterion amended, not reworded to fit: the owner's verdict in attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0653-entry-row-v001, fingerprint 308902012d73bb4ce5f8c6cc34e2215412c11b4db78f9e3f36867b84b03bd5bd, decision 007a6eb5-adf3-427c-8473-547f9d0c4d9e, actor human, 2026-09-24T18:54:09Z, selected 'Amend RFC-BPR-0028 AC-01 and the chrome paragraph to admit the entry rows between the strip and the epoch line'. Against the amended words: ctest --preset dev -R application_windows_title_row on the real windows platform (Qt 6.11.2, 150% scale) passes at f62b745d, at 53d229d7 and on main at 38cc26d7, printing title-row:1280x852:ok and title-row:1920x1080:ok: no title bar, the strip first and as wide as the window with the caption buttons in it (46 px, row height, close ending the row), the client area directly beneath the strip with no menu bar row, and between the strip and the epoch line only composeReviewActionButton@30+20 and composeInlineDraftActionButton@30+20, the composition entry row the amendment admits. It passes again in BPR-1028's popup() form at 2a5580d6 (product source of e08a0409), 1.96 s. The portable form is asserted by qt_workbench_window_scale_* since BPR-0940 (f839e26a).","path":"docs/issues/40-parked/BPR-0653-windows-caption-in-the-strip.md#owner-decisions-2026-09-24-attended-session-cbd48b91"}},{"criterion":"AC2 Snap layouts appear on hovering the maximise button; dragging empty strip space moves the window; double-click maximises and restores","outcome":"pending","revision":"727ad9d7","evidence":{"kind":"observation","detail":"150% accepted by the owner; 100% owed. Attended session df5859c784ba447497026d6cd81af810, candidate BPR-0653-windows-capture-150-v001, revision 727ad9d7 (the station's capture of image 6aa27c8de85604d38a04c8728c610d2afb1fbcbd541e489bdedcc3891d798ac4, source 85e522d8), fingerprint b086dd44326bf32cef5d0703988972e3cd5839786c5cc2434ae66c009e39e775, decision 1aba8610-6768-408a-a0a1-e1613c62c89f, actor human, 2026-09-25T05:07:37Z, selected 'Accept the Windows caption and strip visuals as captured at 150%', no note. At DPI 144 the accepted capture shows the snap layouts flyout on hovering Maximize; a real-pointer (SendInput) drag of empty strip space moving the window by exactly the pointer's travel, (+160, +120) physical pixels in capture.json; and a double-click on empty strip space restoring the maximized window to the same frame. The maximizing double-click is application_windows_title_row's evidence (WM_NCLBUTTONDBLCLK on bare strip surface maximises on the real platform, at 53d229d7 and since), and the question named it as not pictured. The snap and drag observations ride criterion 4's capture, which this card binds at 100% and 150%: the same observations at 100% display scale are owed, and no owner amendment waives them.","path":"docs/issues/40-parked/BPR-0653-windows-caption-in-the-strip.md#the-owners-verdict-on-the-150-capture-2026-09-25-main-mac"}},{"criterion":"AC3 The Windows station's focused Qt selectors and the manifest gate pass; the new uncontained registration, if any, is pinned by name in the same commit","outcome":"pending","revision":"3da31f2a","evidence":{"kind":"command","detail":"One unattended pass at main 3da31f2a (BPR-1129 not landed), build/dev rebuilt with no warning, Windows 11 Home 10.0.26200, Qt 6.11.2, 150%. 19 of 20 rows pass: application_windows_title_row on the real windows platform (1.84 s, title-row:1280x852:ok and title-row:1920x1080:ok), qt_workbench_title_row_scale_{100,125,200}, qt_workbench_focus_order_scale_{100,125,200}, qt_workbench_window_scale_{100,125,200}, qt_workbench_strip_menu, qt_composed_github_scale_100, qt_home_list_scale_100, qt_review_queue, application_diff_workbench_public_path, application_review_round_trip_composition_contract, test_network_isolation_structure_contract, architecture_boundaries and core_dependency_boundary. test_network_isolation_manifest is red on exactly BPR-1108's twelve findings and nothing else: three each (not in the pinned Windows set; no sandbox launcher prefix; no bpr_network_isolated label) for sqlite_release_maintenance_files, application_attended_recent_replay, application_release_maintenance_public_path and application_release_update_public_path. This card adds no uncontained registration. Pending only on the manifest: the criterion says the gate passes and predates BPR-1108, so it is met when that red is accepted as outside it or when those four rows run contained. Logs: build/logs/bpr-0653-main-win/c3-3da31f2a/ on the Windows station.","path":"docs/issues/40-parked/BPR-0653-windows-caption-in-the-strip.md#criterion-3-at-one-main-revision-2026-09-25-bpr-main-win"}},{"criterion":"AC4 Attended capture on the Windows station showing the strip with the caption buttons and the snap layout flyout","outcome":"pending","revision":"727ad9d7","evidence":{"kind":"observation","detail":"150% taken and accepted by the owner; 100% owed. The station captured the strip as the title row with Minimize, Maximize and Close at rest, hovered and pressed, and the snap layouts flyout, in one launch of image 6aa27c8d…98ac4 (bundle ca3bc2b1…8f91, source 85e522d8) at DPI 144, 2026-09-25T04:29:54Z to 04:30:23Z: docs/verification/BPR-0653-2026-09-25, commit 727ad9d7, 39 images each hashing to capture.json. The owner accepted it in attended session df5859c784ba447497026d6cd81af810, decision 1aba8610-6768-408a-a0a1-e1613c62c89f, fingerprint b086dd44326bf32cef5d0703988972e3cd5839786c5cc2434ae66c009e39e775, 2026-09-25T05:07:37Z, actor human, no note. The accept covers that capture at 150% only, with the limits its question stated: driven by windows_caption_capture.ps1, not by a person; the window on Home, signed out; edge snap, the 1920x1080 maximized size and the maximizing double-click not pictured. Owed: the same capture at 100% display scale, which needs the owner to change the display scale on JaegAW, and the owner's review of it. The question told the owner that 100% stays owed whatever the answer, and the verdict amends nothing.","path":"docs/issues/40-parked/BPR-0653-windows-caption-in-the-strip.md#the-owners-verdict-on-the-150-capture-2026-09-25-main-mac"}}]
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
  the comparison is the strip, the review surface's entry rows and the epoch
  line, and the strip is the row the caption buttons sit in. (Amended
  2026-09-24 with RFC-BPR-0028 AC-01; it said "the strip and the epoch line".)
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

## Owner answers, 2026-09-17 (relayed verbatim by the coordinator, dsp-004)

The coordinator put the parked question to the owner: raise the Windows
minimum width to 1,080 pixels, or have one strip control give way below that.

The owner's first answer:

> What would we lose? If it's just the title that's fine so long as the controls stay.

The owner's proposal:

> What about reducing the modality labels to 3 characters prefix each if the window shrinks below that threshold?

**Reading, as sent to the station.**

- Letting text give way is acceptable, whether the pull request identity or
  title, or the modality labels shortened to 3-character prefixes (`Ove`,
  `Rea`, `Rev`, `Dis`, all distinct).
- Every strip control stays top level. No control goes into an overflow menu.
- The accessible names and tooltips should keep the full words.

**Measurement required before implementing.** Measure whether the 3-character
labels, together with the identity's existing yield, keep every strip control
top level at the 900-pixel floor on Windows, at 100% and at 125%.

- **If it fits:** that is the owner's chosen answer.
- **If it does not fit:** return the remaining shortfall in pixels as an exact
  ask. Do not choose another yield.

**Open, only if it matters.** Should the 3-character step also apply on macOS,
or only on Windows? The owner did not say.

## Windows station, 2026-09-17 (task-20260917-win-0653-resume): the 3-character labels are 2 pixels short

The base is `a1864fa296ce3450ce0e0470f36bd0d95cd59470`. The measurement is a
reverted trial; nothing but this record is committed. Logs:
`build/logs/task-20260917-win-0653-resume/measure-{100,125}.log`.

**Method.** The uncommitted work-in-progress patch was applied: the menu
control and three 46-pixel caption buttons. The window is the one in
`test_strip_identity_stays_at_every_width`, with `microsoft/terminal #20607`
open (so Next new comment shows), offscreen at `QT_SCALE_FACTOR` 1.0 and
1.25. It was swept from 1200 to 900 one pixel at a time. At each width,
`apply_strip_width`'s existing yields ran first: the identity yields whole,
the positions below 960, and the modality segments compact. Then the four
modality buttons were set to `Ove`, `Rea`, `Rev` and `Dis`, and the
toolbar was remeasured. The overflow extension button was checked directly
from 903 down to 900.

**Result, in logical pixels, identical at 100% and 125%.**

| At the 900 floor | Compact labels | 3-character labels |
|---|---|---|
| Modality control width | 255 | 175 |
| Toolbar size hint (toolbar width 900) | 982 | 902 |
| Narrowest width with every control top level | 1027 | 902 |

With the 3-character labels the overflow extension button is hidden at 903
and 902, and **shown at 901 and 900**. The labels save 80 pixels but leave
the strip **2 pixels short at the floor, at 100% and at 125% alike**. The
card does not choose another yield.

**macOS.** The 3-character step would never be taken on macOS at or above
its floor. On this station, the same strip without the Windows controls
needs 806 pixels at 900 with the existing compact labels (726 with 3
characters). macOS's own `qt_workbench_window_scale_*` already pins "no strip
control folds into an overflow menu at the width floor" with the compact
labels. A yield taken only while the strip still overflows has nothing to do
there, so "macOS too?" does not matter.

### The ask

The owner's 3-character proposal, together with the identity's existing
yield, leaves the Windows strip **2 logical pixels** wider than the 900-pixel
floor, at 100% and at 125%. Every strip control stays top level from 902
pixels. **How should those 2 pixels be found on Windows?**

## Owner answer on the 2 pixels, 2026-09-17 (relayed verbatim by the coordinator, dsp-004)

The coordinator offered three options:

1. Raise the Windows minimum width to 902, and use the three-character
   modality labels (`Ove`, `Rea`, `Rev`, `Dis`) as the last yield.
2. Trim 2 pixels of spacing and keep 900.
3. Raise the minimum to 1030 and keep today's compact labels.

The owner answered:

> 902 is fine

**Decision: option 1, on Windows.**

- The Windows minimum width becomes 902 logical pixels.
- Below the width at which the compact labels no longer fit, the modality
  labels shorten to their three-character prefixes, as the last yield.
- Every strip control stays top level at the floor.
- The accessible names and tooltips keep the full words.

The station measured that the three-character step never triggers on macOS
at or above its floor, so the macOS question does not arise. macOS keeps 900.
Criterion 4's attended capture is still an owner task for later.

## Windows station, 2026-09-17 (task-20260917-win-0653-implement): criteria 1-3 done

The base is `20d9991412df1fb63dd0ad593ca801a4b87b415d`. The owner's answer above
is implemented. Only criterion 4, the attended capture, is owed.

### The width floor and the last yield

- The Windows minimum width is 902 logical pixels (`workbench_window.cpp`,
  `#if defined(_WIN32)`); macOS and Linux keep 900.
- `modality_control::set_abbreviated` shows `Ove`, `Rea`, `Rev` and `Dis`. It
  lives in the **portable** strip, and `apply_strip_width` takes it **only when
  the toolbar still asks for more width than the window after every existing
  yield** (the identity's title, its owner, the positions, the compact
  segments). A strip that fits never shortens, so macOS and Linux never take
  the step at or above their floor, which the portable window course now pins
  ("where the strip fits at every width the modality labels never shorten").
- The step is measured after the identity has been elided to its budget.
  Measured before it, the text the identity was about to give up counted as
  overflow and the labels shortened a step early.
- Only the visible text shortens. The segment's accessible name
  (`Overview modality`), its tooltip and its action text keep the whole word,
  which the course checks at the floor.
- At 1280 on Windows the identity now elides to a repository fragment beside
  its number, one step further along its own yield order than a strip without
  the Windows controls: the menu control and the caption buttons are about 180
  pixels the identity does not have. The width-sweep course states this per
  platform.

### The title row

- `windows_caption_buttons` (Windows only, `src/adapters/qt`) draws minimise,
  maximise-or-restore and close, 46 pixels each, in the strip's ink, with the
  platform's glyphs, Windows 11's washes and the close red, accessible names
  and descriptions, and strong focus.
  - Windows draws its caption buttons the row's full height and into the
    window's corner, so the top-right pixel is close. A toolbar item sits
    inside the toolbar's 4-pixel item margin instead, so the buttons are laid
    **over** the row's right end and a reserve (`reserve()`) takes their place
    among the toolbar's items: the strip's width accounting and its action
    order are unchanged. The buttons follow that reserve, and hide with it, so
    a window forced below its floor (a zoomed window on a small offscreen
    screen) never has them covering the items the row did lay out.
- The compact menu control opens the menu bar's File, Profiles and View menus,
  read from the bar each time; the bar is hidden and keeps the Quit shortcut
  live on the window.
- On Home the strip keeps both: the title row is the window's, not a pull
  request's.
- Focus order: the menu control first, then BPR-0646's order unchanged, and the
  three caption buttons as the strip's last three stops after Settings.

### The host (`src/app/src/windows_title_row_host.{hpp,cpp}`)

The core decides, the host performs, Qt presents. Presentation answers which
part of the row a point is on (`workbench_window::title_row_part_at`); the host
turns that into a platform code and performs what the part asks.

- **Frame.** `QNativeInterface::Private::QWindowsWindow::setCustomMargins` with
  `top = -(SM_CYCAPTION + SM_CYSIZEFRAME + SM_CXPADDEDBORDER)` at the window's
  DPI, and `-SM_CYCAPTION` while zoomed. **This is private Qt API**, pinned to
  the Qt the build provides (6.10.3) and to be rechecked on every Qt upgrade.
  `src/app` takes the two versioned private include directories from
  `Qt6Gui_DIR`, as the macOS native menu course does; nothing links a private
  module, and no `find_package` is added, which a product CMake file may not
  have.
  - Qt keeps the client's size when the margins change and moves the frame
    instead, so every maximise and restore took the frame's difference off the
    window's height. The host puts the window back where the platform placed
    it. The course pins the restored rectangle.
- **Hit test.** `WM_NCHITTEST`: bare strip surface `HTCAPTION`, the buttons
  `HTMINBUTTON`, `HTMAXBUTTON` and `HTCLOSE` (which is what offers the Windows
  11 snap layouts), every control `HTCLIENT`, the strip's top edge one logical
  pixel of `HTTOP`, and anything outside the client left to the platform, so
  the side and bottom resize borders, the shadow and the rounded corners
  remain.
- **Non-client hover and press.** `WM_NCMOUSEMOVE`, `WM_NCMOUSELEAVE`,
  `WM_NCLBUTTONDOWN`, `WM_NCLBUTTONUP` and `WM_NCLBUTTONDBLCLK` over the
  buttons are taken from the platform and reported to the presenter
  (`set_native_state`), so the platform draws no caption buttons of its own. A
  press released away from the button it started on does nothing.
- **Performing.** `WM_SYSCOMMAND` with `SC_MINIMIZE`, `SC_MAXIMIZE`,
  `SC_RESTORE` or `SC_CLOSE`, posted, so a keyboard activation of the same
  button does exactly what the system menu does. Dragging, double-click, Aero
  shake and edge snapping are the platform's own responses to `HTCAPTION`.
- Only the `windows` platform has a title bar to replace; on any other plugin,
  the offscreen courses included, the host is inert.

### Verification (Windows, bpr-win)

Windows 11 Home 10.0.26200, Visual Studio 18, Qt 6.10.3 `msvc2022_64`,
`windows-dev` Debug, display scale 150%.

- **New course `application_windows_title_row`**
  (`tests/end_to_end/windows_title_row_host_tests.cpp`), on the real `windows`
  platform: passes, 1.32 s. It checks the removed title bar, the strip as the
  first row, every hit code above, close in the corner at the row's full
  height, the left resize border, non-client hover and its clearing,
  double-click maximise with the strip's top row at the work area's top and the
  whole client inside it, `Restore` naming, double-click restore to the same
  rectangle, press-and-release on maximise, keyboard activation, minimise, a
  press released elsewhere doing nothing, and close.
  - It **passes contained**, so the registration needs no `WINDOWS_UNCONTAINED`
    pin. `SetCursorPos` is denied in the container, so the course hands the
    non-client move to the native event filters exactly as Qt's event
    dispatcher hands a queued one (`filterNativeEvent`); Qt does not run those
    filters for input sent straight to a window procedure (QTBUG-67095).
- `qt_workbench_window_scale_100`, `_125`, `_200`: pass (83.6 s, 86.2 s,
  88.5 s), including the pixel-by-pixel width sweep to the 902 floor with no
  control folded into an overflow menu.
- `test_network_isolation_manifest` and
  `test_network_isolation_structure_contract`: pass. `architecture_boundaries`,
  `architecture_boundary_contract`, `core_dependency_boundary`: pass.
- `application_diff_workbench_public_path`, `qt_home_list_scale_100`,
  `qt_review_queue`: pass.
- The repository's `format_check.py` (clang-format 22.1.8) is clean on every
  file this change adds, and on the three it changes that were clean before it.
  `workbench_window.hpp` and `composed_github_tests.cpp` were already outside
  the formatter's verdict; the lines added there follow the alignment around
  them and add nothing to the gap.

### The round-trip pins moved (not resealed)

`src/app/src/github_application.cpp` gained the host's construction, and the
application source tree gained two files, so two of the three pins in
`tests/end_to_end/application_review_round_trip_composition_contract.cmake` are
red. The station does not reseal; main reseals with review.

- `full_application`: `7735e76795c32275d91e1e04994b7f3efce1285db3fad86e68744e4abff31ce7`
- `production_app_manifest`: `cc4e8d9962f06e360a63cc7cf963325968fd686f26c1a96a0883b6b6cd158fbc`
- `production_mutation_branch`: `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`
  (unchanged)

The reviewed range is `20d9991412df1fb63dd0ad593ca801a4b87b415d..` this card's
commit. What changed inside a pinned scope: the `#if defined(_WIN32)`
construction of `windows_title_row_host` beside the window, its member
declaration, one include, and the two new `src/app/src/windows_title_row_host.*`
files. The source-tree and manifest inventories in the contract name those two
files; no digest was changed.

### For macOS at merge

The portable changes are in `src/adapters/qt`: `modality_control`'s
`set_abbreviated` and `relabel`, `workbench_strip::empty_surface_at`,
`apply_strip_width`'s last yield, and the Home kept-action names. Run
`qt_workbench_window_scale_100`, `qt_workbench_window_scale_125`,
`qt_workbench_window_scale_200` and `qt_composed_github_scale_100` on macOS.
Expected there: the floor stays 900, the labels never shorten, and the identity
keeps the repository at 1280.

### New defects found alongside, reported uncarded

All four are pre-existing at `20d99914` on this station and are not this
card's:

1. `qt_composed_github_scale_100/125/200` fail at "the code is in a fixed-pitch
   font at 1.6 times the row's size" (the device sign-in's code field). The
   same failure is in this station's `task-20260917-win-hardening` log, before
   any of this work.
2. `application_workbench_composition_contract` fails
   `WORKBENCH_DRAWER_WIDTH_WIRE_FAILED`, so the run expires its 40-second
   budget with `presentation=0`. Measured at `20d99914` with every change here
   reverted: it fails identically. It passed on this station in August.
   `application_composition_contract` fails only on the round-trip pins above.
3. `dependency_modernization_contract` fails because `find_package(` is in
   `tests/end_to_end/CMakeLists.txt:2199` (`find_package(Python3 ...)`), which
   its own rule forbids in a product CMake file. Pre-existing, and nothing this
   change added.
4. `qt_syntax_pane_composition` fails at "the line that opened the block
   comment has scrolled off the top". It uses the diff workbench, not the
   strip, and passed on this station in August.

### Criterion 4 is still owed

The attended capture steps are the ones recorded above under "Attended capture
(criterion 4), prepared for when the card resumes". They are ready and have not
been performed: they need the owner's signed-in session on JaegAW at 100% and
150% display scale.

## Merged to main with the macOS selectors run (dsp-005, integrator)

Merged from `windows-lane` `c6cac3f0` with a Mac run of the four selectors the
station named. **Two of them were red on macOS and are repaired here.** Both
were the same class of mistake: an expectation generalised from the one
platform the station can run.

**1. The identity's yield ladder (`qt_workbench_window_scale_100/125/200`).**
The station changed

```cpp
expect(text.contains("microsoft/"), "the owner yields after the title");
```

to `== (testCase.Repository && testCase.Title)`. That ties the owner's
presence to the title's, but the ladder drops the **title first and the owner
second**, so at 1280 off Windows the title has gone while the owner and the
repository both remain. The formula asserted the owner away one step early and
the row failed at all three scales on macOS with `FAILED: the owner yields
after the title`. The owner is now its own `width_case` field rather than a
derived expression, so Windows keeps exactly the station's expectation
(`false`) and macOS and Linux keep the behaviour the row asserted before
BPR-0653. `update_strip_identity` is unchanged by this card, so no product
behaviour moved.

**2. The zoom/unzoom double-click (`qt_workbench_window_scale_200` only).**
This one uncovered a **latent bug the station's change legitimately exposed**.
`empty_surface_at` gained `if (!rect().contains(aPoint)) return false;`, which
the Windows hit test needs and which is plainly right — a point the strip does
not contain is not its title-bar surface. Before that guard, an out-of-rect
point fell through `childAt() == nullptr` and read as bare surface, and **this
row had been passing by double-clicking outside the strip altogether.**

The measured state, from a diagnostic added to the assertion:

```
point 483,15  strip=398x30  spacer=426,4 115x22  maximized=1
```

The offscreen screen is 800x600 device pixels, so at 200% it is 400x300
logical. Maximising there gives a window narrower than the strip's own 900
floor; the strip necessarily overflows and offers no bare surface at all, and
the spacer keeps its pre-maximise `x` of 426 beyond the 398-wide toolbar.
`layout()->activate()` does not change that — the geometry, not the layout
pass, is the constraint. The row now proves the point is inside the strip
before using it; where the zoom is narrower than the floor it asserts that
sub-floor geometry explicitly and restores through the window's own state, so
it can never silently skip on a screen that could have hosted the gesture.

**Result: `qt_workbench_window_scale_100/125/200` and
`qt_composed_github_scale_100` all pass on macOS** (18.2, 18.1, 18.3 and
2.3 s), with the floor still 900, the labels never shortened and the identity
keeping the repository at 1280, exactly as the station predicted.

**Independent review: no blocker.** An `rfc-reviewer` read the whole lane diff
and confirmed at file and line that the three-character labels are unreachable
off Windows at or above the floor (`apply_strip_width` resets
`set_abbreviated(false)` unconditionally and takes the step only after the
identity has been elided to its budget, with a new portable pin asserting
`!abbreviated()` at every swept width), that no overlap of the caption buttons
over a laid-out item could be constructed (the reserve is the last toolbar
item and the buttons hide with it), that the abbreviation cannot reach an
accessible name (`relabel` touches only `setText`), that the maximise/restore
correction cannot accumulate drift (it restores the rect the platform just
produced, under an `AppliedTop` re-entry guard), that the host is inert under
any non-Windows plugin, and that the contract edit is inventory-only. Its
non-blocking findings:

- **Minimising a maximised window is not covered** by
  `application_windows_title_row`: `IsZoomed` is false while iconic, so the
  host reapplies margins on an off-screen rect and flips back on restore. Not
  shown to misbehave. **Added to criterion 4's attended steps below.**
- With the menu bar hidden, Alt+F/Alt+V mnemonics are gone and nothing opens
  the compact menu from the keyboard; VS Code binds Alt and F10. Worth a
  follow-up.
- `QKeySequence::Quit` has no Windows binding, so the card's "keeps the Quit
  shortcut live" is moot rather than wrong.
- The comment at `workbench_window.cpp` ~7461 describes room the identity can
  use, but the identity's visibility is already false wherever that branch
  runs, so the call is a no-op there.

**Reseal.** `src/app/src/github_application.cpp` and the two new
`windows_title_row_host.*` files move the full-application and production
manifest digests. Both are resealed in the merge commit on the two recorded
reviews, this one and [[BPR-0859]]'s. The mutation-branch pin still has never
moved. `application_review_round_trip_composition_contract` and
`application_composition_contract` both pass at the resealed head (11.1 s and
152.2 s).

**Open owner ask: private Qt API in a product target.** `src/app/CMakeLists.txt`
now takes QtCore's and QtGui's versioned private header directories for
`better_pull_review_composition` under `WIN32`, because frame removal goes
through `QNativeInterface::Private::QWindowsWindow::setCustomMargins`. The
precedent the station cited is real but **test-only**
(`tests/end_to_end/CMakeLists.txt` ~2602, the macOS native-menu probe); this
is the first *shipped* target to depend on private Qt headers. The integrator
recommends accepting it — there is no public Qt API for removing the title bar
while keeping the frame, and doing it by hand with `WM_NCCALCSIZE` and
`DwmExtendFrameIntoClientArea` fights Qt's own frame computation and is no
less version-coupled — but it pins the shipped app to the Qt the build
provides and obliges a recheck on every Qt upgrade, which is the owner's call.
It is confined to `windows_title_row_host.*`, that CMake block and the
`_WIN32` construction, so a refusal would be surgical and everything else
here would stand.

**Still open.** Criterion 4's attended capture on the Windows station, now
with a seventh step: maximise, minimise from the taskbar, restore from the
taskbar, confirm the window returns maximised with the strip at the work
area's top, then restore and confirm the original rectangle.

## Windows station on main, 2026-09-23 (bpr-main-win): re-verified, parked on the owner

Run `bpr-main-win-20260923`, milestone `windows-strip-title-row` of plan
`ux-redesign-pack-0028` (coordinator-implementer; checkpoint bound at
`4380f310`, definition `sha256:c2ae5fcd…`). Windows 11 Home 10.0.26200, VS 18
MSVC, Qt 6.11.2 `msvc2022_64`, `build/dev`, display scale 150%. **The
candidate is `53d229d7`**: its change was made and measured at `f62b745d` on
base `4380f310` and rebased onto `dbaf7040` without conflict. Logs are under
`build/logs/bpr-0653-main-win/` on this station and are not committed. No
attended or wizard session was started and no pointer was moved on the
owner's desktop.

### What the candidate adds

- **`application_windows_title_row`** now opens a review (two files, every
  command offered) and checks the Windows half of RFC-BPR-0028 AC-01 at
  **1280x852 and 1920x1080** (a restored window placed inside the work area;
  the run prints `title-row:1920x1080:ok`, so it did not fall back for want of
  room). At each size: no title bar; the strip is the first row, as wide as
  the window and 30 pixels; the client area begins directly beneath it with no
  menu bar row; the epoch line spans the window above the panes; the caption
  buttons sit in the strip, 46 pixels, the row's height, close ending it; bare
  surface answers `HTCAPTION` and the buttons `HTMINBUTTON`, `HTMAXBUTTON` and
  `HTCLOSE`. It **reports without judging** what stands between the strip and
  the epoch line (see "Found at main" below).
- The integrator's review finding and former attended step 7, now automated: a
  window **minimised while maximised** and restored as the taskbar does
  (`WM_SYSCOMMAND` `SC_RESTORE`) comes back maximised, the strip at the work
  area's top, the whole client inside it, the button named Restore; Restore
  from the keyboard then returns the original rectangle. A minimised restored
  window also returns to its rectangle.
- The menu control: it opens exactly the menu bar's visible menus in the bar's
  order (File, Profiles, Pull Request, View); its accessible description names
  each of them; and every menu action that carries a shortcut is also the
  window's, so its shortcut stays live while the bar is hidden.
- `workbench_window.cpp`, Windows branch: the menu control's description had
  gone stale ("The File, Profiles and View menus") when [[BPR-0990]] added the
  Pull Request menu. It is now built from the bar's menus.
- **`qt_workbench_title_row_scale_{100,125,200}`**, contained: the window
  suite's strip courses (`--title-row`: the exact order with the menu control
  first and the caption buttons last, the pixel-by-pixel width sweep to the
  902 floor with the three-character labels, the bare-surface gestures) run
  directly, past the epoch-row assertion [[BPR-0940]] owns, as BPR-0981,
  BPR-0985 and BPR-0990 did for their courses. No uncontained registration is
  added, so nothing needs pinning.

### The Qt 6.11.2 recheck of the private API

`92c4e159` upgraded Qt to 6.11.2 from the Mac and moved the host's pin comment,
which obliges a recheck on Windows. `application_windows_title_row` on the
6.11.2 build passed unchanged at `4380f310` before anything here was touched
(1.67 s), and passes at the candidate. The recheck is done; the owner's
decision on shipping private Qt API (below) is not.

### Verification

At `f62b745d`, `ctest --preset dev` over nineteen selectors, 14 pass:
`application_windows_title_row` (1.91 s), `qt_workbench_title_row_scale_100`,
`_125`, `_200` (14.1, 18.2, 18.8 s), `qt_workbench_focus_order_scale_100`,
`_125`, `_200` (the caption buttons are the strip's last three stops),
`test_network_isolation_structure_contract`, `architecture_boundaries`,
`core_dependency_boundary`, `application_review_round_trip_composition_contract`,
`application_diff_workbench_public_path`, `qt_home_list_scale_100`,
`qt_review_queue`. Five fail, none on anything this card changed:

- `qt_workbench_window_scale_100`, `_125`, `_200` stop at "the epoch row is the
  first row of the client area and as wide as it" ([[BPR-0940]]), after their
  strip courses have passed.
- `qt_composed_github_scale_100` stops at [[BPR-0940]]'s "with GitHub CLI
  chosen the card speaks for it…".
- `test_network_isolation_manifest` names eleven uncontained rows that are not
  in the pinned Windows set: `package_runtime_contract`,
  `release_source_assets_contract`, `release_package_audit_contract`,
  `release_security_audit_contract`, `sqlite_release_maintenance_files`,
  `modified_endpoint_fixture_contract`, `application_logging`,
  `application_selected_file_safety_public_path`,
  `application_attended_recent_replay`,
  `application_release_maintenance_public_path` and
  `application_release_update_public_path`. None is this card's; the same kind
  of drift [[BPR-0812]] cleared. A card for it is requested from the
  coordinator.

At the rebased candidate `53d229d7`, the title-row course, the three title-row
and three focus-order rows, the structure contract and `architecture_boundaries`
pass again. The manifest names the same eleven rows.
`application_review_round_trip_composition_contract` is now red on main
because `f863ce1b` ([[BPR-0975]]) changed `github_application.cpp` without a
reseal. That is not this card's either, and it is reported to the coordinator.

clang-format 22.1.8 is clean on the course and on the changed regions of the
other two sources; `tidy.py --warnings-as-errors` is clean on
`windows_title_row_host_tests.cpp` (after two helper renames and one handle
helper) and on `workbench_window.cpp`; `git diff --check` is clean.

### Found at main, not this card's: a row between the strip and the epoch line

RFC-BPR-0037's `cc038748` ([[BPR-0929]]) inserted a composition entry row,
Compose review and Draft comment or reply, into the review surface **above**
the epoch line (`review->insertLayout(1, …)` in `build_central_surface`). The
course measures it at both sizes: `composeReviewActionButton@30+20` and
`composeInlineDraftActionButton@30+20`, 20 logical pixels between the strip's
bottom (30) and the epoch line (50). It is on every platform; it is the
failure [[BPR-0940]] records.

It conflicts with two owner-accepted statements: RFC-BPR-0028 AC-01, "the
chrome above the comparison is the strip and the epoch line only", and the
owner's [[BPR-0737]] direction that the epoch line is the row directly under
the strip. The RFC-BPR-0037 layout study itself put the Compose review entry
"in toolbar" (`conversation-surface-layout-study.md`, its Read row), not in a
row of its own. The strip cannot simply take two more controls: on Windows it
fits at its 902 floor with nothing to spare, which is how the owner chose
that floor. So this is the owner's call, between:

1. amend RFC-BPR-0028 AC-01 and the chrome paragraph to admit the composition
   entry row, and have [[BPR-0940]] assert its place; or
2. keep AC-01 and move the two entries out of that row, for example into the
   strip's existing review controls menu, which costs the strip no width.

Criterion 1's portable half cannot pass as written until that is decided.

### Where each criterion stands

- **Criterion 1**: the Windows form passes at both sizes (no title bar, the
  strip first with the caption buttons in it, the client directly beneath it,
  the epoch line across the window). The portable "strip and epoch line only"
  fails on main because of the composition entry row above. Pending the
  owner's AC-01 decision.
- **Criterion 2**: double-click maximises and restores the same rectangle,
  passes on the real platform. Snap layouts on hover and dragging by empty
  strip space are the platform's own responses to the codes the course proves
  (`HTMAXBUTTON`, `HTCAPTION`, restored and maximised, at both sizes). Seeing
  them happen needs a real pointer on the owner's desktop, so they ride the
  attended capture. Pending.
- **Criterion 3**: the title-row course and the title-row and focus-order rows
  pass at every scale, and no uncontained registration is added. The full
  `qt_workbench_window_scale_*` rows and the manifest gate are red on main for
  the reasons above, none of them this card's. Pending.
- **Criterion 4**: the attended capture has not been taken. Pending.

### What the owner must do

1. **The attended capture (criterion 4)** on JaegAW, signed in, at 100% and at
   150% display scale, from a tests-disabled `app-release` bundle built at the
   candidate when the session is prepared: steps 1 to 6 of "Attended capture
   (criterion 4)" above. Step 7 is now covered by the course and may be
   skipped. What to look at: no system title bar; the strip is the top row
   with the menu control at its left and minimise, maximise and close at its
   right, drawn in the strip's ink; hovering maximise for about a second shows
   the Windows 11 snap layout flyout under it; dragging empty strip space moves
   the window and releasing it against the left screen edge snaps it;
   double-click maximises and restores; maximised at 1920x1080 the strip's top
   row is not cut off.
2. **Private Qt API in a shipped target**, the integrator's open ask above:
   accept `QNativeInterface::Private::QWindowsWindow::setCustomMargins`
   (recommended; rechecked on Qt 6.11.2 here), or refuse it.
3. **The composition entry row**, above: amend AC-01, or move the entries.

The card parks on human-review with the candidate `53d229d7`.

## Re-check on main, 2026-09-23 (bpr-main-win, second pass): the AC-01 ask restated

This was card upkeep in run `bpr-main-win-20260923`, not a new candidate; the
candidate is still `53d229d7`. Main was at `38cc26d7`, after [[BPR-0940]]
finished at `d2ee90cb`, and `build/dev` was rebuilt there. The coordinator
cleared foreground runs for this check alone:

- `application_windows_title_row` took the real platform from 17:11Z to
  17:16Z, and passed;
- the window rows run offscreen.

No pointer was moved, and no attended or wizard session was started. The logs
are under `build/logs/bpr-0653-main-win/` on the station.

### "Found at main", checked against `38cc26d7`

| What the note above says | At `38cc26d7` |
|---|---|
| The composition entry row, Compose review and Draft comment or reply, sits between the strip and the epoch line, 20 pixels at y 30 | **Still true.** `build_central_surface` lays out the readiness line (shown only while a pull request loads), then the entry row (`review->insertLayout(1, …)`), then the epoch line, then the splitter. On the real platform the course prints `windows-title-row-host-tests:between-strip-and-epoch-line:1280x852: composeReviewActionButton@30+20 composeInlineDraftActionButton@30+20`, and the same at 1920x1080. |
| `qt_workbench_window_scale_{100,125,200}` stop at "the epoch row is the first row of the client area" | **No longer.** [[BPR-0940]] (`f839e26a`) reworded that assertion and changed its target. The epoch row must now sit directly beneath the review surface's own entry rows, meaning the two entries and the readiness line, and nothing else may come between the strip and it. On Windows all three rows pass it, and then abort later, in the Discuss course (below). |
| The row is "the failure BPR-0940 records" | **Superseded.** BPR-0940 closed by reading the old "first row" wording as superseded by the owner's accept of the native candidate `rfc0037-compose-pages`. That accept judged the composition pages; it did not amend RFC-BPR-0028 AC-01 or its chrome paragraph. |

So the conflict remains, but no test is red over it now. It is between
the RFC's text and main.

### Owner ask 3, restated (it replaces item 3 of "What the owner must do")

> RFC-BPR-0028 AC-01 says the chrome above the comparison is "the strip and
> the epoch line only", and the chrome paragraph puts the epoch line directly
> below the strip. On main, every review has a third row between them: Compose
> review and Draft comment or reply (RFC-BPR-0037, 20 pixels). While a pull
> request loads, the loading line is there too. Since BPR-0940, the window tests
> assert this arrangement. Should AC-01 and the chrome paragraph be amended to
> admit those entry rows? Or should the two entries move, for example into the
> strip's review controls menu (which costs the strip no width), with the tests
> returning to "the epoch line is the first row under the strip"?

Asks 1 (the attended capture) and 2 (Qt's private Windows native interface)
are unchanged.

### Criterion 3 on main

At `38cc26d7`, and at `fb4357b1` for the rows that do not take the
foreground, these pass:

- `application_windows_title_row` (1.94 s);
- `qt_workbench_title_row_scale_100`, `_125` and `_200` (14.1, 16.3 and 31.1 s);
- `qt_workbench_focus_order_scale_100`, `_125` and `_200`;
- `test_network_isolation_manifest`, green since [[BPR-1020]] (`fb4357b1`)
  pinned or contained the eleven rows the earlier pass named;
- `test_network_isolation_structure_contract`, `architecture_boundaries`,
  `core_dependency_boundary`, `application_review_round_trip_composition_contract`
  (resealed on main since the last pass), `application_diff_workbench_public_path`,
  `qt_home_list_scale_100` and `qt_review_queue`.

These are red on main, and none of them is this card's:

- `qt_workbench_window_scale_100`, `_125` and `_200` abort with `0xc0000409`
  after about 60 s. The cause is an MSVC debug-iterator assertion, "can't
  dereference invalidated vector iterator", in
  `test_discuss_uses_main_content_and_open_in_code_returns_to_read`. A
  temporary, reverted course trace named that course.
  `qt_workbench_conversation_reveal`, which runs the same course, aborts
  identically.
  - The likely cause is in the test itself. It keeps `sameFile`, an iterator
    into `conversation_cards_view::cards()`, across the off-file reply press
    and `process_events()`. `relayout()` clears and rebuilds that vector.
  - The earlier epoch-row stop hid this from the full window run on Windows.
  - A card is requested.
- `qt_composed_github_scale_100` now passes BPR-0940's former stop and fails at
  "the code is in a fixed-pitch font at 1.6 times the row's size", which is
  [[BPR-0869]].

### Also carded from this card's asks

- [[BPR-1021]] closes RFC-BPR-0028. It is in Backlog on this card and
  [[BPR-0746]], under the new plan milestone `close-rfc-0028`.
- [[BPR-1022]] opens the strip's application menu from the keyboard with Alt and
  F10. That is the integrator's non-blocking review finding above, and it is
  standalone.

The card stays parked on human-review with candidate `53d229d7`. All three
owner asks are open.

### The strip menu's opening path changed under [[BPR-1028]] (bpr-main-win, 2026-09-23)

The Application menu used to be a stack `QMenu` run with `exec()`. An
accessible File > Quit destroyed the window inside that menu's loop, and the
Debug CRT asserted. Since `e08a0409` the menu is a heap child of the window
that deletes itself on close, opened with `popup()`
(`workbench_window::open_strip_menu`). It has the same contents, the same
object name, the same position under the control and the same accessible
names. A person sees no difference.

`application_windows_title_row` no longer relies on `exec()` blocking. It
presses the control, waits for the popup, reads it and closes it, and it also
checks that the closed menu deletes itself. So this card's real-platform
evidence has to be re-run in the new form, which is queued with BPR-1028's
foreground runs. The candidate the owner will capture should be at or after
`e08a0409`.

## Owner decisions, 2026-09-24 (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
handed back at sequence 589. Both verdicts below are `actor: human` and carry
their prepared candidate's fingerprint, so each is bound to exactly what was
shown. Asks 2 and 3 are answered; ask 1 is not.

**Ask 3, the entry rows.** Candidate `BPR-0653-entry-row-v001`, revision
`38cc26d7` (ask restated at `919e2154`), fingerprint `308902012d73…`, decision
`007a6eb5-adf3-427c-8473-547f9d0c4d9e`, sequence 569, 2026-09-24T18:54:09Z:
**select**, verbatim:

> Amend RFC-BPR-0028 AC-01 and the chrome paragraph to admit the entry rows
> between the strip and the epoch line

Note, verbatim:

> We should do a UX design iteration with the /frontend-design skill to figure
> out where best to put these. For now you can ammend the card but open a
> follow-up for the ux design.

Applied as an owner amendment:

- RFC-BPR-0028 AC-01 and its chrome paragraph now admit, between the strip and
  the epoch line, the review surface's entry rows: the composition entry row
  (Compose review, Draft comment or reply) and, while a pull request loads, the
  readiness line. Nothing else may sit there.
- This card's criterion 1 is amended to the same words, and **passes** on the
  evidence already taken: `application_windows_title_row` measured exactly
  the admitted row between the strip and the epoch line at both sizes at
  `38cc26d7`, and passes again in [[BPR-1028]]'s popup form at `2a5580d6`.
- The design iteration is [[BPR-1106]], a standalone follow-up. It does not
  hold RFC-BPR-0028's closeout. If it moves the entries, it amends AC-01 again.
- The same answer decides [[BPR-1036]]'s height floor (that card is now
  Ready), and through it [[BPR-1033]]'s AC3.

**Ask 2, Qt's private Windows native interface.** Candidate
`BPR-0653-private-qt-api-v001`, revision `53d229d7` (Qt 6.11.2 recheck at
`4380f310`), fingerprint `da03ec26…`, decision
`8dde5e4d-6edb-403f-9d55-05777ef60250`, sequence 570, 18:54:24Z: **accept**,
note empty. `QNativeInterface::Private::QWindowsWindow::setCustomMargins` may
ship in the Windows target. The accept covers the API as rechecked on Qt
6.11.2; a later Qt upgrade still owes the recheck the host's pin comment names.

**Ask 1, the attended capture: not asked.** `BPR-0653-windows-capture-v001`
waits for a tests-disabled `app-release` bundle built on JaegAW at or after
`e08a0409`, which the Mac preparation could not build or verify. Criteria 2
and 4 wait on that capture. Criterion 3 stays pending on the Windows window
rows recorded above. The card stays parked on human-review, now for this one
ask.

## The capture bundle was not built, 2026-09-24 (bpr-main-win)

The coordinator asked the Windows station to build the tests-disabled
`app-release` bundle for ask 1. The build was to be at or after `e08a0409`,
using the `windows-app-release` preset. First, the station was to judge from
[[BPR-1110]] whether Defender would quarantine the bundle, and not build it
if so. **It was not built.** Nothing was launched, and no image or digest
exists for the capture.

Why, from BPR-1110's findings:

- **The one recorded first launch of such an image was quarantined.** That
  was the tests-disabled Release image at `2d3bdbdf`, on 2026-09-22.
  Defender's cloud sent a signature made for that file about three seconds
  after the launch. Defender quarantined both copies with it, while a course
  was driving the window.
- **The file itself was not what changed.** The file had sat on disk for three
  minutes undetected. Structurally identical siblings, BPR-0014 M8 and M9,
  were never flagged, and as far as recorded they were never launched. So
  building the bundle would very likely pass. Launching it, which is step 1
  of the capture, is the moment the same machine-learning verdict is likely
  to come.
- **A fresh build keeps the profile the verdict rests on.** It is unsigned,
  has no version information and no reputation, carries 3.6 MB of
  near-random resource data, and imports credential, ACL and process calls.
  Nothing has changed since then that would lower the score.
- **A launch here would be a substitute for the owner's decision.** The
  likely outcome is a new Severe detection on the owner's machine in the
  middle of the capture. The owner asked to understand the detection before
  deciding anything about it: [[BPR-0974]] withheld a fresh tests-disabled
  image for that reason.

What would let ask 1 go ahead. These are the owner's choices, with BPR-0974's
disposition:

- signing the Windows image;
- authorizing a fresh tests-disabled build and its launch, accepting that
  Defender may quarantine it mid-capture;
- a false-positive submission;
- amending criterion 4 so the capture may use the tests-enabled Debug build.
  That build draws the same title row through the same host code, and has
  never been flagged on this host.

Until then, criteria 2 and 4 stay pending, and the card stays parked on
human-review.

## BPR-0974's disposition arrived, 2026-09-25

The owner answered [[BPR-0974]]'s disposition in attended session
`7083474240820608cdf9ff551ddd298f` (decision `5c30287a`): lower the static
signals (a `VERSIONINFO` resource, a PDB reference in Release, and the 3.6 MB
of resource blobs moved out of the executable) and go ahead, with Authenticode
signing later. The verdict and its limits are on BPR-0974, which is Ready for
bpr-main-win. None of this card's four options above was chosen for it
directly. What it changes here: once BPR-0974's changes land, a
tests-disabled bundle built at a commit that contains them is the image ask 1
can use. If Defender flags it, the capture stops and the detection is recorded
on BPR-0974. Criteria 2 and 4 stay pending, and the card stays parked on
human-review.

## The capture image, 2026-09-25 (bpr-main-win): ready for the attended capture

[[BPR-0974]]'s three changes landed at `85e522d8`, which also carries this
card's title row. A tests-disabled `windows-app-release` image built from it
passed its single cleared first launch at 2026-09-25T02:21Z:

- the window came up after 0.8 s;
- `WM_CLOSE` gave exit 0;
- no Defender detection and no operational event followed;
- all its files are still present.

The record is on BPR-0974. This is the image for ask 1 and the attended capture
of criteria 2 and 4, left for the integrator:

| File | SHA-256 |
|---|---|
| `C:\Users\mikem\bpr0974-candidate-85e522d8\installed\better_pull_review.exe` | `6aa27c8de85604d38a04c8728c610d2afb1fbcbd541e489bdedcc3891d798ac4` |
| `C:\Users\mikem\bpr0974-candidate-85e522d8\installed\better_pull_review.rcc` | `ca3bc2b191085ef61a5225369565376e163b4efb80920b2b52e72e68bf048f91` |

- **Launch it from `installed\` only.** It is a distribution build: it refuses
  to start without its `plugins\` directory and its resource bundle beside it.
  The build copy in `b\src\app\` has neither.
- **Its first launch used a private temporary profile** (`--render-evidence`).
  An ordinary launch uses the default profile under `%LOCALAPPDATA%`.
- **One launch says nothing about the next.** Defender's verdict is per file,
  so a later launch of these same bytes is still the only check. If a launch is
  flagged, the capture stops and the detection is recorded on BPR-0974.

Criteria 2 and 4 stay pending, and the card stays parked on human-review.

## The capture, staged for the station, 2026-09-25 (bpr-main-win)

The owner cannot reach JaegAW, so the station takes criteria 2 and 4's
captures itself, and the owner reviews them remotely. The capture is the
script [`windows_caption_capture.ps1`](../../../tests/scripts/windows_caption_capture.ps1).
It was prepared headless and has not run: it needs a cleared foreground
window, after [[BPR-0974]]'s AC2 and AC3 runs and before any rebuild.

**What it captures**, from `installed\` only, with the image's and the
bundle's SHA-256 checked first. It runs with `--render-evidence` and no
value: signed out, on a private temporary profile, so no account and no
repository content can appear. At the display scale Windows is set to,
never changed:

1. the three caption buttons at rest;
2. each one hovered;
3. each one pressed and held;
4. the snap layouts flyout, from hovering Maximize;
5. a drag of empty strip space with real pointer input (`SendInput`):
   before, mid-drag with the button held, and after;
6. the maximized state, after a click on Maximize;
7. the restored state, after a double-click on empty strip space. That is
   criterion 2's "double-click maximises and restores".

**Pointer rules**, from the coordinator, and how the script keeps them:

- **The pointer is saved first and restored at the end.** It moves only
  inside the window.
- **Every move and every press has a guard.** The window must be in front,
  it must be the window under the target point, and the point must answer
  the expected `WM_NCHITTEST` code: `HTMINBUTTON`, `HTMAXBUTTON`, `HTCLOSE`
  or `HTCAPTION`. The script asks the window itself, which has no side
  effect. If any check fails, the capture stops.
- **A press never ends over Close or Minimize.** The title row host
  (`windows_title_row_host.cpp`) acts only on a release over the same part
  that was pressed. So the script moves the held button onto empty strip
  space before releasing it. There the release is the platform's no-op.
  A button still held when anything fails is taken to that point, or to the
  middle of the window, before its release. The one release on a button is
  step 6's click on Maximize.
- **The drag stays small.** 160 by 120 physical pixels, towards the
  monitor's middle, inside the work area.

**What it writes.**

- **Images.** Each image is the window's visible frame only (DWM's extended
  frame bounds), never the desktop. The full-window PNG is downscaled to at
  most 1920 pixels wide; the strip and the caption-button area are also
  saved at full resolution.
- **`index.json`.** It records each state's rects, DPI and scale, the
  pointer, the time and every image's SHA-256. Other windows are listed by
  class and rect only, because another application's title may be private.
- **Afterwards.** The images are looked at before any is committed, under
  `docs/verification/BPR-0653-<date>/` with a short index.

**Checked headless.**

- The script parses.
- Its dry run against the candidate binds both digests and runs per-monitor
  DPI aware, so rects, points and pixels are physical. It reads the display
  at DPI 144, which is 150%, the scale under review, so no
  `QT_SCALE_FACTOR=1.5` set is needed.
- A dry run with a wrong bundle digest is refused, and creates no folder.

Criteria 2 and 4 stay pending, and the card stays parked on human-review,
now for the owner's remote review of these captures once they exist.

## The capture, taken 2026-09-25T04:29:54Z to 04:30:23Z (bpr-main-win): ready for the owner's remote review

The station took criteria 2 and 4's captures itself, in a foreground block the
coordinator cleared, with
[`windows_caption_capture.ps1`](../../../tests/scripts/windows_caption_capture.ps1).
The images, their index and every file's SHA-256 are in
[BPR-0653-2026-09-25](../../verification/BPR-0653-2026-09-25/index.md).

**What ran.**

- One launch of [[BPR-0974]]'s cleared candidate from `installed\`: image
  `6aa27c8d…` and bundle `ca3bc2b1…`, both verified by the script first. It
  ran with `--render-evidence`, so it was signed out, on a private temporary
  profile (`AUTOMATED_PROFILE_PRIVATE_OK`). PID 36952; exit 0 after
  `WM_CLOSE`.
- The display at its own scale: DPI 144, which is 150% on the 3840x2160
  panel. The scale was not changed and not overridden.
- All 13 states were captured, 31 guarded gestures in all. Each move and
  press checked three things first: the window in front, the window under
  the point, and the point's own `WM_NCHITTEST` answer. Every pressed button
  was released on empty strip space, and nothing fired: the window was still
  up and not minimized after each release, and it closed only when told to.
- The pointer was restored to (1895, 699). Defender recorded nothing; the
  record is on BPR-0974.

**What the images show,** for the owner to judge:

- **Hover and press.** The three caption buttons at rest, hovered and
  pressed: a grey fill for Minimize and Maximize, lighter when pressed, and
  red for Close.
- **Snap layouts.** The flyout appears on hovering Maximize, and stays while
  it is held.
- **The drag.** Dragging empty strip space with real pointer input moved the
  window by exactly the pointer's travel, (+160, +120) physical pixels.
- **Maximize and restore.** A click on Maximize maximizes, and the glyph
  becomes Restore. A double-click on empty strip space restores to the same
  frame.

**Looked at, all 39 images.** Every one shows only the app, signed out, with
no account, repository or private content, and no desktop or taskbar. In four
window images, other applications' content showed inside the window's rect,
and the committed copies have it filled with the surrounding colour:

- `hover-maximize`, `snap-flyout` and `pressed-maximize`: the snap flyout's
  recent snap groups showed two other applications' icons (Windows Terminal
  and an OpenAI app);
- `maximized`: another application's notification toast.

The rectangles are in `capture.json`, and the unredacted originals stay as
station records, not committed.

**Two capture-script defects, found and fixed before the capture that ran.**

- The pointer helper was named `Move`, which PowerShell resolves to
  `Move-Item`'s alias before any function.
- Its `INPUT` structure carried padding that made it 48 bytes instead of
  x64's 40, so `SendInput` refused every call.

The first attempt (PID 46236, 04:28:11Z) stopped on the first before any
gesture. The pointer never moved.

**Where the card stands.** Criteria 2 and 4 now have their captures. The card
stays parked on human-review for the owner's remote review of them. Criterion
3 is unchanged.

## The owner's remote review, prepared 2026-09-25 (main, Mac)

The station's capture is in front of the owner as one question in a new,
scoped attended session. No earlier item is on the page.

- Session `df5859c784ba447497026d6cd81af810`, state
  `/Users/jaeger/.mask/attended-review/owner-decisions-20260925-bpr0653-v001`
  (external, not committed), ready at sequence 4. Packet:
  [owner-decisions-20260925-bpr0653-v001](../../verification/reviews/owner-decisions-20260925-bpr0653-v001/operator-packet.md).
- Candidate `BPR-0653-windows-capture-150-v001`, revision `727ad9d7` (capture
  of image `6aa27c8d…98ac4`, source `85e522d8`), fingerprint
  `b086dd44326bf32cef5d0703988972e3cd5839786c5cc2434ae66c009e39e775`.
  It carries all 39 images, and each one hashes to `capture.json`.
- The one question: accept the Windows caption and strip visuals as captured
  at 150%, reject with a note, or Other (write-in).
- The item says plainly that the captures are station-driven, not
  owner-driven, and that 100% was not captured: changing the display scale is
  a system setting, and it is deferred. It also lists what is not pictured:
  edge snap, a pull request open, the 1920x1080 maximized size, and the
  maximizing double-click. The course asserts that double-click.

Unless the owner amends the criteria, **100% stays owed**. An accept covers
150% on this exact capture only. Criteria 2 and 4 stay pending until the verdict is recorded
here. Criterion 3 is unchanged. The card stays parked on human-review.

## The owner's verdict on the 150% capture, 2026-09-25 (main, Mac)

Source: attended session `df5859c784ba447497026d6cd81af810`, state
`/Users/jaeger/.mask/attended-review/owner-decisions-20260925-bpr0653-v001`
(external, not committed), packet
[owner-decisions-20260925-bpr0653-v001](../../verification/reviews/owner-decisions-20260925-bpr0653-v001/operator-packet.md).
The owner handed back at sequence 6. The helper's `status --after 4` reads
the verdict back as below: sequence 5 is the decision and sequence 6 the
hand-back, with nothing left pending.

- Candidate `BPR-0653-windows-capture-150-v001`, revision `727ad9d7`: the
  station's capture of image `6aa27c8d…98ac4`, bundle `ca3bc2b1…8f91`, source
  `85e522d8`.
- Fingerprint
  `b086dd44326bf32cef5d0703988972e3cd5839786c5cc2434ae66c009e39e775`, the one
  prepared at sequence 4, so the verdict is bound to exactly what was shown.
- Decision `1aba8610-6768-408a-a0a1-e1613c62c89f`, saved
  2026-09-25T05:07:37Z, `actor: human`. **Selected**, verbatim:

  > Accept the Windows caption and strip visuals as captured at 150%

  The note is empty.

### What the verdict covers

**Criterion 4.** The 150% half is taken and accepted. The capture shows the
strip as the title row, with Minimize, Maximize and Close at rest, hovered and
pressed, and the snap layouts flyout, at DPI 144. The accept covers that
capture only, with the limits its question stated:

- the station drove it with `windows_caption_capture.ps1`, not a person;
- the window was on Home, signed out;
- edge snap, the 1920x1080 maximized size and the maximizing double-click are
  not pictured.

**Criterion 2.** At 150% each of its clauses is now observed:

- the snap layouts flyout on hovering Maximize (accepted);
- a real-pointer drag of empty strip space moving the window by the pointer's
  travel, (+160, +120) physical pixels, in `capture.json` (accepted);
- a double-click on empty strip space restoring the same frame (accepted);
- the maximizing double-click, from `application_windows_title_row` on the
  real platform.

The verdict amends nothing.

### Why the card does not finish with 100% as a residual

No criterion and no owner amendment allows it:

- The card binds criterion 4's capture at **100% and 150%**. That is in the
  criterion's recorded evidence, in "Attended capture (criterion 4)" and in
  "What the owner must do". Criterion 2's snap and drag observations ride
  that capture.
- The question told the owner, before the answer, that 100% "stays owed on the
  card whatever the answer". The preparation section above says the same:
  unless the owner amends the criteria, 100% stays owed.
- The verdict accepts "as captured at 150%", with no note. It does not amend
  the criteria.

So criteria 2 and 4 stay `pending`, each with its 150% half accepted.

**Criterion 3 is also still pending.** Both reds it recorded are fixed:

- [[BPR-1029]] (`8edd961a`): the window rows and
  `qt_workbench_conversation_reveal` pass on the Windows station.
- [[BPR-0869]] (`1eab866e`): the composed rows pass on Windows.

`workbench_window.cpp` has changed since those rows last passed on Windows
(`785a6828`, [[BPR-1022]], among others). No single Windows pass covers the
criterion's selectors together with the manifest gate.

### The session is closed

- **The host is stopped.** `stop` returned `{"stopped": true}`, and PID
  75589 is gone.
- **The protection is released.** `resume-development --candidate
  BPR-0653-windows-capture-150-v001` released it at sequence 7. The decision
  stays on the candidate, and `conflicts` over the capture folder, the packet,
  this card and [[BPR-1021]] is empty.
- **The packet is unchanged.** Its bytes are what the owner was shown, as with
  the earlier packets.

### What remains, exactly

1. **The capture at 100% display scale** (criteria 2 and 4):
   - **Owner first.** Set JaegAW's display scale to 100%. It is a system
     setting, so the station does not change it.
   - **Then the station.** Run `windows_caption_capture.ps1` at DPI 96. Use
     the same image, `6aa27c8d…98ac4` with bundle `ca3bc2b1…8f91`, so that
     both halves picture the one title row the owner accepted at 150%. Each
     launch is its own Defender check: if one is flagged, the capture stops
     and the detection goes on [[BPR-0974]].
   - **Then the owner** reviews that capture.
2. **Criterion 3.** One unattended station pass at a single current main
   revision. It runs the selectors this criterion names and
   `test_network_isolation_manifest`, and needs no owner.

The card stays parked on human-review, now for the 100% capture only.
Criterion 3's station pass is independent unattended work. Only the owner can
shorten this, by amending criteria 2 and 4 to the accepted 150% capture. No
such amendment has been made.

## Criterion 3 at one main revision, 2026-09-25 (bpr-main-win)

One unattended station pass at a single main revision, `3da31f2a`. It runs
every selector this criterion has named and `test_network_isolation_manifest`.
[[BPR-1129]] had not landed: its card was in Ready at that revision and on
`origin/main`.

- **The station.** Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2,
  display scale 150%.
- **The build.** `build/dev` was rebuilt at `3da31f2a` and printed no
  compiler warning. The tree was exactly `3da31f2a`, with nothing uncommitted.
- **The real-platform row.** `application_windows_title_row` ran on the
  `windows` platform at 05:19:53Z to 05:19:55Z. Before and after it, Windows
  Terminal was in front, and there was no WSLg (`msrdc`) window.
- **The other rows.** They ran headless, serially (`ctest -j1`), at 05:20:00Z
  to 05:27:48Z.

| Row | Result |
| --- | --- |
| `application_windows_title_row` | pass, 1.84 s. Prints `title-row:1280x852:ok` and `title-row:1920x1080:ok`. Between the strip and the epoch line are only `composeReviewActionButton@30+20` and `composeInlineDraftActionButton@30+20` |
| `qt_workbench_title_row_scale_100`, `_125`, `_200` | pass, 14.22, 16.19 and 18.06 s |
| `qt_workbench_focus_order_scale_100`, `_125`, `_200` | pass, 0.86, 0.86 and 0.85 s |
| `qt_workbench_window_scale_100`, `_125`, `_200` | pass, 92.63, 96.24 and 98.50 s |
| `qt_workbench_strip_menu` | pass, 14.34 s |
| `qt_composed_github_scale_100` | pass, 39.67 s |
| `qt_home_list_scale_100` | pass, 18.97 s |
| `qt_review_queue` | pass, 0.04 s |
| `application_diff_workbench_public_path` | pass, 21.79 s |
| `application_review_round_trip_composition_contract` | pass, 6.76 s |
| `test_network_isolation_structure_contract` | pass, 3.02 s |
| `architecture_boundaries` | pass, 18.40 s |
| `core_dependency_boundary` | pass, 0.44 s |
| `test_network_isolation_manifest` | **fail**, 5.43 s. It prints exactly [[BPR-1108]]'s twelve findings, and nothing else (below) |

So 19 of the 20 rows pass, the title-row course among them. Only the manifest
is red.

**The manifest's twelve findings.** The output has 12
`TEST_NETWORK_ISOLATION_FAILED` lines, then the runner's exit lines. There are
three lines for each of the four rows [[BPR-1108]] left unpinned:
`sqlite_release_maintenance_files`, `application_attended_recent_replay`,
`application_release_maintenance_public_path` and
`application_release_update_public_path`. Each row has the same three
findings:

- "uses bpr_windows_uncontained but is not in the pinned Windows set";
- "does not start with […better_pull_review_network_sandbox.exe]";
- "lacks the bpr_network_isolated label".

No other row is named. [[BPR-1108]] records this as its outcome, not as a
regression: the row stays red until those four run contained. None of the
four is this card's, and this card adds no uncontained registration.

**Where criterion 3 stands.** Every focused Qt selector passes at one main
revision. The manifest gate does not: it is red, and only on [[BPR-1108]]'s
twelve findings. The criterion says "the manifest gate pass[es]", and it was
written before [[BPR-1108]]. So the criterion stays `pending`. It is met if
the owner or the coordinator accepts the manifest's red for those four rows
as outside this criterion, or once those rows run contained. Nothing else is
owed for it.

The card stays parked on human-review, for the 100% capture (criteria 2
and 4). The logs are station records under
`build/logs/bpr-0653-main-win/c3-3da31f2a/`, and are not committed.

## Not carried into the 2026-10-03 session

`BPR-0653-windows-capture-v001` stayed in the closed session
`cbd48b91000b00f1e72e1ad62c7c384c`, waiting and undecided. It is **not** in the
new short session `f18e0aba4e53e518cd4f57a4dbcfc475`, which holds only the
questions the owner can answer now. The reasons:

- The item was never preparable. Its preparation guard exits 1 by design,
  last at event 917, because it names no Windows build.
- Its wording predates the owner's 150% verdict of 2026-09-25, which
  accepted criteria 2 and 4 at 150%. What remains is the 100% capture, and
  `v001` does not ask that question.
- The owner asked for the next review to start with just the recent
  questions.

Nothing about criteria 2 and 4 changes. They stay `pending` and owe the same
capture at 100% display scale. That capture needs the owner to change the
display scale on JaegAW, then a station capture, then a new item naming its
image digest. That new item, not `v001`, is what a later session should ask.

## The 100% look, prepared for the owner on JaegAW, 2026-10-06 (bpr-main-win)

The owner chose to clear this card in person, on the JaegAW desktop. The
station prepared one question in a new, short attended session. It is not
`cbd48b91`, and no earlier item is on the page.

- **Session.** `39ba8abc36558edf2edecab37b68b819`, state
  `C:\Users\mikem\.mask\attended-review\owner-bpr0653-100pct-20261006-v001`
  (external, not committed), ready at sequence 3. The host is a loopback
  page on JaegAW, so the owner opens it in the Windows desktop browser.
- **Candidate.** `BPR-0653-windows-100pct-live-v001`, fingerprint
  `d76c0fc2c3234deca11dba6e095e659b68e268b2bc619006140c626a5bdeb3ed`.
- **The question.** Accept or Reject, with a note. At 100% display scale, is
  the strip the title row? The criterion covers:
  - the caption buttons at rest, hovered and pressed;
  - the snap layouts flyout from hovering Maximize;
  - dragging empty strip space;
  - double-click to maximize and double-click again to restore.
- **The image.** The unchanged image the owner accepted at 150%: `6aa27c8d…`,
  bundle `ca3bc2b1…`, built at `85e522d8`, in [[BPR-0974]]'s candidate root.
  Using it means both halves show one title row. Nothing in that root was
  touched.
- **The launcher.** `C:\Users\mikem\bpr0653-100pct-v001\launch.cmd` lives in
  its own root, beside its manifest, and does four things:
  - it checks both digests and the `plugins` folder;
  - it reads the primary monitor's effective DPI, and refuses unless it is 96;
  - it starts the image with `--render-evidence`, which means signed out, on a
    private temporary profile, as the 150% capture did;
  - it reports `WINDOW_UP`, `EXITED`, `NO_WINDOW` or a refusal within
    20 s, into `launch.log`.

  It never changes the display scale. The owner sets 100% first. Checked
  headless at 150%: the digests matched, and it refused with "the primary
  display is at 150% (144 DPI)", having launched nothing.

This is the owner's live look, not a station capture, so no PNG record comes
from it. If the PNG record is wanted too, `windows_caption_capture.ps1` can
take it at 100% in a cleared foreground block, while the display is still at
100%.

Criteria 2 and 4 stay `pending` until the verdict is recorded here.
Criterion 3 is unchanged. The card stays parked on human-review.

## Owner session prepared, 2026-10-09 (main, Mac)

Criteria 2 and 4 owe the owner's look at 100% on JaegAW. No agent drives that desktop, so this is a wizard with exact steps. It is served from the Mac, and an offline copy can be opened on JaegAW.

- Wizard `bpr0653-jaegaw-100pct-20261009-v001` (session `08eb6cb8-1e6c-4e8e-96b3-f30a42ba70a4`, 6 steps), state `/Users/jaeger/.mask/wizard/bpr0653-jaegaw-100pct-20261009-v001`, source root this checkout; its loopback host runs detached. The page's URL carries a token and is handed to the owner directly, not committed. Offline page: `/Users/jaeger/.mask/wizard/bpr0653-jaegaw-100pct-20261009-v001-offline.html`; its JSON export imports with `wizard.py import`. Steps: set 100%; run `C:\Users\mikem\bpr0653-100pct-v001\launch.cmd` (the launcher of 2026-10-06, unchanged image `6aa27c8d…`); Accept/Reject on the title row at rest, hovered and pressed (criterion 4; an optional picture is saved into the launcher's `captures` folder); Accept/Reject on snap, drag and double-click (criterion 2); notes (including whether JaegAW's own session `39ba8abc` was already answered); scale back to 150%. That the launcher is still on JaegAW was not checked from the Mac; the steps say what to do if it is missing.
- **Criterion 3 no longer needs the owner.** Its only red was BPR-1108's twelve findings on four unpinned rows. [[BPR-1237]] (Done) has since pinned all four (`sqlite_release_maintenance_files`, `application_attended_recent_replay`, `application_release_maintenance_public_path`, `application_release_update_public_path`, each `dos-volume-name`), and its criterion `test_network_isolation_manifest passes on Windows` passed. What remains is one Windows station pass of the criterion's selectors at a single main revision. That is station work, not a question.

## Criterion 3 at one main revision, again (bpr-main-win, 2026-10-09)

Run at main `b406b2e3b`, on Windows 11 Home 10.0.26200 with MSVC 14.51 and
Qt 6.11.2 at 150%, in `windows-dev-tour-cli` Debug. The tree was rebuilt at
that revision (exit 0) and the rows ran serially.

19 of the criterion's 20 rows ran:

- `qt_workbench_title_row_scale_{100,125,200}`: 46.3, 34.8, 37.8 s;
- `qt_workbench_focus_order_scale_{100,125,200}`: 5.4, 7.2, 9.3 s;
- `qt_workbench_window_scale_{100,125,200}`: 170.6, 179.6, 190.7 s;
- `qt_workbench_strip_menu` 14.9 s;
- `qt_home_list_scale_100` 23.9 s;
- `qt_review_queue` 0.04 s;
- `application_diff_workbench_public_path` 21.8 s;
- `application_review_round_trip_composition_contract` 10.3 s;
- `test_network_isolation_structure_contract` 10.7 s;
- `architecture_boundaries` 39.8 s;
- `core_dependency_boundary` 1.4 s;
- **`test_network_isolation_manifest` 22.4 s, green.** [[BPR-1237]] pinned
  [[BPR-1108]]'s four rows, so its earlier twelve findings are gone.

`qt_composed_github_scale_100` timed out at 120 s in that pass. The pass
overlapped two Release builds. Run again serially on a quiet machine, it
passes in 58.1 s. It passed in 89.6 s earlier the same day.

The 20th row, `application_windows_title_row`, was **not run**. It opens a
real window on the windows platform, and the station stays off the owner's
desktop until the owner clears it. It passed at `38cc26d7` and at
`2a5580d6`, as recorded above.

This card adds no uncontained registration.

So criterion 3 is met at `b406b2e3b` for every row except
`application_windows_title_row`, which is owed only because the desktop
is held.
