---
schema: "implement-rfc.card.v1"
id: "PACK-CC-22"
title: "Reconcile documentation, simplify and independently review the feature"
status: "Backlog"
rfcs: ["RFC-BPR-0047", "RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0050", "RFC-BPR-0051"]
depends_on: ["PACK-CC-19", "PACK-CC-20"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "verification", "acceptance": ["R43", "R46", "R48"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-22: Reconcile documentation, simplify and independently review the feature

## Outcome and work

Update live architecture/UX/user/CLI/skill docs, registry index and per-criterion evidence. Remove duplicate helpers and obsolete local label assumptions, preserving settled historical sources. Perform an independent scoped review focused on public/private authority, raw-source fidelity and count honesty. Record live/native gaps without certifying them.

## Acceptance

- Documentation matches actual implemented surfaces, default behavior, limits and effective hooks.
- One catalog/parser/aggregation/service path remains, with no model/publisher duplication.
- Review findings and per-criterion results reference producing revisions and unresolved gates accurately.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
