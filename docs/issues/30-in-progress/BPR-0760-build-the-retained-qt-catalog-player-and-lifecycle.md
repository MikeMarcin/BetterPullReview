---
schema: "implement-rfc.card.v1"
id: "BPR-0760"
title: "Build the retained Qt catalog player and lifecycle"
status: "In Progress"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0754", "BPR-0758"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"runtime-foundation","acceptance":["B1","B2","B9","B10"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
---

# BPR-0760: Build the retained Qt catalog player and lifecycle

## Outcome

Implement focused cache/descriptor/sequencer/widget helpers and test with fixture assets before final art is accepted. No new framework or per-tick resource work.

## Acceptance

- All clip indices/returns/gutters and delayed/invalid elapsed-time cases are checked.
- One timer, coalesced repaint and static/reduced/hidden/modal/shutdown behavior are enforced.
- Cache is explicit, immutable/shareable; paint selects retained rectangles without decode/slice/scale allocation.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
