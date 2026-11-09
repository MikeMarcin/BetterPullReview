---
id: "RFC-BPR-0097"
title: "Go to a source line in the main diff"
status: "Auto-Verified"
depends_on: []
metadata: {"origin":"owner request, 2026-11-08","inspected_revision":"663137a9ed38","authoring":"author-rfc; frontend-design:frontend-design and desktop guidance loaded","milestone_plan":{"id":"go-to-line-v1","path":"docs/rfcs/plans/go-to-line-v1/milestone-plan.json"}}
---

# Go to a source line in the main diff

## Problem and grounded current behavior

The owner wants Go to line with a hotkey in the main diff window. File/change
navigation and source Find exist, but the inspected
[shortcut catalogue](../../src/adapters/qt/src/keyboard_shortcuts.cpp) and
[workbench menu](../../src/adapters/qt/src/workbench_window_queue_controls_and_menus.cpp)
offer no command for entering a source line number. On macOS, Command-G already
means Next match; Qt spells that binding `Ctrl+G` in the catalogue.

The comparison already retains source-side identity and exposes semantic line
selection in [diff_workbench](../../src/adapters/qt/include/bpr/qt/diff_workbench.hpp).
Its [exact-line reveal](../../src/adapters/qt/src/diff_workbench_presentation.cpp)
maps a source line into unified rows or the split view's shared comparison
controller. The command can build on these paths. These are source observations,
not a runtime verification of this proposed feature.

## Proposed behavior and boundaries

### Entry point and scope

Add **View > Go to line…**, beside Go to header/source, and list it under
Navigation in **Help > Keyboard Shortcuts**. Use physical **Control-G** on
macOS and **Ctrl+G** on Windows/Linux. Keep macOS Command-G and Command-Shift-G
for Find navigation. Register the platform bindings in the existing catalogue;
the active-window and
[no-window desktop menus](../../src/adapters/qt/src/desktop_menu_bar.cpp)
present the same command and shortcut.

The command targets the selected file in the active tab's main pull-request
diff, including a retained historical comparison or public-preview diff. It is
available in Review when at least one loaded text side has a navigable source
line. With no such diff, including Home, other review modalities, loading,
refused or binary comparisons, disable it. Its hotkey acts in source and
non-editing Review chrome; editable fields and other windows retain their own
key handling. Invoking the menu explicitly may move focus from an editor while
preserving its draft.

This proposal covers split and unified presentations of the main diff.
[Review Cycles](../../src/adapters/qt/include/bpr/qt/review_cycle_view.hpp)
uses a separate source/comparison view and is deferred, along with comment-body
navigation, file switching, relative offsets and configurable shortcuts.

### A compact dialog with an explicit source side

Open a compact window-modal **Go to line** dialog with an exclusive **Left** /
**Right** side picker (Base / Head), a labelled **Line** input, **Go**, and
Find's existing **×** close control on one row. Do not repeat the filename.
A quiet second row shows the chosen side's valid line range **1–N**, the
optional **:column** syntax, or the requested line's column range. Size the dialog to its
controls, without a separate button footer, and show validation only when
needed. Reuse Find in Files’ side chips and the app's
theme, dialog controls and focus treatment. The dialog adds no permanent row
and does not resize the diff. All controls and validation messages have
accessible names or associations and remain usable at supported scaling and
the application's minimum window size.

In split view, initially use the focused source side, or the most recently
focused source side for this comparison when invoked from chrome. In unified
view, use the selected source line's side. With no such side, prefer Head;
if that side has no navigable text, use the sole navigable side. Unavailable
or empty sides cannot be chosen. Base and Head always mean the displayed
comparison's endpoints, including in an older tab.

Seed Line with the current source line on the chosen side, otherwise **1**.
Focus and select the number for immediate replacement. Changing Side retains
the entered number and updates its range and validation. Accept a whole
decimal line number, optionally followed by **:column**, using the same
one-based line numbering as the gutter. A plain line targets its first column.
Columns are one-based native UTF-16 caret positions: a tab counts as one,
a supplementary Unicode character counts as two, and a line of length N has
an end-of-line position N+1. CRLF terminators do not count. A position inside
a surrogate pair is invalid. An empty, malformed, overflowing or out-of-range
value leaves Go disabled with a readable reason; never clamp it to a nearby
line or column. Enter/Return submits valid input and Escape/× dismisses the dialog.
The close control retains a readable accessible name and tooltip. Typing and changing
Side do not preview navigation. Cancellation restores the invoking focus and
leaves the reading position and selection intact.

### Reveal the exact line

On Go, close the dialog, reveal the requested source row and put keyboard focus
and the existing native caret on that exact side, line and column. Reveal the
caret horizontally when needed. Replace any previous
source range with that single-line target so subsequent keyboard navigation or
commenting starts there. A request inside a changed group must land on the
requested line rather than the group's first line. A repeated request reveals
the line even when it is already selected but has been scrolled away.

Numbers address the complete loaded source on the chosen side, including
unchanged lines outside changed hunks. Unified visual row numbers, diff prefixes
and spacer rows are never source line numbers. Split mode reveals through the
shared comparison controller, retaining alignment and the link map; unified
mode resolves the corresponding source row. Keep the target visibly identified
using the existing selection treatment, including when line numbers are hidden.

