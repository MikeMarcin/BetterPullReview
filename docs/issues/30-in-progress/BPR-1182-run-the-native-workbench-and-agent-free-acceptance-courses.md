---
schema: "implement-rfc.card.v1"
id: "BPR-1182"
title: "Run the native workbench and agent-free acceptance courses"
status: "In Progress"
rfcs: ["RFC-BPR-0055", "RFC-BPR-0060", "RFC-BPR-0061"]
depends_on: ["BPR-1166", "BPR-1171", "BPR-1177", "BPR-1181"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "qualification", "acceptance": ["RC55-07", "RC60-02", "RC60-10", "RC61-09"], "pack_alias": "PACK-CYCLE-40", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-40** (live `BPR-1182`, allocated at import).

# BPR-1182: Run the native workbench and agent-free acceptance courses

## Outcome and bounded work

Exercise actual Windows/macOS/Linux builds on the supported native environments; compare against 0052 chrome, keyboard/focus/IME/tab behavior and no-agent full loop. Obtain the required bounded owner look through existing attended-review, preserving unanswered gates.

## Acceptance and direct evidence

Native screenshots/measurements and gestures with build SHA, width floors and exact editor operations; no cross-compile-as-native claims. Record human decisions verbatim through the established ledger.

Criteria: RC55-07, RC60-02, RC60-10, RC61-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Input from BPR-1166 (2026-10-03): Share notes at the width floor

At the workbench window's 900-pixel minimum width, once a local cycle has a
second capture, Capture revision gains its menu arrow and the review bar folds
Share notes behind its » overflow. It is still reachable there, but the round's
primary action is no longer on the bar. [[BPR-1166]]'s course first met it on
the offscreen platform, whose 800-pixel screen holds the restored window at its
minimum; at the default 1280 pixels everything fits. Whether that is acceptable
at the floor is this card's width-floor judgment (RC61-09), not a change made
by BPR-1166.
