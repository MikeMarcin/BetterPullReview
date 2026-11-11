---
schema: "implement-rfc.card.v1"
id: "BPR-1994"
title: "Verify and close RFC-BPR-0099"
status: "Backlog"
rfcs: ["RFC-BPR-0099"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0006-close-tour-publishing.md","draft_title":"Close the tour publishing RFC"}
depends_on: ["BPR-1991", "BPR-1992", "BPR-1993"]
closeout_for: ["RFC-BPR-0099"]
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
| **AC01:** With a route long enough to scroll, the head's publication action and the agent bar remain visible and operable at the top and bottom of the Tour tab while the route scrolls between them. | Pending | Pending | Not implemented |
| **AC02:** The head shows each state of the table for a tour in that state, and Publish… opens the tour page with its preview present. | Pending | Pending | Not implemented |
| **AC03:** No mode list is offered. For fixture tours whose stops need one review, a mixed tour and compatibility comments, the plan chooses that form and the panel's sentence names the stops that forced it. | Pending | Pending | Not implemented |
| **AC04:** During a publication of several steps, each step is shown waiting, then sending, then sent, in order, and the head's count agrees with the panel's at every step. | Pending | Pending | Not implemented |
| **AC05:** When a step fails after earlier steps were sent, the banner names the step, the reason, what is on GitHub and what is not, with Resume primary; nothing in the failure is drawn in the placeholder colour. | Pending | Pending | Not implemented |
| **AC06:** A tour no form can carry on the current connection is refused before any write, with the stops, the operation and both ways out named. | Pending | Pending | Not implemented |
| **AC07:** On Overview, Problem and Solution render as Markdown, a stop row opens that stop in Review, and the state line and actions equal the Tour tab head's for the same tour. | Pending | Pending | Not implemented |
| **AC08:** A live publication of a mixed tour on MikeMarcin/BPR-Test shows the advancing steps and produces the same GitHub result as the same tour published with the mode chosen by hand before this change. | Pending | Pending | Not implemented |
| **AC09:** The owner accepts the Tour tab, the publication panel in its running and stopped states, and the Overview summary on a frozen build. | Owner-attended session `5127270e92b1b77ceae8f9df8356c1f6`, four candidates on the frozen build of `bce0d0eba` | Pass: all four accepted, 2026-11-10 ([record](../../verification/RFC-BPR-0099.md#criteria)) | `bce0d0eba` |
