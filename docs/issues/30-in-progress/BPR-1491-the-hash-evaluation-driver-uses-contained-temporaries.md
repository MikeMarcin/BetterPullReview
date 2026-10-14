---
schema: "implement-rfc.card.v1"
id: "BPR-1491"
title: "The hash evaluation driver uses contained temporaries"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_cards":["BPR-1446","BPR-1450","BPR-1451"]}
---

# The hash evaluation driver uses contained temporaries

`contained_temporary_usage_contract` fails on main:
`tests/hash_evaluation/run_hash_evaluation.py` calls `tempfile.mkdtemp` at
lines 277 and 290 instead of `scripts/contained_temporary_support`.

## Acceptance

- AC1: The driver makes its temporaries through
  `scripts/contained_temporary_support`, and
  `contained_temporary_usage_contract` passes.
- AC2: The hash evaluation driver still runs its documented course (or its
  own tests) with the same results.

## Boundaries

No change to what the evaluation measures.
