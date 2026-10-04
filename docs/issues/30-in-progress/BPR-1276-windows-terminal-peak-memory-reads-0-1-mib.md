---
schema: "implement-rfc.card.v1"
id: "BPR-1276"
title: "On Windows the large-project course reads every terminal command's peak memory as 0.1 MiB"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"origin":"BPR-1184 large-project course, Windows Release at 61c96b9a3 (bpr-main-win)","reservation":"docs/issues/_reservations/BPR-1275--BPR-1276--cc9fb8ce4d80.json","related_cards":["BPR-1184","BPR-1275"]}
---

# BPR-1276: On Windows the large-project course reads every terminal command's peak memory as 0.1 MiB

## Observation

[[BPR-1184]]'s course records each `bpr cycle` process's peak memory through
`child_peak_bytes` in `tests/end_to_end/review_cycle_large_project_contract.py`.
That is the resident set on POSIX and the peak commit on Windows. On Windows,
at `61c96b9a3`, every command (open, capture, compare, show) reads 0.1 MiB,
while macOS reads 16.7–92.5 MiB for the same commands. The Windows number is
not a measurement.

## Outcome

Measure each finished child's peak on Windows, for example its
`PeakPagefileUsage` or `PeakWorkingSetSize` read before the process handle is
closed. The report should carry real figures, with a check that a figure under
1 MiB for `bpr cycle open` fails the course.

## Cause and change (bpr-main-win, 2026-10-04)

`PROCESS_MEMORY_COUNTERS` begins with two `DWORD`s (`cb`, `PageFaultCount`);
the course declared every field `SIZE_T`. The structure grew to 80 bytes,
the size of `PROCESS_MEMORY_COUNTERS_EX`, so the field it read as
`PeakPagefileUsage` (offset 72) was the exited child's `PrivateUsage`, about
0.1 MiB. The two `DWORD`s are now `c_uint32`, and the course asserts that
`bpr cycle open`'s peak is over 1 MiB.

Evidence on Windows: a child that touches 200 MiB reads 206.1 MiB through
`child_peak_bytes`, and 0.1 MiB through the old structure, the negative
control. The Debug `application_review_cycle_large_project` row passes in
96.50 s. The POSIX branch is unchanged.
