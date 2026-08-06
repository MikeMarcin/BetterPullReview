---
id: BPR-0191
title: The workbench shell becomes the application's window
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0190]
---

# The workbench shell becomes the application's window

The largest slice on the board, and the one that makes RFC-BPR-0005 real.

`github_window` and `workbench_window` both compose `diff_workbench` and
`epoch_comparison_control`. The application shows `github_window`, so the
shell RFC-BPR-0005 specified — toolbar, compact overflow, drawers, detail pane,
declared focus order — has never been on screen. BPR-0188 decided that
`workbench_window` becomes the application's window and recorded why the
cheaper alternative was refused.

## What moves

RFC-BPR-0002's session chrome becomes a composed region inside the shell rather
than a window of its own: host profile selector, owner/repository/pull-request
locator, connect/open/refresh actions, and the status banner. Its threaded
conversation view moves into the conversations drawer region.

RFC-BPR-0002 keeps sole ownership of session identity, credentials, capability,
and conversation content. It stops owning the window. Nothing about what a
conversation *is* changes here.

## The surface must be composed once

Two owners of one review surface is how this started. When this lands,
`diff_workbench` and `epoch_comparison_control` have exactly one composing
parent, and a test says so rather than a reviewer checking.

## Evidence that moves with the code, not evidence that is rewritten

`github_window`'s existing assertions were each forced by a real shipped
defect. BPR-0097 found three live accessible-name collisions — two focusable
widgets answering to `Previous change`, two to `Files drawer`, two to `Review
navigation` — and an ambiguous `Alt+1`/`Alt+2` binding that made Qt trigger
*neither* shortcut, a defect whose only symptom is a key that silently does
nothing.

Every one of those tests applies to the composed window and must pass against
it. In particular the rule that no duplicate key sequence exists anywhere in
the assembled window is now checked over a strictly larger widget set, and it
is the assertion most likely to fail when these two windows merge. That is the
point of moving it rather than rewriting it.

## Required direct evidence

- `nm` on the shipped binary reports `workbench_window` symbols, and the
  application's window is the shell.
- Exactly one widget composes `diff_workbench`, and exactly one composes
  `epoch_comparison_control`, asserted structurally.
- Every accessible name in the assembled window is unique, and every key
  sequence is unique, over the merged widget set.
- The declared focus order names the interactive stops the composed shell
  offers, and the comparison surface still contributes exactly one stop —
  ordering inside it stays RFC-BPR-0004's decision.
- Connect, open, refresh, and the pull-request locator still work, still emit
  intents, and still take their enablement and disabled reasons from command
  projections rather than from local widget state.
- The compact overflow threshold is still derived from style and font only,
  never from the current toolbar mode.
- RFC-BPR-0005's virtualization figures are re-measured on the composed window
  at 60, 6,000, and 60,000 lines. The recorded 68/68/68 figure was measured on
  `qt_workbench_window`; if the merged window's bounded widget count differs,
  the claim is restated with the new number rather than quietly kept.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What must not be done instead

Keeping both windows and showing the shell only in tests. That is the current
state with extra steps.

Relaxing an accessible-name or key-sequence assertion because the merge made it
fail. A collision after the merge is a real collision — the merged window is
what a user gets, and a name or shortcut that is ambiguous there is ambiguous
for them.
