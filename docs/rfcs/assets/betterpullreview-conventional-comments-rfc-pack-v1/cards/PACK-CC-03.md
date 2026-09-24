---
schema: "implement-rfc.card.v1"
id: "PACK-CC-03"
title: "Implement revisioned header edits and root-post classification"
status: "Backlog"
rfcs: ["RFC-BPR-0047"]
depends_on: ["PACK-CC-02"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "semantics", "acceptance": ["R05", "R07", "R08", "R41"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-03: Implement revisioned header edits and root-post classification

## Outcome and work

Add pure header-edit decisions bound to the exact body revision, with byte spans suitable for deliberate Qt conversion. Resolve root posts using existing normalized identity/linkage. Update derived annotations when accepted bodies change; preserve reply-local annotations and missing/deleted root uncertainty.

## Acceptance

- No-op and round-trip source bytes, LF/CRLF, Unicode and patch suffixes are preserved.
- A root edit moves its own classification; a differently labelled reply does not.
- Stale revisions and scope replacements reject old edits/results without losing the current draft.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
