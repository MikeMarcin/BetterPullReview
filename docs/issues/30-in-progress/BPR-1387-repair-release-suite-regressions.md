---
schema: "implement-rfc.card.v1"
id: "BPR-1387"
title: "Repair the failures exposed by the macOS release suite"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"corrective","origin":"Owner explicitly asks to fix the failures during the release-suite optimization, 2026-10-10","reservation":"docs/issues/_reservations/BPR-1386--BPR-1387--aad26e4d7bc0.json","milestone_plan":{"id":"bpr1386-release-test-suite","path":"docs/rfcs/plans/bpr1386-release-test-suite/milestone-plan.json"}}
---

# Repair the failures exposed by the macOS release suite

## Mandate and observed failures

The retained 0.1.7 release run failed these eight rows:

- `application_composition_contract` and `application_review_round_trip_composition_contract`: the production application manifest digest drifted.
- `application_large_review_launch` and `application_large_review_active_work`: responsiveness budgets failed.
- `application_large_review_history`: epoch refresh exceeded its measured platform budget.
- `application_home_open_feedback_public_path`: the slow read failed to open the pull request.
- `application_rate_limit_render_recovery`: the recovered comparison did not settle.
- `application_active_changed_scaling`: the exact fixture transport reported a mismatch.

These are historical observations, not evidence that every failure remains on
the current revision. Reproduce against the current built source and preserve
already-fixed dispositions as evidence. Work directly in this session.

## Acceptance

- AC1: Reproduce or give an evidence-backed current-source disposition for all eight historical failures, including the shared digest failure.
- AC2: Repair remaining causes with tests that exercise the failing contract. Preserve production authority, exact fixture checking, functional assertions and justified performance budgets; do not hide failures with skips or larger limits.
- AC3: All eight rows pass on the final applicable macOS graph, with focused Debug evidence where configuration affects the cause, and the integrated gate from BPR-1386 records any additional failures honestly.
- AC4: Relevant boundary, formatting and documentation checks pass; record commands, measured outcomes and the limits of this Mac's evidence, then commit the completed slice intentionally.

## Scope

No product redesign or weakening of release qualification. Fixture and test
procedure repairs are preferred only when direct evidence shows that the harness
is at fault. A product regression receives a narrow product fix and regression
check. Existing human and platform obligations remain unchanged.
