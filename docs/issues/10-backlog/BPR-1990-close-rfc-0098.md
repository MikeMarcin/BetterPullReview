---
schema: "implement-rfc.card.v1"
id: "BPR-1990"
title: "Verify and close RFC-BPR-0098"
status: "Backlog"
rfcs: ["RFC-BPR-0098"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0002-close-home-next-move.md","draft_title":"Close the Home next-move RFC"}
depends_on: ["BPR-1989"]
closeout_for: ["RFC-BPR-0098"]
---

# BPR-1990: Verify and close RFC-BPR-0098

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0098](../../rfcs/RFC-BPR-0098-home-says-whose-move-my-pull-request-is.md)
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
| **AC01:** For each row of the decision table, a fixture pull request with exactly those facts yields that next move from the core, and a pull request matching several rows yields the first. | Pending | Pending | Not implemented |
| **AC02:** On Home, authored rows appear in My pull requests in the table's order, Waiting rows remain in Waiting on others, and no authored row appears in Needs me. | Pending | Pending | Not implemented |
| **AC03:** A Returned row's sentence names the reviewer who requested changes and the number of conversations awaiting my reply; a Ready row's sentence names its approvers. An approved, mergeable pull request never reads Quiet. | Pending | Pending | Not implemented |
| **AC04:** With the conversation field unavailable, a changes-requested pull request is still Returned, no row states a conversation count, and a pull request with only unanswered conversations is not claimed Returned. | Pending | Pending | Not implemented |
| **AC05:** The Home read for a profile with authored pull requests makes no more requests than before, and its added fields are bounded as stated. | Pending | Pending | Not implemented |
| **AC06:** The owner accepts the rows on a frozen build against their own open pull requests. | Pending | Pending | Not implemented |
