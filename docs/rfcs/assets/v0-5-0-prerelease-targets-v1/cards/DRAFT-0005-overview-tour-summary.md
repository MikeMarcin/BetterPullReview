---
schema: "implement-rfc.card.v1"
id: "DRAFT-0005"
title: "Make the tour on Overview read like the tour"
status: "Backlog"
rfcs: ["RFC-DRAFT-0002"]
depends_on: ["DRAFT-0003"]
---

# Make the tour on Overview read like the tour

## Scope and acceptance

Implements
[RFC-DRAFT-0002](../RFC-DRAFT-0002-tour-publishing-in-reach-and-reported.md).

Render Problem and Solution as Markdown on Overview, give each stop row its
file, lines and first sentence and make it open that stop in Review, and reuse
the Tour tab head's state line and actions.

Covers AC07. The owner confirmed on 2026-11-10 that these three faults are
the ones note 3 meant. Load frontend-design before the Qt work.

This is part of `tour-publishing` in the [plan](../milestone-plan.json).
