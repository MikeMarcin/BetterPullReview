---
schema: "implement-rfc.card.v1"
id: "BPR-0766"
title: "Run focused target-native resource and lifecycle verification"
status: "In Progress"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0762", "BPR-0765"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"verification-closeout","acceptance":["B1","B4","B9","B10","B12"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# BPR-0766: Run focused target-native resource and lifecycle verification

## Outcome

Run the integration course on the primary available target; add only relevant Windows/macOS/Linux build/resource/observer/lifecycle evidence using current platform lanes.

## Acceptance

- Real app and tests-disabled target select identical accepted resources; core-only excludes them.
- Support/failure/unsupported motion observations and package/resource behavior are documented per claimed target.
- Missing targets are not represented as passing; no default full Cartesian retest or live PR mutation.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
