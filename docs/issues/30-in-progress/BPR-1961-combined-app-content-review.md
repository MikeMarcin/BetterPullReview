---
schema: "implement-rfc.card.v1"
id: "BPR-1961"
title: "Review and bind latest accepted application composition content"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind": "test-correctness", "found_by": "frozen43Mac qualification / latest-main source audit", "reservation": "docs/issues/_reservations/BPR-1959--BPR-1961--64e658d8c3db.json", "milestone_plan": {"id": "bpr-1959-1961-mac-release-contracts", "path": "docs/rfcs/plans/bpr-1959-1961-mac-release-contracts/milestone-plan.json"}}
---

# BPR-1961: Review and bind latest accepted application composition content

## Acceptance

- AC1. Independently recompute unchanged142-source inventory at last matching9c and combined latestmain, review every changed source/effect boundary and commit range, reseal only production manifest pin; preserve full-app/mutation/inventory pins.
- AC2. Retain actual prior digest failure; maintained printer agrees with independent recomputation and existing adversarial composition contract passes, without full native acceptance claims.
- AC3. Format/docs/board and intentional latest-main integration pass; retain raw43failures, exact proof scope and pending final native qualification.

## Mandate

Coordinator approved this exact test-contract scope. Frozen producer source/builds remain untouched; all source fixes land authoritative main.
