---
schema: "implement-rfc.card.v1"
id: "BPR-1993"
title: "Make the tour on Overview read like the tour"
status: "In Progress"
rfcs: ["RFC-BPR-0099"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0005-overview-tour-summary.md"}
depends_on: ["BPR-1991"]
---

# Make the tour on Overview read like the tour

## Scope and acceptance

Implements
[RFC-BPR-0099](../../rfcs/RFC-BPR-0099-tour-publishing-in-reach-and-reported.md).

Render Problem and Solution as Markdown on Overview, give each stop row its
file, lines and first sentence and make it open that stop in Review, and reuse
the Tour tab head's state line and actions.

Covers AC07. The owner confirmed on 2026-11-10 that these three faults are
the ones note 3 meant. Load frontend-design before the Qt work.

This is part of `tour-publishing` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
