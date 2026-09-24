---
schema: "implement-rfc.card.v1"
id: "PACK-CC-13"
title: "Project accepted label buckets and bounded counter deltas"
status: "Backlog"
rfcs: ["RFC-BPR-0049"]
depends_on: ["PACK-CC-04", "PACK-CC-09"]
metadata: {"pack": "betterpullreview-conventional-comments-rfc-pack-v1", "milestone": "overview", "acceptance": ["R25", "R26", "R27", "R29"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "betterpullreview-conventional-comments-rfc-pack-v1", "path": "docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# PACK-CC-13: Project accepted label buckets and bounded counter deltas

## Outcome and work

Adapt the current accepted read model into normalized core inputs, using native resolution observations and REST unknown states. Cache annotations by exact body identity/revision. Publish coherent per-conversation/file/ancestor deltas and explicit stale/partial coverage.

## Acceptance

- Website-only body edits and resolve/reopen changes update the correct population without requiring a new head.
- No duplicate native/REST/tour-link entries or guessed file mappings appear.
- Unchanged bodies are not reparsed and changing columns performs no provider requests.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow `verification/PRODUCT-VERIFICATION.md` and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
