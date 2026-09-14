---
schema: "implement-rfc.card.v1"
id: "BPR-0759"
title: "Implement bounded native motion observation"
status: "Ready"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0758"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"runtime-foundation","acceptance":["B4","B9"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# BPR-0759: Implement bounded native motion observation

## Outcome

Add/read the reliable per-platform reduction observation through an injected Qt edge. Unsupported is a first-class result; no settings subprocess or polling.

## Acceptance

- System unknown/failure resolves reduced; explicit Full is available.
- Change delivery and observer shutdown are focused-tested; native support claims remain separate from injected tests.
- Any attended OS preference change is restored and platform gaps are recorded.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
