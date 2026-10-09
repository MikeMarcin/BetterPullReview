---
schema: "implement-rfc.card.v1"
id: "BPR-1092"
title: "Expose deterministic convention operations through the shared headless CLI"
status: "In Progress"
rfcs: ["RFC-BPR-0049", "RFC-BPR-0051"]
depends_on: ["BPR-1088", "BPR-1089"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-18","milestone":"hooks-cli","acceptance":["R36","R38","R39","R41"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"},"sequence_after":["BPR-1091"],"coordinator_amendment":{"date":"2026-10-08","decision":"BPR-1091's landed workshop wiring satisfies the dependency; its parked owner look does not block this card"}}
---

# BPR-1092: Expose deterministic convention operations through the shared headless CLI

Pack alias: **PACK-CC-18** (live `BPR-1092`, allocated at import).

## Outcome and work

Inspect and extend the actual imported CLI grammar minimally for catalog, parse/preview, exact shared-draft edits and label-resolution breakdowns of already available data. Use current C++ services/owner handoff. Document actual command and schema additions; keep human output default and JSON explicit.

## Coordinator amendment (2026-10-08)

The coordinator ruled that [[BPR-1091]]'s landed workshop wiring satisfies
this card's dependency. BPR-1091 stays parked only for its owner look, which
does not block this card. BPR-1091 moved from `depends_on` to
`metadata.sequence_after`, as [[BPR-1089]] and BPR-1091 did. BPR-1088 and
BPR-1089 are Done.

## Acceptance

- CLI and desktop produce identical annotations/counts/draft results for the same fixture identity.
- Headless operation opens no GUI, creates no shadow store and uses no independent publisher/regex.
- Cross-interface edit conflicts, missing capabilities and unknown coverage return honest structured/human errors.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
