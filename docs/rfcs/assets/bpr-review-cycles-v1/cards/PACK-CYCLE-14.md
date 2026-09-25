---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-14"
title: "Deliver the native focused suggestion editor"
status: "Backlog"
rfcs: ["RFC-BPR-0055", "RFC-BPR-0061"]
depends_on: ["PACK-CYCLE-12", "PACK-CYCLE-13"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-07", "RC55-09", "RC61-04"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-14: Deliver the native focused suggestion editor

## Outcome and bounded work

Integrate prefilled Suggest change into the shared native composer with source/range identity, before/after preview, ordinary undo and IME. Inventory actual multi-caret/virtual-space component; implement only missing narrow editing behavior, not an IDE or renderer replacement.

## Acceptance and direct evidence

Attended/automated native tests for Alt+Shift+Down then Shift+End, virtual space, box/multi-caret copy/paste, tabs, CRLF, deletion, undo, IME and stale-source refresh while composing; no checkout writes or model call.

Criteria: RC55-07, RC55-09, RC61-04.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
