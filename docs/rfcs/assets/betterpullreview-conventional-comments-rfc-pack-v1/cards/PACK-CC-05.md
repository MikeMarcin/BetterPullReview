---
schema: "implement-rfc.card.v1"
id: "PACK-CC-05"
title: "Develop compact authoring, counter and resolution UX candidates"
status: "Backlog"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0050"]
depends_on: ["PACK-CC-01"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "design", "acceptance": ["R30", "R33"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-05: Develop compact authoring, counter and resolution UX candidates

## Outcome and work

Inspect the current tour-enabled workbench before proposing changes. Produce a small set of concrete candidates covering optional authoring, native review preflight, Settings and Overview with zero/typical/all label columns. Include unknown coverage and labelled replies. Do not prescribe icons from the chat.

## Acceptance

- Candidates demonstrate actual narrow/wide constraints, long content and full twelve-column overflow behavior.
- Each candidate explains counting units, catchall remainder, native review choices and public/local ownership.
- Tradeoffs are recorded without claiming a mockup is implemented.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
