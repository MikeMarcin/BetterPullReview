---
schema: "implement-rfc.card.v1"
id: "BPR-2015"
title: "Verify and close RFC-BPR-0106"
status: "Done"
rfcs: ["RFC-BPR-0106"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0027-close-tour-stops.md","draft_title":"Close the tour stop RFC"}
depends_on: ["BPR-2013", "BPR-2014", "BPR-2117"]
closeout_for: ["RFC-BPR-0106"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"observation","detail":"See the closeout checklist and ledger"}},{"criterion":"AC02","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"observation","detail":"See the closeout checklist and ledger"}},{"criterion":"AC03","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"observation","detail":"See the closeout checklist and ledger"}},{"criterion":"AC04","outcome":"pass","revision":"1d5558c896254ba9d9a74e3a1d88661e43396138","evidence":{"kind":"observation","detail":"Core real comparison and composed application_review_tour_publication_push; negative control fails"}},{"criterion":"AC05","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"observation","detail":"See the closeout checklist and ledger"}},{"criterion":"AC06","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"observation","detail":"See the closeout checklist and ledger"}},{"criterion":"AC07","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"observation","detail":"See the closeout checklist and ledger"}},{"criterion":"AC08","outcome":"pass","revision":"93b1045a8e1248bbd382d7b6324e8522859d858b","evidence":{"kind":"observation","detail":"Owner accepted the stop looks (sessions 679d26ab, 7c920675)"}}]
---

# BPR-2015: Verify and close RFC-BPR-0106

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0106](../../rfcs/RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md)
against actual evidence from the cards this one depends on, including its
owner-attended criterion, the reconciliation of the live RFCs it amends, and
the documentation gate. Record failures and unreached requirements exactly.

Leave live IDs, the implementation baseline and acceptance records to
authorized execution. This proposed closeout carries no passing evidence and
changes no live RFC's status.

This is part of `accepted-scope` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Each listed way of arriving at a stop, including choosing the row that is already current after the code was scrolled elsewhere, leaves the stop's file open in Review with its lines revealed, and no Show in code button exists. | `qt_tour_stop_acceptance_card`, composition and cold-import courses | Pass | `93b1045a8` |
| **AC02:** For each row of the state table, a fixture stop in that condition yields that state from the core and that ring colour. | `core_tour_stop_acceptance` (`test_states_and_rings`), `qt_tour_stop_acceptance_card` | Pass | `93b1045a8` |
| **AC03:** Accept on the current stop, Save of an edit, and Accept on that stop's own proposal each leave the stop Accepted; opening the stop, clicking its ring and Accept all on a tour revision do not. | `core_tour_stop_acceptance`, `qt_tour_stop_acceptance_card`, `qt_tour_chat_in_tab`, `application_review_tour_writing` | Pass | `93b1045a8` |
| **AC04:** After a push that changes one stop's lines and the lines one other stop links to, exactly those two stops are Changed since accepted, each with its reason, and every other stop stays Accepted. | `core_diff_semantics` (`test_tour_acceptance_after_push`); composed `application_review_tour_publication_push` on `tour-push-v1` ([[BPR-2117]]), negative control fails | Pass | `93b1045a8`, `1d5558c89` |
| **AC05:** Publication is refused while any stop is yellow, naming each with its reason, and allowed once all are Accepted; the published result for an all-Accepted tour equals the result before this change. | `core_tour_stop_acceptance` (`test_publication`); the publication courses | Pass | `93b1045a8` |
| **AC06:** A stop at rest shows only the listed items; Use selected lines, Enhance, Remove, Cancel and Save appear only while it is edited; no Ready, next button and no Head changed chip exists; and every operation offered before this change is still reachable. | `qt_tour_stop_acceptance_card`, `qt_tour_stop_card_cleanup`, `qt_workbench_window_scale_*` | Pass | `93b1045a8` |
| **AC07:** Suggest an outline and Insert outline are absent from the application, `bpr tour outline` behaves as before, and a saved draft with outline-suggested stops opens with them Not accepted yet and can be accepted and published. | `qt_tour_stop_acceptance_card`, agent setup and failure rows, `core_tour_stop_acceptance` (`test_carry_saved_drafts`), `sqlite_review_tour_storage`, `application_review_tour_cli_*` | Pass | `93b1045a8` |
| **AC08:** The owner accepts the stop at rest, being edited, and in each ring state on a frozen build. | The owner's look on frozen builds | Pass: owner accepted (sessions `679d26ab…`, `7c920675…`) | `93b1045a8` |

## Closeout (2026-11-11)

Every criterion judged against the [ledger](../../verification/RFC-BPR-0106.md). AC04 now passes through the window on real code ([[BPR-2117]]). Documentation gate run before this commit. RFC-BPR-0106 is Auto-Verified.
