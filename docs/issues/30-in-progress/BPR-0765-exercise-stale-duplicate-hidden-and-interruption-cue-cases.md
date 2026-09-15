---
schema: "implement-rfc.card.v1"
id: "BPR-0765"
title: "Exercise stale, duplicate, hidden and interruption cue cases"
status: "In Progress"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0764"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"context-integration","acceptance":["B7","B8","B9"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# BPR-0765: Exercise stale, duplicate, hidden and interruption cue cases

## Outcome

Use existing fixture/provider/agent seams to test realistic accepted and rejected public flows, including duplicate delivery, new invocation, profile/window/PR switch and local draft conflict.

## Acceptance

- Each negative case suppresses the wrong gesture without suppressing the authoritative text/result.
- Hidden results do not replay; old-revision continuation remains explicitly bound/labeled.
- Cooldown/coalescing/preemption never causes an animation queue or delays navigation/errors.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
