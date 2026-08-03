---
id: BPR-0102
title: Move change-navigation enablement into the core projection
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0092, BPR-0094]
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
