---
schema: "implement-rfc.card.v1"
id: "BPR-1947"
title: "Verify and close RFC-BPR-0095"
status: "Ready"
rfcs: ["RFC-BPR-0095"]
closeout_for: ["RFC-BPR-0095"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"diff-scroll-geometry-v1","path":"docs/rfcs/plans/diff-scroll-geometry-v1/milestone-plan.json"},"mandate_revision":"diff-scroll-geometry-r1","assigned_implementer":"/root/diff_scroll_implementation","reservation":"docs/issues/_reservations/BPR-1947--BPR-1947--a48cecbc976d.json"}
---

# BPR-1947: Verify and close RFC-BPR-0095

Implement and close [RFC-BPR-0095](../../rfcs/RFC-BPR-0095-stable-diff-gutters-and-find.md)
as one bounded milestone. User mandate: implement supplied RFC-DRAFT-0029,
revision r1, all four acceptance criteria unchanged. No attended criterion.

## Acceptance

- AC01: Rendered unified unchanged/removed/added rows keep their numbers, backgrounds, selection and indications aligned within one device pixel; heading fixed and number clicks select the displayed row across residuals, crossings and reversal.
- AC02: Find rectangle stays exactly stationary through both-axis scrolling, match reveals and nonzero-residual placement refresh in unified and both split panes, using an independent displayed-area oracle.
- AC03: Resize, code zoom, number visibility, query height and presentation switching retain correct anchors and subsequent scrolling at 100%, 125% and 200%, including narrow fitting.
- AC04: Production-workbench interaction and full-window render through installed Git/cache demonstrate both fixes; existing navigation/Find and bounded-work checks pass.

## Continuation

Baseline `31559e616707ed106d0bd98f8d047df785c38fc0`.
[Checkpoint](../_checkpoints/BPR-1947/handoff.md) retains candidate and commands.

## Closeout checklist

RFC base: `31559e616707ed106d0bd98f8d047df785c38fc0`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Rendered unified comparisons containing unchanged, removed and added rows demonstrate the defined gutter alignment and fixed heading through several within-row offsets, whole-row crossings and reversal. Number and text row placement agree within one device pixel, including at clipped edges; clicking a visible number resolves its displayed source row. | Geometry and source Find scale courses | Pending | Baseline `31559e616707` |
| **AC02:** An open Find widget retains exactly the same window-relative rectangle through vertical and horizontal scrolling and match reveals in unified and both split panes. Refreshing placement at a nonzero residual produces no jump. The oracle uses the stationary displayed area independently of the raw backing-widget rectangle. | Geometry and source Find scale courses | Pending | Baseline `31559e616707` |
| **AC03:** After resize, code zoom, line-number visibility changes, Find query height changes and split/unified switching, the defined anchors are correct and subsequent scrolling retains them. Verify rendered alignment at 100%, 125% and 200% scale with existing narrow-width Find fitting preserved. | Geometry and source Find scale courses | Pending | Baseline `31559e616707` |
| **AC04:** A composed production-workbench interaction and full-window render through the installed Git/cache path demonstrate both corrections. Existing fractional navigation and source Find behavior remain valid, and scroll observations retain the bounded-work rules under Scope and ownership. | Geometry and source Find scale courses | Pending | Baseline `31559e616707` |
