---
schema: "implement-rfc.card.v1"
id: "BPR-1082"
title: "Compose convention edits through shared durable draft services"
status: "Ready"
rfcs: ["RFC-BPR-0048"]
depends_on: ["BPR-1077", "BPR-1081"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-08","milestone":"authoring","acceptance":["R09","R10","R11","R41","R42"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1082: Compose convention edits through shared durable draft services

Pack alias: **PACK-CC-08** (live `BPR-1082`, allocated at import).

## Outcome and work

Bind pure prefix edits to current revisioned draft/host/store services, including inherited desktop/CLI ownership. Preserve optimistic conflict candidates, existing save semantics and mutation proposal identity. Do not persist public classification as an independent editable authority.

## Acceptance

- Human-only compose/save/restart works with no agent configured.
- Conflicting edits do not lose data or publish; existing old-schema drafts load intact.
- A changed label after preparation invalidates the exact affected action without bypassing recovery.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.
