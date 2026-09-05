---
schema: "implement-rfc.card.v1"
id: "BPR-0092"
title: "Implement the native main window and primary toolbar"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0091"]
---

# Implement the native main window and primary toolbar

Build the native main window and top primary toolbar as Qt event and action adapters, not controllers. Previous/next file and previous/next change are top-level and keyboard addressable at supported widths, moving into an explicit compact overflow without losing keyboard access rather than disappearing.

Every action translates to a semantic intent; `QAction` state follows core command projections.

## Implementation

`bpr::qt::workbench_window` is a `QMainWindow` that also implements the
BPR-0091 projection port, the same shape `epoch_comparison_control` uses. It
computes no legality: `QAction::setEnabled` follows the core's
`workbench_command_projection`, and a disabled action carries the exact
`workbench_reason_code` text in both tooltip and status tip.

Compact overflow preserves keyboard access by construction rather than by
convention. All four actions are added to the window itself with
`Qt::WindowShortcut` at construction and are never removed; only toolbar
membership changes. Window association is what keeps a Qt shortcut live, so the
actions stay addressable whether top-level or in the overflow, which is
explicitly focusable and carries accessible name and description.

The compact threshold derives only from font metrics, style pixel metrics,
label widths, and a reserved central code width, so it is mode-independent and
cannot oscillate: entering compact mode cannot change the answer that caused
it. The minimum window size is deliberately below the threshold so the compact
path is genuinely reachable, and the test asserts that relationship rather than
assuming it.

A platform issue surfaced honestly during the slice: macOS returns an empty
`QKeySequence::mnemonic()`, so overflow keyboard access now sets an explicit
shortcut after `setText`. That was a real test catch, not a weakened assertion.

`fail()` deliberately leaves action state untouched, because a failed effect
changes no command legality; it surfaces status text and one deduplicated
polite announcement.

Drawers, comparison surface, detail pane, and selectors are named empty
placeholders owned by later cards.

## Direct verification

- `qt_workbench_window_scale_100`, `_125`, and `_200` pass offscreen and
  serial, alongside `architecture_boundaries` and
  `architecture_boundary_contract`. The full existing Qt suite passes 24/24
  with no regression.
- The 189-file format gate and strict `clang-tidy` with `--warnings-as-errors`
  pass.
- Non-vacuity, disabled-reason rule: pinning the file-action reason fails with
  "a disabled file action's tooltip carries its exact core reason"; separately
  pinning the change-action outcome fails with the change-action equivalent.
  The two were broken independently so neither red masked the other.
- Non-vacuity, compact keyboard rule: adding `removeAction` in the compact
  branch, the naive hide-them implementation, fails with "every primary action
  stays associated with the window so its shortcut keeps working in the compact
  overflow". Reproduced independently after the slice was reported, with the
  build exit status checked before trusting the red.

## Architecture finding carried to BPR-0102

The worker could not express change-navigation enablement as a core projection
because `core::workbench_command_id` names only file and drawer commands, and
change legality lives in the RFC-BPR-0004 semantic index rather than the
workbench core. Rather than invent core policy from the Qt layer, it introduced
a Qt-local `workbench_change_command_projection` carrying a decided
`change_navigation_outcome`, and correctly escalated the choice.

It also avoided a wrong local rule worth recording: deriving enablement from
whether a navigation result carries a group would have been incorrect, because
`at_start` and `at_end` both carry the current group.

That Qt-local projection is a genuine boundary deviation. The architecture is
"the core decides, Qt presents", and the RFC requires `QAction` state to follow
command projections. BPR-0102 moves change-navigation enablement into the core,
symmetric with file navigation: the host supplies bounded change-cursor facts
and the core projects the command. It is tracked rather than left implicit.

## Retained risks

- `publish()` ignores the drawer and layout domains, which belong to BPR-0093;
  the window drops them rather than half-applying them.
- Shortcut delivery is proven structurally, not by a synthesized keypress: Qt
  Test is not linked in this repository and public injection paths need private
  QPA headers. End-to-end delivery belongs to BPR-0097's UI automation.
- The minimum window size must stay below the compact threshold as later cards
  add content, or the compact path becomes unreachable.
- Nothing constructs the window yet; composition-root wiring is a later card.

## Result

The workbench shell presents top-level, keyboard-addressable navigation whose
enablement and disabled reasons come from the core, with a compact overflow
that keeps keyboard access structurally rather than by convention. BPR-0092 is
complete.
