---
schema: "implement-rfc.card.v1"
id: "BPR-1093"
title: "Verify syntax, ownership, migration and mutation adversaries"
status: "In Progress"
rfcs: ["RFC-BPR-0047", "RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0051"]
depends_on: ["BPR-1084", "BPR-1085", "BPR-1086", "BPR-1089", "BPR-1092"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-19","milestone":"verification","acceptance":["R40","R41","R42","R43"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1093: Verify syntax, ownership, migration and mutation adversaries

Pack alias: **PACK-CC-19** (live `BPR-1093`, allocated at import).

## Outcome and work

Exercise real core/host/provider/store boundaries for malformed inputs, unknown roots/resolution, duplicate paging, stale edits, account swaps, migration conflicts and interrupted publication. Include prompt injection as untrusted data and native suggestion preservation. Reuse existing isolation harnesses.

## Acceptance

- Every fixture scenario maps to a real product test or a specifically recorded capability limitation.
- No unsafe effect is possible from parsing, settings, preview or local classification.
- Recovery tests prove no duplicate post/event and no stale-body authorization after a label edit.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
