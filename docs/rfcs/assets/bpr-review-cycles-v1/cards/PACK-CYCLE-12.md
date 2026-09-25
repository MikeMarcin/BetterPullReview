---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-12"
title: "Integrate labels and provenance without rewriting text"
status: "Backlog"
rfcs: ["RFC-BPR-0055"]
depends_on: ["PACK-CYCLE-11"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-03", "RC55-08", "RC55-10"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-12: Integrate labels and provenance without rewriting text

## Outcome and bounded work

Reuse the all-twelve-label/Plain parser and existing Markdown composer. Keep raw body authoritative; record human, observed adapter, self-reported and unknown provenance. Maintain deliberate public/private selection and exact suggestion compatibility.

## Acceptance and direct evidence

All labels plus malformed/plain bodies, model-identity claims, unselected hidden turns, undo/restart, prefix edit preserving suggestion bytes and renderer/source round trip.

Criteria: RC55-03, RC55-08, RC55-10.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
