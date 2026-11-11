---
schema: "implement-rfc.card.v1"
id: "BPR-2015"
title: "Verify and close RFC-BPR-0106"
status: "Backlog"
rfcs: ["RFC-BPR-0106"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0027-close-tour-stops.md","draft_title":"Close the tour stop RFC"}
depends_on: ["BPR-2013", "BPR-2014", "BPR-2117"]
closeout_for: ["RFC-BPR-0106"]
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
| **AC01:** Each listed way of arriving at a stop, including choosing the row that is already current after the code was scrolled elsewhere, leaves the stop's file open in Review with its lines revealed, and no Show in code button exists. | Pending | Pending | Not implemented |
| **AC02:** For each row of the state table, a fixture stop in that condition yields that state from the core and that ring colour. | Pending | Pending | Not implemented |
| **AC03:** Accept on the current stop, Save of an edit, and Accept on that stop's own proposal each leave the stop Accepted; opening the stop, clicking its ring and Accept all on a tour revision do not. | Pending | Pending | Not implemented |
| **AC04:** After a push that changes one stop's lines and the lines one other stop links to, exactly those two stops are Changed since accepted, each with its reason, and every other stop stays Accepted. | Pending | Pending | Not implemented |
| **AC05:** Publication is refused while any stop is yellow, naming each with its reason, and allowed once all are Accepted; the published result for an all-Accepted tour equals the result before this change. | Pending | Pending | Not implemented |
| **AC06:** A stop at rest shows only the listed items; Use selected lines, Enhance, Remove, Cancel and Save appear only while it is edited; no Ready, next button and no Head changed chip exists; and every operation offered before this change is still reachable. | Pending | Pending | Not implemented |
| **AC07:** Suggest an outline and Insert outline are absent from the application, `bpr tour outline` behaves as before, and a saved draft with outline-suggested stops opens with them Not accepted yet and can be accepted and published. | Pending | Pending | Not implemented |
| **AC08:** The owner accepts the stop at rest, being edited, and in each ring state on a frozen build. | Pending | Pending | Not implemented |