Bind the dialog to its opening window, tab, file and exact comparison/material.
Revalidate that target on submission. If it has changed or become unavailable,
show an explanation and refuse the jump; closing and reopening targets the new
comparison. Never apply the old number to a replacement file or revision.

### Ownership and limits

Follow the existing [workbench allocation](../architecture/system-architecture.md#rfc-bpr-0005-workbench-allocation):
core validates semantic line identity and selection; Qt owns the dialog,
shortcut, focus and reveal geometry. Reuse loaded line indexes and semantic
transitions. Opening or submitting this command performs no new source
acquisition, search or diff computation and introduces no per-line widgets or
whole-document navigation scans. Ordinary reading-state observation continues
through its existing owner. The command grants no publication authority.

The original defaults were adopted for implementation. Owner amendment
`go-to-line-appearance-r2` (attended handback37, 2026-11-08) replaces the
Base/Head dropdown with exclusive Left/Right chips inspired by Find in Files
and removes wasted dialog space. Navigation and focus were accepted for the
exact v002 frozen candidate. The revised appearance needs a fresh owner look;
all AC01–AC04 and source-side semantics remain unchanged.

Owner amendment `go-to-line-appearance-r3` (v003 handback 8, 2026-11-08)
rejects the revised appearance as still an inefficient use of space and
requests a dedicated design pass. The two-row composition above replaces
v003's file heading, separated range and bottom buttons. Original automated
acceptance and the exact v002 navigation verdict remain unchanged; the new
appearance requires its own frozen candidate and owner judgment.

Owner amendment `go-to-line-columns-r4` (chat, 2026-11-08) replaces Cancel
with the existing Find close control, removes the filename and adds the
familiar `line:column` syntax. The chosen column convention follows Qt's
native UTF-16 positions; it does not claim all VS Code Go to line behavior.
In particular, current VS Code converts visible columns through tab stops and
clamps some values ([primary implementation](https://github.com/microsoft/vscode/blob/main/src/vs/editor/contrib/quickAccess/browser/gotoLineQuickAccess.ts));
this command retains its explicit refusal contract. The follow-up
[BPR-1957](../issues/60-done/BPR-1957-go-to-line-columns.md) owns AC05–AC06.
Previously accepted criteria and frozen verdicts retain their producing revisions.

## Acceptance criteria

- **AC01:** Menu and hotkey invoke the same dialog for the eligible active
  diff, and the shortcut help shows the platform-correct binding. Ineligible
  views and editable fields obey Entry point and scope, and macOS Find keys
  retain their current behavior.
- **AC02:** In split and unified views, distinguishable Base/Head content
  demonstrates correct default-side selection and exact navigation to entered
  source numbers, including unchanged context, a changed-group interior and
  the last source line. Returning to an already selected offscreen line works;
  keyboard navigation and a subsequent draft target that line.
- **AC03:** Invalid input, an unavailable side and cancellation follow the
  dialog behavior without moving or replacing the source selection. A file,
  tab or comparison invalidated while the dialog is open cannot receive the
  old request.
- **AC04:** Through the composed production workbench, keyboard-only and menu
  flows demonstrate the defined focus, selection and accessible dialog states
  in both presentations. Full-window renders at minimum supported size and
  supported scaling show usable controls and a visible target; navigation
  retains the shared scroll geometry and the stated bounded-work behavior.
- **AC05:** Plain line and `line:column` requests place the exact native caret
  in split and unified views, including ASCII, tabs, supplementary Unicode,
  CRLF, empty lines and end-of-line positions. Invalid columns refuse without
  moving selection or scroll. Long-line carets are visible horizontally;
  subsequent comments still anchor exactly one source line. The current line's
  column range is readable and accessible, with no new source acquisition or
  diff/search computation.
- **AC06:** The compact panel has the shared Find close control, no filename
  or Cancel footer, and usable keyboard, accessible, light/dark and supported
  scale states. The owner accepts the exact frozen panel's appearance.

## Execution record

The original milestone, [BPR-1948](../issues/60-done/BPR-1948-close-rfc-0097.md), implements AC01–AC04.
The `line-column-panel` follow-up implements AC05–AC06 without rewriting that
completed card's historical acceptance.
RFC_BASE_SHA: `663137a9ed3825c87934c93926d414725f8bee22`.
[Milestone plan](plans/go-to-line-v1/MILESTONES.md).

Accepted on the isolated implementation branch at test candidate `e3e600c9f5483bcec66b140d0473d232b4ae7564`;
product candidate `52c8469b6a1c9d5fd9fc0fe7a6c39469abc73841` is unchanged.
All original AC01–AC04 pass at that revision. [Verification](../verification/RFC-BPR-0097.md) records
13-row integration, strengthened three-scale checks, falsification and limits.
Main integration remains with the coordinator while its other writer is active.

The `line-column-panel` milestone is complete at source/test revision
`bb06010ffe29011cf3529fb6c3efff4b89130f00`. AC05 includes the native BOM-column correction;
AC06 appearance was accepted for exact frozen v005, with its composition
unchanged by that correction. Original BPR-1948 acceptance and all historical
frozen verdicts retain their producing revisions. The
[final closeout](../issues/_checkpoints/go-to-line-v1/closeout.md) records evidence,
review intake, command quiescence and the coordinator's integration boundary.
