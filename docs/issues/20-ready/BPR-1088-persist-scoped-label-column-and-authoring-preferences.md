---
schema: "implement-rfc.card.v1"
id: "BPR-1088"
title: "Persist scoped label-column and authoring preferences"
status: "Ready"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0049"]
depends_on: ["BPR-1082", "BPR-1087"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-14","milestone":"overview","acceptance":["R20","R28","R41"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1088: Persist scoped label-column and authoring preferences

Pack alias: **PACK-CC-14** (live `BPR-1088`, allocated at import).

## Outcome and work

Extend the existing typed profile settings and migration path for ordered label columns and convention/count defaults. Keep active view state in its existing owner. Preserve the unrelated Comments drawer filter, other settings and custom hooks; handle failed saves and concurrent windows.

## Acceptance

- New fields absent in old settings adopt safe defaults without resetting anything else.
- All twelve column choices persist; duplicate/unknown IDs fail safely and explain recovery.
- Reset, cancellation, cross-window update and profile isolation follow the existing authority contract.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
