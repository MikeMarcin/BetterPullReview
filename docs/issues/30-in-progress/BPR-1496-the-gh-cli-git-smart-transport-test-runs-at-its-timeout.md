---
schema: "implement-rfc.card.v1"
id: "BPR-1496"
title: "The gh CLI Git smart transport test runs at its timeout"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# The gh CLI Git smart transport test runs at its timeout

`gh_cli_git_smart_transport` timed out (60 s) in the full gate on main at
cd4860e30. Alone on a quiet machine it passed in 55.4 s, against 36.9 s in
the 2026-10-12 gate. The row's `TIMEOUT 60` leaves no margin, so the test
fails under the gate's load.

## Acceptance

- AC1: What the test spends its time on is measured, and the reason it grew
  from about 37 s to about 55 s is named.
- AC2: The test finishes well inside its timeout on a loaded machine: by
  removing needless waiting, or, if the work is genuinely that long, by a
  justified timeout with the measurement beside it.

## Boundaries

No test course is removed.
