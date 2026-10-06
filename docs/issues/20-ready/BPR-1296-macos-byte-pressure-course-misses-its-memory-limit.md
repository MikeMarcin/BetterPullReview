---
schema: "implement-rfc.card.v1"
id: "BPR-1296"
title: "macOS: the large-review byte-pressure course intermittently misses its 184 MiB memory limit"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"BPR-1290 optimized large-review rows, 2026-10-05","reservation":"docs/issues/_reservations/BPR-1296--BPR-1296--f65dd49cf8eb.json","related_cards":["BPR-0967","BPR-0893","BPR-1032","BPR-1264","BPR-1265"]}
---

# BPR-1296: macOS byte-pressure course misses its 184 MiB memory limit

## Observation

On macOS (M4 Pro, Qt 6.11.2, optimized tests-on tree)
`application_large_review_cache_bytes` intermittently fails on plain
`origin/main`: 195.1 MB and 198.7 MB measured against 192.9 MB (184 MiB),
recorded on [[BPR-1290]]. Windows passes it in 7 of 7 optimized runs. The
Windows Debug byte-course failure is a different cause, owned by [[BPR-1265]].

The limit was set by [[BPR-1032]] (10% above a 165.5-166.7 MiB peak) and
revalidated on Release at `5a26d6b1b` by [[BPR-0967]]: sampled heap growth
164.0 MiB, lifetime footprint growth 158.2 MiB.

## Outcome

Measure the row's distribution at `origin/main`, find what grew since the
budget was set, and repair a real regression. Re-baseline only if the
footprint is legitimately higher and stable, with evidence and the BPR-0967
budget rule; never raise the limit to absorb variance.

## Direct acceptance

- AC1: 20 of 20 optimized macOS runs of the row pass at the candidate, with
  the distribution recorded.
- AC2: the growth is attributed to its commit with before/after measurement.
- AC3: a negative control shows the limit still fails when the growth returns.
