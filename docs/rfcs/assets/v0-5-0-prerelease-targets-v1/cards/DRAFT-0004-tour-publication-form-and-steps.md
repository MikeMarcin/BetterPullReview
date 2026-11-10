---
schema: "implement-rfc.card.v1"
id: "DRAFT-0004"
title: "Choose the publication form, advance its steps and report failures"
status: "Backlog"
rfcs: ["RFC-DRAFT-0002"]
depends_on: ["DRAFT-0003"]
---

# Choose the publication form, advance its steps and report failures

## Scope and acceptance

Implements
[RFC-DRAFT-0002](../RFC-DRAFT-0002-tour-publishing-in-reach-and-reported.md).

Remove the mode list; have the plan choose the first form that carries every
stop and state the choice with the stops that forced it; give each publication
step a state in the core's projection and show the advancing checklist in the
panel and the count in the head; replace status-line refusals and failures
with the error-role banner the RFC defines, including the before-sending
refusal that names stops, operation and both ways out.

Covers AC03 to AC06 and the live course of AC08 on MikeMarcin/BPR-Test.
Publication semantics and recovery under RFC-BPR-0040 to 0042 do not change.

This is part of `tour-publishing` in the [plan](../milestone-plan.json).
