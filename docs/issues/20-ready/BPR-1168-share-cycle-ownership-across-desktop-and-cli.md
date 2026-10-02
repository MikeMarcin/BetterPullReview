---
schema: "implement-rfc.card.v1"
id: "BPR-1168"
title: "Share cycle ownership across desktop and CLI"
status: "Ready"
rfcs: ["RFC-BPR-0058"]
depends_on: ["BPR-1145", "BPR-1167"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-05", "RC58-06"], "pack_alias": "PACK-CYCLE-26", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-26** (live `BPR-1168`, allocated at import).

# BPR-1168: Share cycle ownership across desktop and CLI

## Outcome and bounded work

Extend the tour owner service with finite cycle operations, exact profile/peer identity, operation deduplication, expected revisions and safe dirty-editor flush. Implement and qualify missing native transport rather than opening two writers or building an HTTP server.

## Acceptance and direct evidence

CLI-alone and desktop-open exchange, owner death/replacement, stale endpoint, wrong user, protocol mismatch, duplicate request, IME busy, out-of-order reply and cross-interface undo. Record platform-specific evidence.

Criteria: RC58-05, RC58-06.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
