---
schema: "implement-rfc.card.v1"
id: "BPR-1083"
title: "Implement selected authoring controls and shared label visuals"
status: "Ready"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0050"]
depends_on: ["BPR-1081", "BPR-1082"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-09","milestone":"authoring","acceptance":["R09","R10","R31","R32","R33"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1083: Implement selected authoring controls and shared label visuals

Pack alias: **PACK-CC-09** (live `BPR-1083`, allocated at import).

## Outcome and work

Implement the selected optional controls, public/root/reply label badges and shared icon resource lookup in current native editors/cards/detail. All twelve and Plain are reachable. Preserve raw body source, normal undo and inherited multi-caret/virtual-space behavior; avoid reformatting incomplete typing.

## Acceptance

- Controls and manually typed syntax show the same draft and projected intent.
- All twelve icons use one resource vocabulary and have accessible identity.
- Labels do not create clipped duplicate composers, new scroll traps or publish effects.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
