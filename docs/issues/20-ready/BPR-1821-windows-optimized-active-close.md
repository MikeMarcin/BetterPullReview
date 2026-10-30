---
schema: "implement-rfc.card.v1"
id: "BPR-1821"
title: "Diagnose the optimized Windows active-close budget miss"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
---

# Diagnose the optimized Windows active-close budget miss

Frozen build-4 Windows Release `application_large_review_active_work` reports
active-close quiescence 241.9007 ms against the existing 150 ms platform limit.
The other eight completion budgets pass, the final file is exact and the close
fully drains. Closing starts with one diff and two epoch jobs. This is an
optimized Release result; Debug timing policy and the memory waiver do not
resolve it.

## Acceptance

- AC1: Preserve the exact original report and compare historical/source behavior.
  At the quiet PC profile boundary, run one authorized controlled follow-up on
  the unchanged actual image and maintained course before source changes. Bind
  the image, flags, source, host activity and exact predicate; do not infer a
  passing original result or blanket timing waiver.
- AC2: Diagnose a reproducible defect and correct it narrowly, preserving genuine
  cancellation, close/quiescence and ownership assertions, or document a grounded
  unchanged-source follow-up disposition if evidence supports it. Keep the 150 ms
  limit and every other gate unless the owner explicitly changes it. Run focused
  native proofs and meaningful controls; retain original and supplementary results
  separately and require a successor cohort for promoted changed inputs.
