---
schema: "implement-rfc.card.v1"
id: "PACK-CC-27"
title: "Verify and close convention-aware hooks and CLI parity"
status: "Backlog"
rfcs: ["RFC-BPR-0051"]
depends_on: ["PACK-CC-26"]
closeout_for: ["RFC-BPR-0051"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "closeout", "acceptance": ["R34", "R35", "R36", "R37", "R38", "R39", "R41", "R48"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-27: Verify and close convention-aware hooks and CLI parity

## Outcome and work

Reconcile effective default/override hook behavior and shared headless semantics through actual composition. Verify no required core operation needs a model, no nested agent/publisher appeared, and inherited tour paths remain intact.

## Acceptance

- Default/custom/disabled/no-agent modes and cross-interface conflicts pass their actual boundaries.
- Published CLI grammar/schema and canonical skill source match the shipped implementation.
- Final report names exact remaining limitations and only records earned status transitions.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
