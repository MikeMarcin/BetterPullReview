---
schema: "implement-rfc.card.v1"
id: "PACK-CC-15"
title: "Deliver resolution-aware Overview columns and exact drill-down"
status: "Backlog"
rfcs: ["RFC-BPR-0049", "RFC-BPR-0050"]
depends_on: ["PACK-CC-09", "PACK-CC-13", "PACK-CC-14"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "overview", "acceptance": ["R20", "R21", "R23", "R24", "R27", "R31", "R33"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-15: Deliver resolution-aware Overview columns and exact drill-down

## Outcome and work

Implement the selected compact Overview headers, numeric rendering, catchall remainder, include-resolved behavior and uncertainty presentation. Sorting and activation use the same accepted conversation population. Preserve file reading/navigation and description geometry.

## Acceptance

- No-column, typical-subset and all-twelve states show a disjoint correct breakdown.
- Unknown resolution cannot look like a fully clear review, and all-state counts expose their breakdown.
- Keyboard/screen-reader activation opens exactly the counted conversations and retains focus across refresh.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
