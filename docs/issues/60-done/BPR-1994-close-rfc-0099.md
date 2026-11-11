---
schema: "implement-rfc.card.v1"
id: "BPR-1994"
title: "Verify and close RFC-BPR-0099"
status: "Done"
rfcs: ["RFC-BPR-0099"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0006-close-tour-publishing.md","draft_title":"Close the tour publishing RFC"}
depends_on: ["BPR-1991", "BPR-1992", "BPR-1993"]
closeout_for: ["RFC-BPR-0099"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"f51063b5ba00693c0d7d76f0af8677860660f95d","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R qt_review_tour_publication_working"}},{"criterion":"AC02","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'core_tour_publication_head|qt_review_tour_publication_working|application_review_tour_publication_stop'"}},{"criterion":"AC03","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_review_tour_publication_"}},{"criterion":"AC04","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'review_submission_application_host|qt_review_tour_publication_working|application_review_tour_publication_stop'"}},{"criterion":"AC05","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'qt_review_tour_publication_working|application_review_tour_publication_(conflict|description|stop)'"}},{"criterion":"AC06","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_review_tour_publication_unsupported"}},{"criterion":"AC07","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'qt_workbench_window_scale|core_tour_publication_head'"}},{"criterion":"AC08","outcome":"pass","revision":"33c282ccdf87eaee5975fb5e5327c256b9efb01a","evidence":{"kind":"observation","detail":"Agent-run live course on BPR-Test #177: six steps waiting/sending/sent, head count equal to panel's; one COMMENT review 5854708476 with stop 1, conversation comment 6788770868 for stop 2, description linked; same shape as the hand-chosen mixed mode"}},{"criterion":"AC09","outcome":"pass","revision":"bce0d0eba3f6f73e9d8744e243b4c47e250f7f6b","evidence":{"kind":"observation","detail":"Owner accepted all four candidates in attended session 5127270e"}}]
---

# BPR-1994: Verify and close RFC-BPR-0099

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0099](../../rfcs/RFC-BPR-0099-tour-publishing-in-reach-and-reported.md)
against actual evidence from the cards this one depends on, including its
live, installed-agent and owner-attended criteria, the reconciliation of the
live RFCs it amends, and the documentation gate. Record failures and unreached
requirements exactly.

Leave live IDs, the implementation baseline and acceptance records to
authorized execution. This proposed closeout carries no passing evidence and
changes no live RFC's status.

This is part of `accepted-scope` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** With a route long enough to scroll, the head's publication action and the agent bar remain visible and operable at the top and bottom of the Tour tab while the route scrolls between them. (Amended 2026-11-11 at closeout: the owner's layout decision "A + footer 1" ([[BPR-2051]]) removed the agent bar; the agent picker is the window's footer, outside the tab, and Draft the tour is in the route's head. The criterion is met by the head's publication action staying visible and operable at the top while the route scrolls.) | `qt_review_tour_publication_working` (`test_bands`, as updated by BPR-2051): 24 stops; the head and Publish… stay put at the top while the route scrolls; the agent picker is the window footer's (amended 2026-11-11) | Pass (as amended) | `f51063b5b` |
| **AC02:** The head shows each state of the table for a tour in that state, and Publish… opens the tour page with its preview present. | `core_tour_publication_head`; `test_head_states`; `application_review_tour_publication_stop` | Pass | `3d97ea548` |
| **AC03:** No mode list is offered. For fixture tours whose stops need one review, a mixed tour and compatibility comments, the plan chooses that form and the panel's sentence names the stops that forced it. | No mode list; `native`, `mixed`, `file`, `compat` courses choose their forms and name the stops | Pass | `3d97ea548` |
| **AC04:** During a publication of several steps, each step is shown waiting, then sending, then sent, in order, and the head's count agrees with the panel's at every step. | `review_submission_application_host`, `qt_review_tour_publication_working`, `_stop`; live in AC08 | Pass | `3d97ea548` |
| **AC05:** When a step fails after earlier steps were sent, the banner names the step, the reason, what is on GitHub and what is not, with Resume primary; nothing in the failure is drawn in the placeholder colour. | `qt_review_tour_publication_working` banner; `_conflict`, `_description`, `_stop` | Pass | `3d97ea548` |
| **AC06:** A tour no form can carry on the current connection is refused before any write, with the stops, the operation and both ways out named. | `application_review_tour_publication_unsupported`: refused before any write, both ways out | Pass | `3d97ea548` |
| **AC07:** On Overview, Problem and Solution render as Markdown, a stop row opens that stop in Review, and the state line and actions equal the Tour tab head's for the same tour. | `qt_workbench_window_scale_{100,125,200}` Overview tour; `core_tour_publication_head` | Pass | `3d97ea548` |
| **AC08:** A live publication of a mixed tour on MikeMarcin/BPR-Test shows the advancing steps and produces the same GitHub result as the same tour published with the mode chosen by hand before this change. | Agent-run live course `tests/end_to_end/live_session_course.py` on disposable BPR-Test #177 | Pass: six steps waiting, sending, sent with the head's count equal to the panel's; one COMMENT review with stop 1, stop 2 as a conversation comment at the earlier head, the description linked ([record](../../verification/RFC-BPR-0099.md#agent-run-live-course-2026-11-10)) | `33c282ccd` |
| **AC09:** The owner accepts the Tour tab, the publication panel in its running and stopped states, and the Overview summary on a frozen build. | Owner-attended session `5127270e92b1b77ceae8f9df8356c1f6`, four candidates on the frozen build of `bce0d0eba` | Pass: all four accepted, 2026-11-10 ([record](../../verification/RFC-BPR-0099.md#criteria)) | `bce0d0eba` |

## Closeout (2026-11-11)

Every criterion judged against the [ledger](../../verification/RFC-BPR-0099.md). AC01 was reconciled with the owner's layout decision of 2026-11-11 ("A + footer 1", [BPR-2051 design record](../../design/BPR-2051-agents-roster-and-footer/README.md)), which removed the Tour tab's agent bar: the picker is the window footer's and Draft the tour is the route's head (RFC-BPR-0106 already says so). The RFC records the amendment; the owner accepted the footer and the tour panel layout in session `7c920675…`. AC08 is the agent-run live course the owner moved it to. Documentation gate run before this commit. RFC-BPR-0099 is Auto-Verified.
