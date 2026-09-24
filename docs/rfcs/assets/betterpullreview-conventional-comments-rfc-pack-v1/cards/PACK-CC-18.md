---
schema: "implement-rfc.card.v1"
id: "PACK-CC-18"
title: "Expose deterministic convention operations through the shared headless CLI"
status: "Backlog"
rfcs: ["RFC-BPR-0049", "RFC-BPR-0051"]
depends_on: ["PACK-CC-14", "PACK-CC-15", "PACK-CC-17"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "hooks-cli", "acceptance": ["R36", "R38", "R39", "R41"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-18: Expose deterministic convention operations through the shared headless CLI

## Outcome and work

Inspect and extend the actual imported CLI grammar minimally for catalog, parse/preview, exact shared-draft edits and label-resolution breakdowns of already available data. Use current C++ services/owner handoff. Document actual command and schema additions; keep human output default and JSON explicit.

## Acceptance

- CLI and desktop produce identical annotations/counts/draft results for the same fixture identity.
- Headless operation opens no GUI, creates no shadow store and uses no independent publisher/regex.
- Cross-interface edit conflicts, missing capabilities and unknown coverage return honest structured/human errors.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
