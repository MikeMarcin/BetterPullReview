---
schema: "implement-rfc.card.v1"
id: "BPR-1495"
title: "The large review launch course fails"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# The large review launch course fails

`application_large_review_launch` fails on main at cd4860e30 in the full gate
and again alone, after `LARGE_REVIEW_RESPONSIVENESS_MEASURED`, without a
FAILED line naming which check failed. It also failed in the 2026-10-12 gate
(2.9 s). `application_large_review_responsiveness` and
`application_large_review_history` pass alone.

## Acceptance

- AC1: The failing check is named, with its cause and the commit that
  introduced it. A budget miss is reported as a miss; no budget or assertion
  is loosened to pass.
- AC2: If the course does not name a failing check, it is changed to name it.
- AC3: `application_large_review_launch` passes on macOS, or the remaining
  miss is reported with its measurement for the owner.

## Boundaries

No change to the course's budgets.
