---
schema: "implement-rfc.card.v1"
id: "BPR-2014"
title: "Accept a tour stop the way a file is accepted"
status: "Parked"
rfcs: ["RFC-BPR-0106"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0026-stop-acceptance.md"}
depends_on: ["BPR-2013"]
parked_on: "human-review"
acceptance: [{"criterion":"RFC-BPR-0106 AC02","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"command","detail":"Each state-table row yields its state, reason and ring","command":"ctest --preset dev -R 'core_tour_stop_acceptance|qt_tour_stop_acceptance_card'"}},{"criterion":"RFC-BPR-0106 AC03","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"command","detail":"Accept, Save and the stop's own proposal accept; opening, the ring, Accept all, a hidden Accept and a relayed Ready do not","command":"ctest --preset dev -R 'core_tour_stop_acceptance|qt_tour_stop_acceptance_card|qt_tour_chat_in_tab|application_review_tour_writing'"}},{"criterion":"RFC-BPR-0106 AC04","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"command","detail":"Real comparisons of a push: exactly the stop whose lines changed and the stop whose linked lines changed turn yellow with their reasons; composed course shows only 'not compared yet' (fixture has no readable second head)","command":"ctest --preset dev -R 'core_diff_semantics|application_review_tour_publication_reconciliation'"}},{"criterion":"RFC-BPR-0106 AC05","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"command","detail":"Publication refused while any stop is yellow, allowed once all are accepted; publication and CLI courses unchanged in writes","command":"ctest --preset dev -R 'core_tour_stop_acceptance|application_review_tour_publication_|application_review_tour_cli_'"}},{"criterion":"RFC-BPR-0106 AC08","outcome":"pending","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"freeform","detail":"Owner look filed: attended session 679d26abbfa93b3357729061432131ba (five candidates, with BPR-2013's stop frames), frozen root /Users/jaeger/tour-stops-v001.5rt9c5gn (app c5e2a178c2ea...)"}},{"criterion":"BPR-2014 critical review","outcome":"pending","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"freeform","detail":"The pack asks for critical review of the publication rule; no separately authorized reviewer this round; not self-certified"}}]
---

# Accept a tour stop the way a file is accepted

## Scope and acceptance

Implements
[RFC-BPR-0106](../../rfcs/RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md).

Record in the core what the author accepted for a stop (its text, its
destination, and the code there and at each reference in its text) and decide
Accepted, Not accepted yet and Changed since accepted from it. Make Accept on
the current stop, Save of an edit and Accept on the stop's own proposal accept
it, and nothing else. Keep an unchanged stop Accepted across a new head and
turn yellow only the stops whose material changed. Require every stop Accepted
to publish, replacing the Ready mark, the Head changed chip and the separate
confirmation of lines. Carry saved drafts across: a stop that was Ready and
reaffirmed stays Accepted, and every other stop is Not accepted yet.

Covers AC02 to AC05 and the ring states of the owner's look in AC08. The owner
accepted these rules as drawn in the stop mockup on 2026-11-10; the look at
the running stop is still owed. This amends
publication eligibility under RFC-BPR-0042, 0043 and 0096: give it critical
review, since it decides what may be published.

This is part of `tour-stops` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Evidence

Product candidate `93b1045a8`; record
[RFC-BPR-0106 verification](../../verification/RFC-BPR-0106.md).
Frontend-design was loaded before the Qt work; the accepted stop mockup is the
design.

- AC02: `core_tour_stop_acceptance` (each row of the state table, with its
  reason), `qt_tour_stop_acceptance_card` (each ring and row's words) — pass.
- AC03: `core_tour_stop_acceptance`, `qt_tour_stop_acceptance_card`,
  `qt_tour_chat_in_tab` (the stop's own proposal accepts; Accept all does
  not), `application_review_tour_writing` (a hidden Accept accepts nothing) —
  pass.
- AC04: `core_diff_semantics` (`test_tour_acceptance_after_push`, real
  comparisons: exactly the two stops whose lines or linked lines changed turn
  yellow, each with its reason) — pass at the core. Through the window the
  only fixture push has no readable second head, so the course shows "not
  compared yet" rather than a real comparison; recorded as a gap.
- AC05: `core_tour_stop_acceptance` (`test_publication`), and all fourteen
  publication and CLI courses publish all-accepted tours with their write
  assertions unchanged — pass.
- AC08 (with BPR-2013's stop frames): the owner's look, filed and not waited
  on (below).
- Critical review of the publication rule: an independent reviewer returned
  pass-with-findings, provisional, on `93b1045a8`. Each finding was
  reproduced by a test and fixed or recorded in the post-review round (the
  [record's table](../../verification/RFC-BPR-0106.md#critical-review)). A
  re-review of the fixes by an independent reviewer is owed.

## Owner look (filed, not waited on)

Attended session `679d26abbfa93b3357729061432131ba`, state
`/Users/jaeger/.attended-review-state/tour-stops-v002-2026-11-10/state`,
cursor 15, host PID 61912 (its `tailscale serve` child 61936), five
candidates, one question each, ready and protected: `BPR-2013-arrival`,
`BPR-2013-stop-at-rest`, `BPR-2013-stop-editing`, `BPR-2014-accept` (on the
frozen app, conventional-comments-v1 renderer.cpp with `--attended-tour-look`)
and `BPR-2014-ring-states` (the frozen `tour_stop_acceptance_card --look`:
each ring state after a push). Frozen root
`/Users/jaeger/tour-stops-v001.5rt9c5gn` from `93b1045a8` (app SHA-256
`c5e2a178c2ea…`, card test `6683038724ed…`; the launcher's own check reached
`ATTENDED_TOUR_SHOWN`). The URL stays in the session state.
