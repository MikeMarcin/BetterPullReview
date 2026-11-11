---
schema: "implement-rfc.card.v1"
id: "BPR-1993"
title: "Make the tour on Overview read like the tour"
status: "Done"
rfcs: ["RFC-BPR-0099"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0005-overview-tour-summary.md"}
depends_on: ["BPR-1991"]
acceptance: [{"criterion":"RFC-BPR-0099 AC07","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","command":"ctest --preset dev -R 'qt_workbench_window_scale_|core_tour_publication_head'","detail":"Overview's Tour tab: Problem renders as Markdown (no source marks in the rendered text); the stop row shows item.cpp, lines 4-6 and its first sentence and opens that stop in Review's Tour tab; the state line and actions equal the Tour tab head's, Publish… included; rows carry one-based lines and the first sentence."}}]
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

## Record

Built in `3d97ea548`; record in
[RFC-BPR-0099 verification](../../verification/RFC-BPR-0099.md). Problem and
Solution on Overview are the Tour tab's Markdown reader; a stop row has its
file, lines and first sentence (emphasis and code marks dropped from the row)
and opens its stop in Review with a click or Return; a writer's state line and
actions are the Tour tab head's own decision, drawn by the same widget, and
act through the Tour tab. Frontend-design was loaded before the Qt work.
