---
id: "RFC-BPR-0033"
title: "Profile Chooser and the Last Window: Profiles Are a Menu, Not a Gate"
status: "Proposed"
area: "BPR"
milestone: "Shell Surfaces v1"
depends_on: ["RFC-BPR-0024"]
metadata: {"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# RFC-BPR-0033: Profile chooser and the last window

## Summary and scope

Closing the last BetterPullReview window presents the Manage Profiles window.
The owner's verdict on 2026-09-13: "The profile select screen coming up every
time you close the main window is terrible." This RFC removes that behaviour,
gives each platform the exit it expects, and redesigns the chooser as a small
sheet of rows that opens when asked. It amends the presentation and the
last-window composition of RFC-BPR-0024; it changes no profile identity,
registry, credential, storage-root or runtime-ownership rule, and it supersedes
[[BPR-0696]]'s last-window presentation only.

## Problem, named from the source

`profile_desktop.cpp` presents the management window whenever the last profile
runtime is gone (`Runtimes.empty() && !Quitting && Management`), and whenever a
projection carries a failure while the window is hidden. BPR-0696 added the
first as the fix for an invisible desktop: `main.cpp` sets
`setQuitOnLastWindowClosed(false)` on every platform, so without the
presentation a reviewer on Windows or Linux who closed the last window was left
with a running process and nothing on screen. The chooser therefore became the
thing a reviewer meets after every close.

The window itself (`profile_management_window.cpp`) is a bare list, a
permanent "Profile name" field, a status sentence that is always present
("Open a profile, or create or rename one."), four buttons in a row, and a
full-width "Reload Profiles" button that the desktop appends for recovery. It
has the shape of a form that is always being filled in, for an action that is
taken a few times a month.

## Accepted design

### The last window

- Closing a window closes a window. No chooser, no other window, is presented
  because a window closed. Explicit close still removes the window from the
  next restore, and quitting still preserves the open set, as RFC-BPR-0024
  states.
- **macOS.** The application stays resident with no window, as macOS
  applications do: the menu bar remains (File, Profiles, View and the rest), the
  Dock icon remains. Activating the application (a Dock click, ⌘Tab, or a Finder
  launch while running), choosing a profile from the Profiles menu, or ⌘N,
  opens the most recently used profile's window at Home through the existing
  activate and new-window intents. The registry and scratch lease stay open
  exactly as BPR-0696 left them.
- **Windows and Linux.** Closing the last window of the last open profile quits
  the application through the existing desktop Quit path, after every profile
  has drained. The platform expectation is that a program with no window is
  gone, and the restore rules already distinguish this close from Quit.
- **Failures.** A profile operation that fails while a window exists reports in
  that window (the Profiles menu's status item and the window's status bar),
  with the chooser's own notice shown only if the chooser is open. The chooser
  is presented for a failure only when no window exists to carry it, which is
  the startup case RFC-BPR-0024 already describes.

### The chooser

The window is titled Profiles and opens from Profiles › Manage Profiles… or at
startup when no profile can be opened. It is a sheet of rows over the window
ground: a letter avatar, the profile name, and its state at the right ("Open in
2 windows", "Opening", "Closing", "Closed") from the existing projection. The
current profile's row is selected on open. Return or double-click opens the
selected profile (or focuses it when it is open); the footer's filled button
says Open for a closed profile and New window for an open one, because that is
what the gesture does.

- **Rename** is a footer action that turns the selected row's name into an
  editor in place with Save and Cancel replacing the footer's buttons; Escape
  cancels and keeps the text. The name field does not exist otherwise.
- **New profile** is a flat footer action that appends an editing row with the
  placeholder "Profile name" and the same Save and Cancel; the new profile
  starts empty with standard defaults, as RFC-BPR-0024 states.
- **Failure** is one notice above the rows in the sheet, in the existing
  failure vocabulary ("Profiles could not be saved. The name is kept.") with one
  Try again button that reloads; it is absent otherwise. There is no permanent
  Reload Profiles button and no permanent status sentence; "Profile saved." is
  not announced, because the renamed row is the confirmation.
- **Profiles menu.** Lists the profiles by name with the current one checked,
  then New Window, then Manage Profiles…, as today; the menu is the ordinary way
  to switch, and the sheet is for creating and renaming.

The sheet is 440 by 360 points, rows 44 high with one hairline between them,
the same radius and palette roles as RFC-BPR-0032, no other colour, no icons.

## Architecture

The core decides. The host performs. Qt presents.

- **Composition root (`profile_desktop.cpp`).** The last-runtime branch no
  longer presents the management window. A platform policy object, injected by
  the composition and observable in tests, decides what the empty desktop does:
  stay resident (macOS) or quit (elsewhere). The application-activate and menu
  paths dispatch the existing `open_bpr_profile_intent` or
  `new_bpr_window_intent` for the most recently used profile, which the registry
  already orders. A failure with a live window is routed to that window's
  existing status projection; the management window's `present()` on failure is
  gated on it already being visible or no window existing. The appended
  RecoveryNotice and ReloadProfiles widgets move into the window's own failure
  notice.
- **Qt adapter (`profile_management_window.cpp`).** Rows through a delegate,
  in-place editors, the footer, the notice. The same `profile_management_dispatch`
  and projection; no new intent. Accessible names and the tab order are
  declared; the row's state is its accessible description.
- **Application host and core.** No change. Ordering by most recent use, if not
  already projected, is read from the registry's existing records; nothing new
  is persisted.

Prohibited: a launcher screen, a splash, a tray item, a timer that reopens a
window, any change to runtime admission or to which process owns a profile, and
any per-platform behaviour that is not a named policy in the composition.

## Non-goals

Profile deletion, export, cloud sync, profile pictures, a profile switcher in
the strip, and window restoration changes beyond routing the empty desktop.

## Acceptance criteria

- **B1 — A close is a close.** Closing the last window of the last open profile
  never presents the chooser: on macOS the application stays resident with its
  menu bar and Dock icon and no window; on Windows and Linux it quits through the
  existing drain and Quit path; the restore records distinguish this close from
  Quit as RFC-BPR-0024 requires.
- **B2 — Reopening.** With no window on macOS, application activation, ⌘N, and
  choosing a profile from the Profiles menu open the most recently used
  profile's window at Home through the existing intents, without a second
  runtime owner and without touching another profile.
- **B3 — Failures go to a window.** A failed profile operation with a live
  window is reported in that window and does not present the chooser; the
  chooser shows its notice only while open, and is presented for a failure only
  when no window exists.
- **B4 — The chooser is rows.** The Profiles sheet shows one row per profile
  with avatar, name and state, selects the current profile on open, opens or
  focuses on Return and double-click, renames and creates in place with Save and
  Cancel, keeps text on Escape and on failure, shows one failure notice with Try
  again only after a failure, and has no permanent name field, status sentence
  or Reload button.
- **B5 — Rendered and reachable.** The sheet in its default, renaming, creating
  and failed states renders at 100, 125, 150 and 200 percent in the four
  appearances, every control is reachable by keyboard with an accessible name
  and the row state as its description, and the owner accepts the real macOS
  application through the attended-review workflow, including closing the last
  window and reopening from the Dock.

## Verification

| Claim | Direct surface | Check |
|---|---|---|
| B1, B2 | The shipped desktop composition with the recording registry, driven per platform policy | Close the last window and assert no management presentation, the resident or quit outcome by policy, the restore record; activate and ⌘N reopen the most recent profile once; the existing management course keeps passing |
| B3 | Desktop composition with injected failures | Fail a rename with a window open and assert the window's status and no presentation; fail with no window and assert presentation |
| B4 | `qt_profile_management` component course | Row inventory, selection on open, Return and double-click, in-place rename and create, Escape and failure text retention, notice presence only after failure, absence of the retired controls |
| B5 | Deterministic captures then attended review on macOS | Four appearances, four scales, four states; keyboard walk; VoiceOver names and descriptions; the owner's close-and-reopen course |

Target-native Windows and Linux quit behaviour is platform work on the existing
lanes; until observed it is recorded as not run, never as passing. No live
GitHub or GHES access is needed.

## Documentation impact

The profile and window authority inventory, the workbench specification's
menu-bar section and the settings guide's General page describe the empty
desktop per platform and the chooser; RFC-BPR-0024's own text is not edited.
Historical BPR-0696 evidence stays as recorded.
