---
schema: "implement-rfc.card.v1"
id: "BPR-1091"
title: "Wire optional convention-aware hooks through the existing workshop"
status: "Ready"
rfcs: ["RFC-BPR-0051"]
depends_on: []
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-17","milestone":"hooks-cli","acceptance":["R19","R34","R35","R36","R37"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"},"sequence_after":["BPR-1090"],"coordinator_amendment":{"date":"2026-10-08","decision":"BPR-1090's landed Review feedback hook and workshop_feedback_guidance() satisfy the dependency; its parked owner Settings look does not block this wiring"}}
---

# BPR-1091: Wire optional convention-aware hooks through the existing workshop

Pack alias: **PACK-CC-17** (live `BPR-1091`, allocated at import).

## Outcome and work

Compose the default fragment only into relevant explicitly invoked tasks. Preserve user overrides and tour purposes. Validate candidate prefixes through the shared parser and display semantic/patch changes. Keep runtime-supplied provenance and permitted context boundaries intact.

## Coordinator amendment (2026-10-08)

The coordinator ruled that [[BPR-1090]]'s landed Review feedback hook and
`workshop_feedback_guidance()` satisfy this card's dependency. BPR-1090 stays
parked only for the owner's Settings look, which does not block this wiring.
BPR-1090 moved from `depends_on` to `metadata.sequence_after`, as
[[BPR-1089]] did, because the board keeps a card with an unfinished dependency
in Backlog. Coordinator direction for this card:
- Send the guidance with comment-workshop requests only while the label
  helper is on.
- No model calls in tests; use the fake agent harness.
- Leave the owner look open.

## Acceptance

- Default, customized, disabled and no-agent fixtures dispatch the expected content with no automatic launch.
- Prose-only enhancements preserve uncertainty and patch bytes unless a visible candidate proposes a change.
- Candidate acceptance is local only; prompts cannot bypass publication or native review authority.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
