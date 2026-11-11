---
schema: "implement-rfc.card.v1"
id: "BPR-2117"
title: "A composed course shows a push changing accepted tour stops"
status: "Backlog"
rfcs: ["RFC-BPR-0106"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2116--BPR-2117--f5d986a0e8bc.json","origin":"RFC-BPR-0106 closeout review, 2026-11-11 (ledger, What no course shows yet)","related_cards":["BPR-2014","BPR-2015"]}
depends_on: []
---

# A composed course shows a push changing accepted tour stops

## Scope and acceptance

RFC-BPR-0106 AC04: "After a push that changes one stop's lines and the lines
one other stop links to, exactly those two stops are Changed since accepted,
each with its reason, and every other stop stays Accepted." It passes in the
core on a real comparison (`core_diff_semantics`,
`test_tour_acceptance_after_push`), but through the window it is shown only as
"not compared yet": no fixture has a second head with real content for a tour
route ([ledger](../../verification/RFC-BPR-0106.md#what-no-course-shows-yet)).
The agent-run live course of 2026-11-10 found exactly this kind of gap (sparse
heads never compared until the pinned-source fix), so core evidence alone does
not close AC04. RFC-BPR-0106 stays open ([[BPR-2015]] depends on this card).

- A committed fixture pull request with two readable heads, the second
  changing one stop's lines and the lines a second stop links to.
- A composed course (or the opt-in live course on BPR-Test) accepts three
  stops at the first head, moves to the second, waits for the comparison and
  reads each stop's state and reason from the window: exactly two Changed
  since accepted, the third Accepted; Accept at the new head turns the first
  green.

## Intended verification

The course itself, and a negative control that drops the comparison and
fails.
