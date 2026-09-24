---
schema: "implement-rfc.card.v1"
id: "BPR-1104"
title: "Windows: the review-restoration course still stops at EPOCH_CACHE_FAILURE:7 (revision-prepare, spool_failure) after BPR-1040's retry"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1050's 32-row table","found_at":"bcd57840","related_cards":["BPR-1040","BPR-1050","BPR-0876"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
---

# BPR-1104: Windows: the review-restoration course still stops at EPOCH_CACHE_FAILURE:7 (revision-prepare, spool_failure) after BPR-1040's retry

## Defect

[[BPR-1040]] named three operations on a just-written pack spool that
Windows briefly refuses, and retried them, bounded. It left one
`spool_failure` in eight review-restoration runs unexplained. The failure is
still there. In [[BPR-1050]]'s 32-row table at `bcd57840`,
`application_profile_window_review_restoration_public_path` stopped at its
16th phase, the `unavailable-file` course's `auth-seed`:

```text
auth-seed failed (1)
EPOCH_CACHE_FAILURE:7:
EPOCH_EFFECT_FAILED:revision-prepare:0
FAILED: reading epoch ready
```

`7` is `git_cache_failure::spool_failure`. The 15 phases before it passed.
The course then passed on its own (365.48 s) and in the sweep (366.21 s), so
it is intermittent. The logs are station records under
`build/logs/bpr-1050-main-win/` and are not committed.

## Scope

- Measure a rate over N runs.
- Name the operation that fails and its Windows error, from a reproduced
  failure.
- Fix it in the layer that owns it, bounded and named, as BPR-1040 did.
- Show the rate drop over the same kind of runs.

## Acceptance

- AC1 The failing operation and its Windows error are named with evidence
  from a reproduced failure.
- AC2 The failure rate is measured before and after the fix over the same
  runs, and the fix brings it to zero in N runs large enough to have shown it
  before.
- AC3 Negative control: the same build and runs without the fix fail at the
  operation AC1 names.
- AC4 macOS and Linux keep their paths, or any shared change is run there.
