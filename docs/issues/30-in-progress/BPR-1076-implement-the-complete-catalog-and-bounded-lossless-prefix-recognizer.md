---
schema: "implement-rfc.card.v1"
id: "BPR-1076"
title: "Implement the complete catalog and bounded lossless prefix recognizer"
status: "In Progress"
rfcs: ["RFC-BPR-0047"]
depends_on: ["BPR-1075"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-02","milestone":"semantics","acceptance":["R01","R02","R03","R04","R06","R40"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1076: Implement the complete catalog and bounded lossless prefix recognizer

Pack alias: **PACK-CC-02** (live `BPR-1076`, allocated at import).

## Outcome and work

Implement the twelve-ID catalog and the bounded grammar in the Qt-free core. Return raw/body spans and diagnostic facts; do not reconstruct Markdown. Add the supplied positive/negative corpus and size/pathological tests. Keep guidance separate from explicit expectation and retain custom decorations.

## Acceptance

- Every catalog label has identical feature capability and all documented parser cases pass against real core code.
- Quoted, code, HTML, list, heading and later-paragraph examples cannot classify the outer body.
- Malformed, unknown and limited input remains readable with no effects or destructive normalization.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
