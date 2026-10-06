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

## Scope decision — 2026-10-06

The pack's "connect to existing suggestion composition/preview" has no target
for ordinary pull-request comments. The only suggestion composer is Review
Cycles' (`suggestion_composer`, `review_cycle_suggestion`). A comment carries
a hand-typed ```` ```suggestion ```` fence, and native or fallback is decided
only at Publish (`classify_review_body` → `incompatible_body`). The integrator
approved this scope on 2026-10-06, as coordinator:

1. **R12 byte preservation:** a matrix through every label and prefix edit the
   core offers (multiline, deletion, range, CRLF and tab, Unicode, two fences),
   with every byte outside the header span and every semantic anchor
   unchanged.
2. **A decision before Publish:** a pure core decision — native, textual-only
   or no patch — from the body and the existing placement and capability
   facts, with no line relocation. It is shown in the comment draft editor
   **and in [[BPR-1290]]'s in-place Reply editor**, since replies can carry
   fences too. It is shown as a small mark or tooltip-level text only, per the
   owner's minimal-text preference.
3. **Public composition on the local fixture:** issue-with-patch,
   suggestion-without-patch and unsupported-native-patch. No live GitHub.

No new suggestion editor is built. The UI-visible change keeps the owner-look
criterion open for a Mac session. The frontend-design skill is not installed
on this station, so the presentation follows the existing editor patterns.
