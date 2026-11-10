---
schema: "implement-rfc.card.v1"
id: "BPR-2006"
title: "Verify and close RFC-BPR-0103"
status: "Backlog"
rfcs: ["RFC-BPR-0103"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0018-close-agent-work-visible.md","draft_title":"Close the agent work visibility RFC"}
depends_on: ["BPR-2004", "BPR-2005"]
closeout_for: ["RFC-BPR-0103"]
---

# BPR-2006: Verify and close RFC-BPR-0103

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0103](../../rfcs/RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md)
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
| **AC01:** For each installed agent and transport BPR supports, a recorded course states how background work is reported and whether the agent is run with helpers visible or foreground only. | Pending | Pending | Not implemented |
| **AC02:** With a scripted runtime that starts three helpers, writes a message, and reports `result` before they finish, the turn stays running, shows three helper rows with their states, accepts the later frames, and ends once after the closing message with one duration and one cost. | Pending | Pending | Not implemented |
| **AC03:** With the same script and one helper that never finishes before the connection closes, the turn ends as an error that names the unfinished helper and keeps the text that arrived. | Pending | Pending | Not implemented |
| **AC04:** A frame that arrives when no turn is open is refused as before. | Pending | Pending | Not implemented |
| **AC05:** After 120 seconds without a frame the notice appears with both choices; a new frame clears it; Keep waiting leaves the run untouched. | Pending | Pending | Not implemented |
| **AC06:** During a turn the block names the file being read or the command being run as the runtime reports them, and "last activity" resets on every frame. | Pending | Pending | Not implemented |
| **AC07:** With an installed agent on MikeMarcin/BPR-Test, a request that makes the agent start helpers either shows them and completes in one turn, or runs foreground only and completes in one turn. | Pending | Pending | Not implemented |
| **AC08:** The tour side chat shows the same block and the same endings. | Pending | Pending | Not implemented |
| **AC09:** The owner accepts the running, quiet and ended states on a frozen build. | Pending | Pending | Not implemented |
