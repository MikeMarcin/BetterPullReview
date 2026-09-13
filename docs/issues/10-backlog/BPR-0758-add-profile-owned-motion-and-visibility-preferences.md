---
schema: "implement-rfc.card.v1"
id: "BPR-0758"
title: "Add profile-owned motion and visibility preferences"
status: "Backlog"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0752"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"runtime-foundation","acceptance":["B3"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# BPR-0758: Add profile-owned motion and visibility preferences

## Outcome

Extend current typed appearance settings/host/SQLite migration and real Appearance controls. Preserve current schema semantics, scoped reset and same-profile multi-window fan-out.

## Acceptance

- Default shown/System, explicit Full/Reduced/hidden and legacy migration are tested through accepted host paths.
- Future/corrupt stores, failed writes, stale completion and session-only labels remain truthful.
- Same-profile windows agree; other profiles and syntax/diff caches are unchanged.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
