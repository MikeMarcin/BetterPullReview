---
schema: "implement-rfc.card.v1"
id: "BPR-1085"
title: "Integrate reviewer-scoped preflight with native review decisions"
status: "In Progress"
rfcs: ["RFC-BPR-0048"]
depends_on: ["BPR-1082", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-11","milestone":"authoring","acceptance":["R14","R15","R42"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1085: Integrate reviewer-scoped preflight with native review decisions

Pack alias: **PACK-CC-11** (live `BPR-1085`, allocated at import).

## Outcome and work

Add the selected preflight projection to the current review submission flow. Distinguish own published blockers, selected pending comments, advisory label guidance and unavailable coverage. Retain explicit native decisions and the existing provider/submission journal.

## Acceptance

- Comment, Approve and Request changes use their normal exact native event paths.
- Changes to body/selection/head/account invalidate only applicable prepared actions.
- No label or last-resolution event triggers a review verdict; other reviewers are not attributed to the submitter.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
