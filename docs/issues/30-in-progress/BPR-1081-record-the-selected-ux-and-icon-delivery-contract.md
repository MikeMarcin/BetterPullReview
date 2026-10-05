---
schema: "implement-rfc.card.v1"
id: "BPR-1081"
title: "Record the selected UX and icon delivery contract"
status: "In Progress"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0050"]
depends_on: ["BPR-1079", "BPR-1080"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-07","milestone":"design","acceptance":["R30","R31","R32","R33"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1081: Record the selected UX and icon delivery contract

Pack alias: **PACK-CC-07** (live `BPR-1081`, allocated at import).

## Outcome and work

Select and record the layouts, counter-state treatment, label identification, icon resources and required interaction cases in a live RFC-linked design record. Use an existing human gate only where actually required. Preserve partial verdicts; park only dependent work while independent semantics continue.

## Acceptance

- The selected decisions and authority are explicit, with open items not disguised as acceptance.
- Implementation surfaces, textual fallback, focus/overflow and unknown-state treatment are specified.
- The complete icon inventory is bound to the selected source revision and intended packaging path.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
