---
schema: "implement-rfc.card.v1"
id: "BPR-1992"
title: "Choose the publication form, advance its steps and report failures"
status: "Backlog"
rfcs: ["RFC-BPR-0099"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0004-tour-publication-form-and-steps.md"}
depends_on: ["BPR-1991"]
---

# Choose the publication form, advance its steps and report failures

## Scope and acceptance

Implements
[RFC-BPR-0099](../../rfcs/RFC-BPR-0099-tour-publishing-in-reach-and-reported.md).

Remove the mode list; have the plan choose the first form that carries every
stop and state the choice with the stops that forced it; give each publication
step a state in the core's projection and show the advancing checklist in the
panel and the count in the head; replace status-line refusals and failures
with the error-role banner the RFC defines, including the before-sending
refusal that names stops, operation and both ways out.

Covers AC03 to AC06 and the live course of AC08 on MikeMarcin/BPR-Test.
Publication semantics and recovery under RFC-BPR-0040 to 0042 do not change.

This is part of `tour-publishing` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
