---
schema: "implement-rfc.card.v1"
id: "PACK-CC-23"
title: "Verify and close conventional-comment semantics"
status: "Backlog"
rfcs: ["RFC-BPR-0047"]
depends_on: ["PACK-CC-22"]
closeout_for: ["RFC-BPR-0047"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "closeout", "acceptance": ["R01", "R02", "R03", "R04", "R05", "R06", "R07", "R08", "R40", "R43", "R46", "R47", "R48"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-23: Verify and close conventional-comment semantics

## Outcome and work

Reconcile every RFC0047 acceptance row against exact core/host evidence and current source. Follow live canonical order; retain any unsupported/limited grammar behavior explicitly. Do not use pack self-validation as product proof.

## Acceptance

- All owned requirements have direct current evidence or remain open.
- Live aliases are mapped and source/identity invariants are independently reviewed.
- Status changes obey existing predecessor/closeout rules without historical edits.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
