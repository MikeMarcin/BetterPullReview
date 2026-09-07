---
schema: "implement-rfc.card.v1"
id: "BPR-0675"
title: "A drawer cannot be collapsed by anyone who has not read the code"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# A drawer cannot be collapsed by anyone who has not read the code

The owner, 2026-09-07: "I cannot figure out how to collapse a drawer at
all."

RFC-BPR-0028 AC-03 gives every drawer a collapsed rail with a vertical
label and an unread count, and the strip carries toggles. The owner, who
wrote the acceptance criterion, could not find them. Their capture shows
two toggle buttons at the left of the strip, unlabelled, next to Home, for
three drawers; nothing on a drawer itself offers to collapse it, and the
splitter handle between drawers gives no hint that dragging it to the edge
is not the way.

An affordance only its author can find is not an affordance.

## Scope

- A drawer offers its own collapse where the reviewer is looking: on the
  drawer, not only in the strip.
- The strip's toggles say which drawer they are for, and there is one per
  drawer.
- Collapsing and expanding are reachable by keyboard and named in the
  accessibility tree.
- Owned paths: `src/adapters/qt` drawer panel and workbench window strip,
  their tests.

## Acceptance

- A reviewer who has never seen the application can collapse and expand
  each drawer without being told how; the owner judges that in an attended
  session.
- Each control names its drawer, and a test pins one control per drawer.
