---
schema: "implement-rfc.card.v1"
id: "BPR-1991"
title: "Give the Tour tab a head, a scrolling route and a pinned agent bar"
status: "Done"
rfcs: ["RFC-BPR-0099"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0003-tour-tab-bands.md"}
depends_on: []
acceptance: [{"criterion":"RFC-BPR-0099 AC01","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","command":"ctest --preset dev -R 'qt_review_tour_publication_working|qt_tour_click_scroll|application_review_tour_composition'","detail":"test_bands: 24 stops in a 380x620 tab; the route scrolls; the head (top) and agent bar (bottom) are outside the route and keep their positions and visibility with the route scrolled to its end; Publish… stays in the head. Clicks keep the route's place; no sideways clipping at 100, 125 and 200 percent."}},{"criterion":"RFC-BPR-0099 AC02","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","command":"ctest --preset dev -R 'core_tour_publication_head|qt_review_tour_publication_working|application_review_tour_publication_stop'","detail":"Every row of the head table with its text and actions; Publish… disabled with a tooltip naming what is missing; Publish… opens the tour page and computes its preview; publishing reads step 1 while the first write is held; after a restart the head reads stopped at step 2 of 4 with Resume first."}}]
---

# Give the Tour tab a head, a scrolling route and a pinned agent bar

## Scope and acceptance

Implements
[RFC-BPR-0099](../../rfcs/RFC-BPR-0099-tour-publishing-in-reach-and-reported.md).

Rebuild the Tour tab as three bands with only the route scrolling, add the
head with its state table and single publication action, keep the agent bar at
the bottom at every route length, and remove the bottom Preview and publish
button.

Covers AC01 and AC02. The head's Publishing and Stopped states need the per-
step projection from BPR-1992 and are completed there. Load frontend-design
before the Qt work.

This is part of `tour-publishing` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Record

Built in `3d97ea548`; record in
[RFC-BPR-0099 verification](../../verification/RFC-BPR-0099.md). The head is
decided in the core (`decide_tour_head`) and drawn by one widget the Tour tab
and Overview share. The drawer's scroll area around the whole tab is gone; the
route has its own, and keeps the tab's 280-pixel floor so a narrow drawer
narrows it instead of scrolling it sideways (a first design scrolled sideways
at a large font). Until RFC-BPR-0106 lands, a stop counts as accepted under
the rule `assess_tour_publication` applies to every stop
(`tour_stop_publishable`). Frontend-design was loaded before the Qt work.
