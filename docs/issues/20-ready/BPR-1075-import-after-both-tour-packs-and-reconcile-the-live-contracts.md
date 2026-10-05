---
schema: "implement-rfc.card.v1"
id: "BPR-1075"
title: "Import after both tour packs and reconcile the live contracts"
status: "Ready"
rfcs: ["RFC-BPR-0047", "RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0050", "RFC-BPR-0051"]
depends_on: []
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-01","milestone":"intake","acceptance":["R19","R39","R47"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1075: Import after both tour packs and reconcile the live contracts

Pack alias: **PACK-CC-01** (live `BPR-1075`, allocated at import).

## Outcome and work

Verify this archive. Read actual predecessor manifests/import maps, live HEAD, source drift and the installed import interface. Allocate the five RFCs and 27 cards without touching settled identities. Record the tour body-segment, shared draft, headless owner and hook ownership seams. Adopt the eight milestones; document only necessary representation changes.

## Acceptance

- The complete RFC/issue map and returned allocator receipt/counter paths are committed.
- The imported plan covers each live card exactly once; predecessor hashes/ranges are verified or explicitly qualified.
- Current versus deferred promotion/placement paths and exact inherited hook/CLI owners are identified.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
