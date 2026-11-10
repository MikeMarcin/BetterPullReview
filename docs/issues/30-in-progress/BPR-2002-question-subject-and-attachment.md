---
schema: "implement-rfc.card.v1"
id: "BPR-2002"
title: "Give each agent question a subject and replace the scope list with the attachment"
status: "In Progress"
rfcs: ["RFC-BPR-0102"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0014-question-subject-and-attachment.md"}
depends_on: []
---

# Give each agent question a subject and replace the scope list with the attachment

## Scope and acceptance

Implements
[RFC-BPR-0102](../../rfcs/RFC-BPR-0102-agent-questions-say-what-they-point-at.md).

Have the core decide a request's subject; send where the reviewer is looking
separately from it; remove the scope list and show selected lines as a
detachable chip; make suggestions carry their subject and entry point so each
one sends from any mode; send a typed question with no selection; clear the
diff's selection on Escape under the RFC's precedence.

Covers AC01 to AC07. AC03 needs an installed agent on a fixture pull request
of at least five changed files. Load frontend-design before the Qt work.

This is part of `agent-tab-asks` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
