---
schema: "implement-rfc.card.v1"
id: "BPR-1947"
title: "Verify and close RFC-BPR-0095"
status: "Done"
rfcs: ["RFC-BPR-0095"]
closeout_for: ["RFC-BPR-0095"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"diff-scroll-geometry-v1","path":"docs/rfcs/plans/diff-scroll-geometry-v1/milestone-plan.json"},"mandate_revision":"diff-scroll-geometry-r1","assigned_implementer":"/root/diff_scroll_implementation","reservation":"docs/issues/_reservations/BPR-1947--BPR-1947--a48cecbc976d.json"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"7e685a115ea5bdd8c6f956447470ba4788be1e19","evidence":{"kind":"observation","detail":"Qt workbench at 100/125/200%; native cursor versus number glyph bounds, number clicks and thread bars. Pass: fixed heading; offsets, crossings and reversal align within one device pixel. Seven-row final integration, focused format/tidy and documentation checks pass. See docs/verification/RFC-BPR-0095.md."}},{"criterion":"AC02","outcome":"pass","revision":"7e685a115ea5bdd8c6f956447470ba4788be1e19","evidence":{"kind":"observation","detail":"Workbench horizontal targets and production source Find at all three scales. Pass: exact window rectangles across scrolling, match reveals and nonzero-residual refresh; compact count stays stable. Seven-row final integration, focused format/tidy and documentation checks pass. See docs/verification/RFC-BPR-0095.md."}},{"criterion":"AC03","outcome":"pass","revision":"7e685a115ea5bdd8c6f956447470ba4788be1e19","evidence":{"kind":"observation","detail":"Raster and composed courses after resize, code zoom, numbers, query height and presentation changes. Pass: anchors correct and subsequent scroll stable; all narrow controls fit. Seven-row final integration, focused format/tidy and documentation checks pass. See docs/verification/RFC-BPR-0095.md."}},{"criterion":"AC04","outcome":"pass","revision":"7e685a115ea5bdd8c6f956447470ba4788be1e19","evidence":{"kind":"observation","detail":"Production source Find through installed Git/cache at all three scales; full-window renders and counter observations. Pass: full composed interaction/render; existing fractional navigation and Find retained; bounded work. Seven-row final integration, focused format/tidy and documentation checks pass. See docs/verification/RFC-BPR-0095.md."}}]
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

## Verification and execution record

Accepted product/test candidate `7e685a115ea5bdd8c6f956447470ba4788be1e19`.
All seven final integration rows pass (108.49s); focused format/tidy and both
documentation commands pass. The negative control refuses the original shifted
heading. [Verification](../../verification/RFC-BPR-0095.md) records commands,
per-criterion observations, renders, actual failures and scope limits.
No human, independent or native acceptance remains in this RFC.
[Checkpoint](../_checkpoints/BPR-1947/handoff.md) retains candidate and commands.

## Closeout checklist

RFC base: `31559e616707ed106d0bd98f8d047df785c38fc0`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Rendered unified comparisons containing unchanged, removed and added rows demonstrate the defined gutter alignment and fixed heading through several within-row offsets, whole-row crossings and reversal. Number and text row placement agree within one device pixel, including at clipped edges; clicking a visible number resolves its displayed source row. | Qt workbench at 100/125/200%; native cursor versus number glyph bounds, number clicks and thread bars | Pass: fixed heading; offsets, crossings and reversal align within one device pixel | `7e685a115ea5bdd8c6f956447470ba4788be1e19` |
| **AC02:** An open Find widget retains exactly the same window-relative rectangle through vertical and horizontal scrolling and match reveals in unified and both split panes. Refreshing placement at a nonzero residual produces no jump. The oracle uses the stationary displayed area independently of the raw backing-widget rectangle. | Workbench horizontal targets and production source Find at all three scales | Pass: exact window rectangles across scrolling, match reveals and nonzero-residual refresh; compact count stays stable | `7e685a115ea5bdd8c6f956447470ba4788be1e19` |
| **AC03:** After resize, code zoom, line-number visibility changes, Find query height changes and split/unified switching, the defined anchors are correct and subsequent scrolling retains them. Verify rendered alignment at 100%, 125% and 200% scale with existing narrow-width Find fitting preserved. | Raster and composed courses after resize, code zoom, numbers, query height and presentation changes | Pass: anchors correct and subsequent scroll stable; all narrow controls fit | `7e685a115ea5bdd8c6f956447470ba4788be1e19` |
| **AC04:** A composed production-workbench interaction and full-window render through the installed Git/cache path demonstrate both corrections. Existing fractional navigation and source Find behavior remain valid, and scroll observations retain the bounded-work rules under Scope and ownership. | Production source Find through installed Git/cache at all three scales; full-window renders and counter observations | Pass: full composed interaction/render; existing fractional navigation and Find retained; bounded work | `7e685a115ea5bdd8c6f956447470ba4788be1e19` |
