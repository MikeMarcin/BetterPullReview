---
schema: "implement-rfc.card.v1"
id: "BPR-1959"
title: "Feedback settings contract scopes the feedback prompt page"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind": "test-correctness", "found_by": "frozen43Mac qualification / latest-main source audit", "reservation": "docs/issues/_reservations/BPR-1959--BPR-1961--64e658d8c3db.json", "milestone_plan": {"id": "bpr-1959-1961-mac-release-contracts", "path": "docs/rfcs/plans/bpr-1959-1961-mac-release-contracts/milestone-plan.json"}}
---

# BPR-1959: Feedback settings contract scopes the feedback prompt page

## Acceptance

- AC1. Assert no checkbox in the feedback hook page, while the separately adopted hidden Review skill page retains its own named copy control. Preserve Guidance/navigation/dispatch/published settings assertions; no product UI change.
- AC2. Old recursive descendant assertion fails against accepted BPR-1917; exact focused Qt course and a deliberate feedback-page checkbox negative control pass/fail as intended.
- AC3. Format/docs/board and intentional latest-main integration pass; retain raw43failures, exact proof scope and pending final native qualification.

## Mandate

Coordinator approved this exact test-contract scope. Frozen producer source/builds remain untouched; all source fixes land authoritative main.
