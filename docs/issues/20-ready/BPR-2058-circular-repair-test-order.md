---
schema: "implement-rfc.card.v1"
id: "BPR-2058"
title: "Rotate repair qualification at the failed test"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2057"]
priority: "P1"
---

# BPR-2058: Circular repair test order

## Scope

Owner clarification, 2026-11-11: rotate the existing test order at the failed
test, continue through the remaining suffix and wrap to the preceding prefix.
Do not regroup tests into failed, unrun and passing buckets. Apply only to future
main workflows, keep all selected coverage and fixture/dependency requirements,
and use focused correctness checks without a performance campaign.

## Acceptance criteria

- AC1: An explicit failure pivot produces a circular selection plan, with the
  pivot's required fixture/dependency group first, then suffix and wrapped prefix.
- AC2: Every selected row remains required on success. A failing segment prevents
  later segments, retains failures and exact unrun rows, and never claims a full
  pass or reuses previous-source pass credit. Native cleanup and locks survive.
- AC3: Focused real CTest controls, documentation and independent review pass;
  the active v0.4.2 release remains unchanged.
