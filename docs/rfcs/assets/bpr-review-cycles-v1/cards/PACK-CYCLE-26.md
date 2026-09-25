---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-26"
title: "Share cycle ownership across desktop and CLI"
status: "Backlog"
rfcs: ["RFC-BPR-0058"]
depends_on: ["PACK-CYCLE-03", "PACK-CYCLE-25"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-05", "RC58-06"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-26: Share cycle ownership across desktop and CLI

## Outcome and bounded work

Extend the tour owner service with finite cycle operations, exact profile/peer identity, operation deduplication, expected revisions and safe dirty-editor flush. Implement and qualify missing native transport rather than opening two writers or building an HTTP server.

## Acceptance and direct evidence

CLI-alone and desktop-open exchange, owner death/replacement, stale endpoint, wrong user, protocol mismatch, duplicate request, IME busy, out-of-order reply and cross-interface undo. Record platform-specific evidence.

Criteria: RC58-05, RC58-06.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
