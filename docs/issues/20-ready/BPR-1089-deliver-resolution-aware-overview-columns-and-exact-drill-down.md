---
schema: "implement-rfc.card.v1"
id: "BPR-1089"
title: "Deliver resolution-aware Overview columns and exact drill-down"
status: "Ready"
rfcs: ["RFC-BPR-0049", "RFC-BPR-0050"]
depends_on: ["BPR-1083", "BPR-1087"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-15","milestone":"overview","acceptance":["R20","R21","R23","R24","R27","R31","R33"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"},"sequence_after":["BPR-1088"],"coordinator_amendment":{"date":"2026-10-06","decision":"BPR-1088's landed settings (4a07f26bc) satisfy the dependency; its R20 owner look folds into BPR-1089's owner look"}}
---

# BPR-1089: Deliver resolution-aware Overview columns and exact drill-down

Pack alias: **PACK-CC-15** (live `BPR-1089`, allocated at import).

## Outcome and work

Implement the selected compact Overview headers, numeric rendering, catchall remainder, include-resolved behavior and uncertainty presentation. Sorting and activation use the same accepted conversation population. Preserve file reading/navigation and description geometry.

## Coordinator amendment (2026-10-06)

The coordinator ruled that [[BPR-1088]]'s landed settings (`4a07f26bc`: the
label helper, Overview counts and ordered label chips on the profile's
review behaviour) satisfy this card's dependency on it. BPR-1088 stays
parked for human review only for its R20 owner look, and that look folds
into this card's owner look. BPR-1088 moved from `depends_on` to
`metadata.sequence_after`, as [[BPR-1293]] records its sequence, because the
board keeps a card with an unfinished dependency in Backlog.

## Acceptance

- No-column, typical-subset and all-twelve states show a disjoint correct breakdown.
- Unknown resolution cannot look like a fully clear review, and all-state counts expose their breakdown.
- Keyboard/screen-reader activation opens exactly the counted conversations and retains focus across refresh.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
