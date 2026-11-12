---
schema: "implement-rfc.card.v1"
id: "BPR-1997"
title: "Verify and close RFC-BPR-0100"
status: "Done"
rfcs: ["RFC-BPR-0100"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0009-close-tour-conversation.md","draft_title":"Close the tour conversation RFC"}
depends_on: ["BPR-1995", "BPR-1996", "BPR-2116"]
closeout_for: ["RFC-BPR-0100"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R qt_tour_side_chat"}},{"criterion":"AC02","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R qt_tour_side_chat"}},{"criterion":"AC03","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"See the closeout checklist and ledger (live-tour-requests 5/5 for AC03)","command":"ctest --preset dev -R application_tour_agent_conversation"}},{"criterion":"AC04","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R qt_tour_agent_session_view"}},{"criterion":"AC05","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'qt_tour_side_chat|application_review_tour_writing|core_tour_references'"}},{"criterion":"AC06","outcome":"pass","revision":"f042726a674dc2436156529ba6c8b5bb85c53c6d","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'qt_tour_side_chat|application_review_tour_publication_native'"}},{"criterion":"AC07","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R 'application_tour_agent_conversation|application_writing_delivery'"}},{"criterion":"AC08","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"observation","detail":"Owner accepted the flag-move drafted tour (decision 1180b2e4, session c46887c9)"}},{"criterion":"AC09","outcome":"pass","revision":"e6e84653e78106d8561193f6252e9cc45c6b3e22","evidence":{"kind":"observation","detail":"Owner accepted the lower edge (8736f969) and the revision card (e1ad5848), session c46887c9"}}]
---

# BPR-1997: Verify and close RFC-BPR-0100

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0100](../../rfcs/RFC-BPR-0100-tour-conversation-that-changes-the-tour-in-one-turn.md)
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
| **AC01:** With a native session and an unmoved head, nothing but the composer and the session line is shown under the transcript; a replay or a moved head each adds exactly its one notice. | `qt_tour_side_chat` (8): only the composer and the session line; one notice each | Pass on the fixture; owner look accepted (AC09) | `e6e84653e` |
| **AC02:** In each of the four thread kinds, an answer that proposed nothing is followed by the link naming that part, and following it sends the update turn. No other control offers the update. | `qt_tour_side_chat`: the link in each thread kind, naming the part; no Update button | Pass | `e6e84653e` |
| **AC03:** The whole-tour request and response contracts state that a change request is answered with its revision in that turn; with an installed agent on a fixture tour, each of five fixed change requests yields a proposal card in its first answer. | `application_tour_agent_conversation`; live `live-tour-requests` (Claude Code 2.1.319, Sonnet) | Pass: 5/5 cards in the first answer | `e6e84653e` |
| **AC04:** Accept all applies every operation of a revision; Review each leaves them grey in the tour; settling the revision in the tour settles the chat card. | `qt_tour_agent_session_view`: Accept all, Review each, settling in the tour | Pass on the fixture; owner look accepted (AC09) | `e6e84653e` |
| **AC05:** Enhance on a stop with prior conversation sends a turn in that stop's session, and its proposal changes the stop only on Accept. Enhance on a selection replaces only the selection. | `qt_tour_side_chat`, `application_review_tour_writing`, `core_tour_references` | Pass | `e6e84653e` |
| **AC06:** A stop text containing `path:first-last` shows a link that opens those lines in Review; the same reference alone on a line shows the snippet. The published comment contains the commit-pinned permalink in the form established on BPR-Test. | `qt_tour_side_chat` (10), `application_review_tour_publication_native`, BPR-Test embed check | Pass, a new publication and a tour update of an edited published stop ([[BPR-2116]]) | `e6e84653e`, `f042726a6` |
| **AC07:** The citation rule and the moved-code rule are present in the drafting prompt, the conversation contract and the stop-comment hook, and the guide and hook versions differ from the inspected ones. | `application_tour_agent_conversation`, `application_writing_delivery` | Pass: guide v2, `tour-stop-comment-v2` | `e6e84653e` |
| **AC08:** On a fixture pull request that moves an unchanged path behind a flag, the owner accepts that a drafted tour's stops do not describe the retained path's behaviour. | The owner's judgement of the `flag-move-v1` drafted tour | Pass: owner accepted (`1180b2e4`, session `c46887c9`) | `e6e84653e` |
| **AC09:** The owner accepts the side chat's lower edge and the revision card on a frozen build. | The owner's look on the frozen build | Pass: owner accepted the lower edge (`8736f969`) and the card (`e1ad5848`), session `c46887c9` | `e6e84653e` |

## Closeout (2026-11-11)

Every criterion judged against the [ledger](../../verification/RFC-BPR-0100.md). AC06 now holds on the update path too ([[BPR-2116]], `f042726a6`); AC08 and AC09 were accepted by the owner in session `c46887c9`. Known limits that remain are outside the criteria (the Problem and Solution are edited as source in the Tour tab; `qt_tour_agent_session_view` under the native-look environment). Documentation gate run before this commit. RFC-BPR-0100 is Auto-Verified.
