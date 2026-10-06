---
schema: "implement-rfc.card.v1"
id: "BPR-1084"
title: "Preserve and compose native suggested changes"
status: "In Progress"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0050"]
depends_on: ["BPR-1082", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-10","milestone":"authoring","acceptance":["R12","R13","R32"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1084: Preserve and compose native suggested changes

Pack alias: **PACK-CC-10** (live `BPR-1084`, allocated at import).

## Outcome and work

Connect convention-assisted comments to existing suggestion composition/preview and exact native anchor checks. Preserve fences, replacement text and empty/multiline changes during prefix edits. Keep unsupported locations textual and provide the existing precise native/browser path rather than fake apply.

## Acceptance

- The fixture transformations leave every unrelated suggestion byte and semantic anchor unchanged.
- Native and unsupported/fallback contexts are distinguishable without guessed line relocation.
- Issue-with-patch and suggestion-without-patch both work in the public composition.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
