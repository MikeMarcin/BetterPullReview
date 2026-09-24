---
schema: "implement-rfc.card.v1"
id: "PACK-CC-04"
title: "Implement conversation partition and explicit resolution coverage"
status: "Backlog"
rfcs: ["RFC-BPR-0047", "RFC-BPR-0049"]
depends_on: ["PACK-CC-03"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "semantics", "acceptance": ["R21", "R22", "R23", "R24", "R25", "R26", "R43"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-04: Implement conversation partition and explicit resolution coverage

## Outcome and work

Implement a pure normalized conversation accounting policy with unresolved/resolved/unknown/not-applicable lanes, exact deduplication, root labels and coverage. Adapt the supplied oracle cases to independent core tests. Track partial classification separately from a truly plain catchall body.

## Acceptance

- Dedicated counters plus catchall equal the included population for all 4,096 subsets.
- Draft/private/deleted units and duplicate pages obey the documented inclusion rules.
- Missing resolution/root/anchor and incomplete collection cases never produce a false complete clean result.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
