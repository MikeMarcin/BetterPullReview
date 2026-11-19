---
schema: "implement-rfc.card.v1"
id: "BPR-2278"
title: "A tour's stops start at the drawer's left edge"
status: "Done"
rfcs: []
depends_on: ["BPR-2274"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2277--BPR-2279--424d9c5fc0e9.json","found_by":"owner attended review, 2026-11-19, frozen fixture build of 91dedb8a3","related_cards":["BPR-2274"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9512b097b","evidence":{"kind":"command","detail":"qt_tour_stop_card_cleanup: Draft Tour, the route row, the open card and Add stop all start at x=10 in the editor"}},{"criterion":"AC2","outcome":"pass","revision":"9512b097b","evidence":{"kind":"command","detail":"qt_tour_stop_card_cleanup BPR-2278 geometry check; failed at 18 vs 10 before the fix"}},{"criterion":"AC3","outcome":"pass","revision":"9512b097b","evidence":{"kind":"command","detail":"qt_tour_*, qt_review_tour_*, application_review_tour_composition_public_path and scale 125/200, coding-format-check, verify_documentation"}}]
---

# BPR-2278: A tour's stops start at the drawer's left edge

## Observed

Owner, 2026-11-19: "stops are still started off shifted to the right like
they were when there was a parent section, they can start on the left of the
drawer now." [[BPR-2274]] removed the Problem and Solution; the stop route
kept the indentation it had under them.

## Acceptance

- AC1. The stop route (number rail, drag handles, cards, Add stop) aligns
  with the drawer's left content edge, the Draft Tour button.
- AC2. A Qt test asserts the geometry.
- AC3. Focused tests, format and the documentation gate pass.

## Cause

The route's slot kept an 8 px left margin from when it sat under the
Problem and Solution, and the stop card's ground began 14 px into its row,
after a gutter for the drag grip. Together the cards started 22 px right of
Draft Tour, and Add stop 8 px.

## Fix

The slot has no left margin, and the stop card's ground starts at the row's
left (`card_left`, its border a pixel in). The grip's dots now sit inside
the card, between its accent bar and the number ring (`ring_left` 16, from
22); the text column moves with the ring. The page mode's outline uses the
same delegate and moves the same way.

## Verification

`qt_tour_stop_card_cleanup` measures, in the editor's coordinates, the left
of Draft Tour, of the route's first row, of the open stop's card and of Add
stop: 10, 10, 10, 10 (before: 10, 18, -, 18; it failed at "the stops' rail
starts at the drawer's left content edge (18 vs 10)"). Run with a path
prefix to save `card-read.png`.
