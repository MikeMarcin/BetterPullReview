---
schema: "implement-rfc.card.v1"
id: "BPR-1494"
title: "A returning account does not resume its retained comparison"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# A returning account does not resume its retained comparison

`application_recent_account_isolation` fails on main at cd4860e30, in the
full gate and again alone (49 s), at its identity seed:

```text
pending=returning account resumes its exact retained comparison status=6 diagnostic= recording=15 operation=8
FAILED: returning account resumes its exact retained comparison
identity-seed failed (1)
```

It also failed in the 2026-10-12 gate. BPR-1403's ledger records an earlier,
different cause (a fresh legacy phase without a sign-in, fixed in ab32dd295).

## Acceptance

- AC1: The cause is named with the commit that introduced it, and it is
  classified as a product defect or a course defect, with the evidence.
- AC2: The fix makes `application_recent_account_isolation` pass on macOS
  without weakening account isolation or the exact-comparison check.

## Boundaries

Account isolation semantics do not change.
