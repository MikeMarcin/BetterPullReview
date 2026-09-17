---
schema: "implement-rfc.card.v1"
id: "BPR-0653"
title: "Windows: the strip as the title row"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
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
