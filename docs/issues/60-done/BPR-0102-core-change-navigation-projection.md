---
schema: "implement-rfc.card.v1"
id: "BPR-0102"
title: "Move change-navigation enablement into the core projection"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0092","BPR-0094"]
---

# Move change-navigation enablement into the core projection

BPR-0092 could not express previous/next change enablement as a core command
projection. `core::workbench_command_id` names only the file and drawer
commands, and change legality lives in the RFC-BPR-0004 semantic index rather
than in the workbench core, which holds no diff. Rather than invent core policy
from the presentation layer, that slice introduced a Qt-local
`workbench_change_command_projection` carrying an already-decided
`change_navigation_outcome`.

That is a real boundary deviation. The governing architecture is that the core
decides and Qt presents, and RFC-BPR-0005 requires `QAction` state to follow
command projections. Today two of the four primary actions follow a core
projection and two follow a Qt-local struct, so the same acceptance criterion is
met by two different mechanisms.

Make change navigation symmetric with file navigation. The host supplies bounded
change-cursor facts in the same shape it already supplies the file cursor —
whether a previous or next change exists, and the outcome vocabulary — and the
core projects `previous_change` and `next_change` commands with exact disabled
reasons. The window then consumes one uniform `workbench_command_delta` and the
Qt-local projection type disappears.

Preserve the correct judgement BPR-0092 recorded: enablement must not be derived
from whether a navigation result carries a group, because `at_start` and
`at_end` both carry the current group. Whatever the core uses must distinguish
those outcomes explicitly.

This is a boundary correction, not a behaviour change. The window must keep
emitting the navigation policy it was offered, so the gesture and the enablement
that offered it cannot disagree about stop versus wrap.

## Required direct evidence

- `core::workbench_command_id` names all four primary commands, and the Qt layer
  holds no projection type of its own.
- A core test proves `at_start` and `at_end` disable the matching direction
  while still reporting the current group, so the rejected derivation stays
  rejected.
- The window test proves all four actions follow one core projection, and the
  disabled-reason assertions still fail when the reason is pinned.

## Implementation

`core::workbench_command_id` now names all four primary commands, with
`previous_change` and `next_change` between the file commands and the drawer
toggles. All four follow one `workbench_command_delta`, and the Qt-local
`workbench_change_command_projection` is gone from both the header and the
implementation.

The core still has no diff, exactly as it has no file list. A
`workbench_change_cursor` carries the bounded facts the host supplies — the
selected group, ordinal, change count, and explicit `PreviousAvailable` and
`NextAvailable` — and the core decides legality from them. This mirrors the
file cursor rather than inventing a second shape.

The rejected derivation stays rejected, and the code says why at the decision
site: enablement is never derived from whether a navigation result carried a
group, because `at_start` and `at_end` both report the current group, so group
presence cannot distinguish a step from a refusal.

Inserting two enum members shifted the drawer toggles from positions 2 and 3 to
4 and 5. Rather than leave that coupling implicit, the window carries
`static_assert`s binding each command id to its projection index, so a future
reorder becomes a compile error instead of a silent misread. The `-Wswitch`
error that first exposed the half-migration was left unsilenced — no `default:`
label was added, since that error is what makes the next enum addition fail
loudly.

## Direct verification

- `core_workbench_interaction` and the full core suite pass 10/10; the Qt suite
  passes 24/24; both architecture boundary suites and the format gate pass.
- Non-vacuity, rejected derivation: replacing the availability facts with
  `cursor.Group.has_value()` fails with "the first change disables previous
  change while the retained cursor still reports that exact current group".
  That assertion names the precise property, so the derivation BPR-0092
  declined cannot be reintroduced silently.

## A verification error worth recording

The first three attempts at that non-vacuity check all reported a pass, and I
briefly concluded the coverage was missing. The cause was mine, not the code's:
`workbench_interaction_tests.cpp` has its own executable target, and I had been
rebuilding `test_better_pull_review_epoch_core_tests` instead of
`test_better_pull_review_workbench_interaction_core_tests`. Every green came
from a stale binary that never saw the break.

It was caught by escalating the break rather than believing the result — an
unconditional always-disable also passed, which is impossible for a suite that
compares enablement against navigation outcomes, so the binary had to be stale.
Checking `BUILD-EXIT` is not sufficient on its own: the build succeeded every
time, because it was faithfully building a target that did not contain the
changed code. The check that actually works is confirming the build recompiled
the file you edited.

## Result

All four primary commands follow one core projection, the Qt layer holds no
projection type of its own, and the positional coupling that the enum change
introduced is now a compile-time assertion rather than a latent misread.
BPR-0102 is complete.
