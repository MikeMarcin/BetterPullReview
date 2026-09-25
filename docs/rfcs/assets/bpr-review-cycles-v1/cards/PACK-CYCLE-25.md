---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-25"
title: "Add the agent-free headless cycle frontend"
status: "Backlog"
rfcs: ["RFC-BPR-0058"]
depends_on: ["PACK-CYCLE-04", "PACK-CYCLE-16", "PACK-CYCLE-17", "PACK-CYCLE-22"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-01", "RC58-02"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-25: Add the agent-free headless cycle frontend

## Outcome and bounded work

Extend the actual shared bpr runtime/command registry with local open/show/capture/note/review/freeze/export/compare/response operations. Preserve Qt Core if needed but no GUI/display. Implement useful human output, explicit versioned JSON and noninteractive behavior.

## Acceptance and direct evidence

Tests-disabled CLI install, empty environment with no desktop/provider, human versus JSON/stdout/stderr cases, stdin/editor handling, unsupported version and no credential or browser bootstrap for local operations.

Criteria: RC58-01, RC58-02.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
