---
schema: "implement-rfc.card.v1"
id: "BPR-1086"
title: "Integrate private tracking, promotion and ordinary tour comments"
status: "Ready"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0051"]
depends_on: ["BPR-1082", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-12","milestone":"authoring","acceptance":["R16","R17","R18","R19"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1086: Integrate private tracking, promotion and ordinary tour comments

Pack alias: **PACK-CC-12** (live `BPR-1086`, allocated at import).

## Outcome and work

Reuse explicit defect classification/verification and available note/finding promotion into the shared draft. Keep severity/provenance private. Exercise ordinary and deliberately labelled tour comments against the inherited segment/order/publication contracts. Record absent deferred interfaces without duplicating them.

## Acceptance

- Issue recognition offers tracking but does not silently create or close defects.
- Website resolution, patch application and relabelling cannot verify or withdraw a private defect.
- Promotion preserves lineage and tours preserve order/links/hooks with no mandatory taxonomy.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
