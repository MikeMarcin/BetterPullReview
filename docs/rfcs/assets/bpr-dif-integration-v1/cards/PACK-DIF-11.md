---
schema: "implement-rfc.card.v1"
id: "PACK-DIF-11"
title: "Prove Home, profile preference and shared-cache behavior"
status: "Backlog"
rfcs: ["RFC-BPR-0031"]
depends_on: ["PACK-DIF-10"]
metadata: {"pack": "bpr-dif-integration-v1", "milestone": "home-integration", "acceptance": ["B2", "B3", "B5", "B9", "B10", "B11"], "alias_requires_live_allocation": true}
---

# PACK-DIF-11: Prove Home, profile preference and shared-cache behavior

## Outcome

Run one focused app course through real gestures, restart, two windows and two profiles; compare timers/decodes/reading state rather than repeatedly rebuilding everything.

## Acceptance

- Actual rendered frames differ at expected full-motion times; reduced intervals remain static.
- Same-profile preference fan-out and cross-profile isolation pass; one shared decode lifetime is demonstrated.
- Home/input/return-to-review regression checks and hide/deactivate behavior pass.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
