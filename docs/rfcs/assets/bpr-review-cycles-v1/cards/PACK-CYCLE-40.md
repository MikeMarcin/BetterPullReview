---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-40"
title: "Run the native workbench and agent-free acceptance courses"
status: "Backlog"
rfcs: ["RFC-BPR-0055", "RFC-BPR-0060", "RFC-BPR-0061"]
depends_on: ["PACK-CYCLE-24", "PACK-CYCLE-29", "PACK-CYCLE-35", "PACK-CYCLE-39"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "qualification", "acceptance": ["RC55-07", "RC60-02", "RC60-10", "RC61-09"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-40: Run the native workbench and agent-free acceptance courses

## Outcome and bounded work

Exercise actual Windows/macOS/Linux builds on the supported native environments; compare against 0052 chrome, keyboard/focus/IME/tab behavior and no-agent full loop. Obtain the required bounded owner look through existing attended-review, preserving unanswered gates.

## Acceptance and direct evidence

Native screenshots/measurements and gestures with build SHA, width floors and exact editor operations; no cross-compile-as-native claims. Record human decisions verbatim through the established ledger.

Criteria: RC55-07, RC60-02, RC60-10, RC61-09.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
