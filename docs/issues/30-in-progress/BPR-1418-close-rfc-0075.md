---
schema: "implement-rfc.card.v1"
id: "BPR-1418"
title: "Verify and close RFC-BPR-0075"
status: "In Progress"
rfcs: ["RFC-BPR-0075"]
depends_on: []
closeout_for: ["RFC-BPR-0075"]
metadata: {"milestone_plan":{"id":"title-row-publication-v1","path":"docs/rfcs/plans/title-row-publication-v1/milestone-plan.json"}}
---

# BPR-1418: Verify and close RFC-BPR-0075

Mandate `title-row-publication-r1` authorizes full implementation of
[RFC-BPR-0075](../../rfcs/RFC-BPR-0075-title-row-publication.md), adopted from
RFC-DRAFT-0014. 

RFC base: `d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1`.

## Acceptance

- AC01: Loading-to-acceptance and direct acceptance show the current identity and adjacent browser button without resize or Home round trip.
- AC02: Replacement, republication and clear use only the current text/browser context; absent context hides the button.
- AC03: Preserved Home, local-cycle, modality and selected-file navigation remains intact; returning presents the current row.
- AC04: Existing order, adjacency, accessibility and width yield pass rendered checks at 100%, 125% and 200%, roomy/compact/platform floor widths, without overflow or oscillation.

## Current frontier

Plan prepared by the milestone implementer; product work has not begun.
The original proposal and normalized plan are preserved in the live import.
No criterion requires a new human or native verdict; offscreen evidence cannot
qualify native appearance or the owner's installed binary.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** A first read that exposes its loading surface before acceptance, with normally offered workbench commands retained, shows the accepted identity and adjacent Open on GitHub button at a fixed roomy width. The observation occurs before any resize or Home round trip and fails against the investigated implementation; direct acceptance also succeeds. | Public window transition and title-row checks | Pending | `d9c93c2db` baseline |
| **AC02:** Replacement, same-PR republication and clearing settle the row without a geometry change. Its text and browser command refer to the latest accepted publication only, and a PR without a browser context offers no title-row browser button. | Public window transition and title-row checks | Pending | `d9c93c2db` baseline |
| **AC03:** Content publication preserves the defined Home, local-cycle and navigation behavior. Returning to a retained PR presents its current row immediately, without a second corrective gesture. | Public window transition and title-row checks | Pending | `d9c93c2db` baseline |
| **AC04:** The corrected transition retains existing title-row order, adjacency, accessibility and width-yield behavior at the registered 100%, 125% and 200% scales; roomy, compact and platform-floor widths obey the existing contract without toolbar overflow or visibility oscillation. | Public window transition and title-row checks | Pending | `d9c93c2db` baseline |
