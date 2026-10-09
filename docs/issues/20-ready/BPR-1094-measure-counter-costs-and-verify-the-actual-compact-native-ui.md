---
schema: "implement-rfc.card.v1"
id: "BPR-1094"
title: "Measure counter costs and verify the actual compact native UI"
status: "Ready"
rfcs: ["RFC-BPR-0049", "RFC-BPR-0050", "RFC-BPR-0051"]
depends_on: ["BPR-1083", "BPR-1089", "BPR-1092"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-20","milestone":"verification","acceptance":["R29","R31","R32","R33","R44"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1094: Measure counter costs and verify the actual compact native UI

Pack alias: **PACK-CC-20** (live `BPR-1094`, allocated at import).

## Outcome and work

Measure the inherited large-review workload and a justified larger synthetic core workload, including 0/3/12 columns and 1k/10k normalized conversations where admitted. Inspect actual composed native states at selected scales/themes/input modes; reuse unchanged coverage.

## Acceptance

- Record hardware/build/workload and observed work/allocation/latency, not invented universal thresholds.
- A column toggle makes no network request or per-body parse; a root edit updates bounded relevant state.
- Rendered application evidence demonstrates label identity, state/coverage clarity, full content and usable keyboard/scroll behavior.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
