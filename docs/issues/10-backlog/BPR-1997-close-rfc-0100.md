---
schema: "implement-rfc.card.v1"
id: "BPR-1997"
title: "Verify and close RFC-BPR-0100"
status: "Backlog"
rfcs: ["RFC-BPR-0100"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0009-close-tour-conversation.md","draft_title":"Close the tour conversation RFC"}
depends_on: ["BPR-1995", "BPR-1996"]
closeout_for: ["RFC-BPR-0100"]
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
| **AC01:** With a native session and an unmoved head, nothing but the composer and the session line is shown under the transcript; a replay or a moved head each adds exactly its one notice. | Pending | Pending | Not implemented |
| **AC02:** In each of the four thread kinds, an answer that proposed nothing is followed by the link naming that part, and following it sends the update turn. No other control offers the update. | Pending | Pending | Not implemented |
| **AC03:** The whole-tour request and response contracts state that a change request is answered with its revision in that turn; with an installed agent on a fixture tour, each of five fixed change requests yields a proposal card in its first answer. | Pending | Pending | Not implemented |
| **AC04:** Accept all applies every operation of a revision; Review each leaves them grey in the tour; settling the revision in the tour settles the chat card. | Pending | Pending | Not implemented |
| **AC05:** Enhance on a stop with prior conversation sends a turn in that stop's session, and its proposal changes the stop only on Accept. Enhance on a selection replaces only the selection. | Pending | Pending | Not implemented |
| **AC06:** A stop text containing `path:first-last` shows a link that opens those lines in Review; the same reference alone on a line shows the snippet. The published comment contains the commit-pinned permalink in the form established on BPR-Test. | Pending | Pending | Not implemented |
| **AC07:** The citation rule and the moved-code rule are present in the drafting prompt, the conversation contract and the stop-comment hook, and the guide and hook versions differ from the inspected ones. | Pending | Pending | Not implemented |
| **AC08:** On a fixture pull request that moves an unchanged path behind a flag, the owner accepts that a drafted tour's stops do not describe the retained path's behaviour. | Pending | Pending | Not implemented |
| **AC09:** The owner accepts the side chat's lower edge and the revision card on a frozen build. | Pending | Pending | Not implemented |
