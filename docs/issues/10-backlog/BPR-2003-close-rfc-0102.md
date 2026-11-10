---
schema: "implement-rfc.card.v1"
id: "BPR-2003"
title: "Verify and close RFC-BPR-0102"
status: "Backlog"
rfcs: ["RFC-BPR-0102"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0015-close-agent-question-subject.md","draft_title":"Close the agent question subject RFC"}
depends_on: ["BPR-2002"]
closeout_for: ["RFC-BPR-0102"]
---

# BPR-2003: Verify and close RFC-BPR-0102

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0102](../../rfcs/RFC-BPR-0102-agent-questions-say-what-they-point-at.md)
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
| **AC01:** From Overview with nothing selected, "Review this pull request" sends, and its request has the pull request as subject, the review entry point, no selection, and every changed file listed as in scope. | Pending | Pending | Not implemented |
| **AC02:** With lines selected, the same suggestion sends the same request: the selection is neither attached nor required. | Pending | Pending | Not implemented |
| **AC03:** With an installed agent on a fixture pull request of at least five changed files and one of them open, the answer to "Review this pull request" discusses more than the open file. | Pending | Pending | Not implemented |
| **AC04:** Selecting lines shows the chip and a typed question's request carries those lines as its subject; ✕ sends the next question with the pull request as subject and leaves the diff's selection in place. | Pending | Pending | Not implemented |
| **AC05:** Escape in the diff clears the selection and the chip; with the find bar open, Escape closes the find bar and the selection remains. | Pending | Pending | Not implemented |
| **AC06:** No scope list is shown, and no suggestion or enabled control in the Agent tab ends in "Not sent. Select the lines…". | Pending | Pending | Not implemented |
| **AC07:** A typed question with nothing selected is sent and answered. | Pending | Pending | Not implemented |
