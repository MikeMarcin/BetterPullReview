---
id: "RFC-BPR-0096"
title: "Go to a source line in the main diff"
status: "In Progress"
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
navigation, file switching, columns, relative offsets and configurable shortcuts.

### A compact dialog with an explicit source side

Open a compact window-modal **Go to line** dialog showing the current file,
a **Side** choice of **Base** or **Head**, a labelled **Line** input, the chosen
side's valid range **1–N**, and **Go** and **Cancel** buttons. Reuse the app's
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
the entered number and updates its range and validation. Accept one whole
decimal line number within that range, using the same one-based numbering as
the gutter. An empty, malformed, overflowing or out-of-range value leaves Go
disabled with a readable reason; never clamp it to a nearby line. Enter/Return
submits valid input and Escape/Cancel dismisses the dialog. Typing and changing
Side do not preview navigation. Cancellation restores the invoking focus and
leaves the reading position and selection intact.

### Reveal the exact line

On Go, close the dialog, reveal the requested source row and put keyboard focus
and the existing line cursor on that exact side and line. Replace any previous
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

These are proposed defaults, not recorded owner answers. No material question
remains open in this draft; internal API and widget choices belong to implementation.

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

## Execution record

One milestone, [BPR-1948](../issues/20-ready/BPR-1948-close-rfc-0096.md), implements all four criteria.
RFC_BASE_SHA: `663137a9ed3825c87934c93926d414725f8bee22`.
[Milestone plan](plans/go-to-line-v1/MILESTONES.md).
