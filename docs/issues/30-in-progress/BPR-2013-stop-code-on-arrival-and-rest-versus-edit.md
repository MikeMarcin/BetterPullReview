---
schema: "implement-rfc.card.v1"
id: "BPR-2013"
title: "Show a stop's code on arrival and separate reading a stop from editing it"
status: "In Progress"
rfcs: ["RFC-BPR-0106"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0025-stop-code-on-arrival-and-rest-versus-edit.md"}
depends_on: ["BPR-1991"]
---

# Show a stop's code on arrival and separate reading a stop from editing it

## Scope and acceptance

Implements
[RFC-BPR-0106](../../rfcs/RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md).

Make every way of arriving at a stop open its file in Review and reveal its
lines, remove the Show in code button, and make the stop's file-and-lines line
the link back to them. Reduce a stop at rest to its text, Edit, its side chat
and, while it is yellow, Accept with its reason; offer Use selected lines,
Enhance, Remove, Cancel and Save only while the stop is edited; keep route
operations in the stop's context menu; and remove Suggest an outline and
Insert outline from the application while leaving the `bpr tour outline`
commands as they are.

Covers AC01, AC06 and AC07 and the stop frames of the owner's look in AC08.
First establish which arrival paths fail to navigate today; the owner's extra
click was not reproduced in authoring. The Accept button's behaviour comes
from BPR-2014. Load frontend-design before the Qt work.

This is part of `tour-stops` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
