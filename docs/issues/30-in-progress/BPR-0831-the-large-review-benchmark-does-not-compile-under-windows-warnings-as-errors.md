---
schema: "implement-rfc.card.v1"
id: "BPR-0831"
title: "The large review benchmark does not compile under Windows warnings-as-errors"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0831: The large review benchmark does not compile under Windows warnings-as-errors

## Defect

A full `windows-dev` build fails on one target,
`bench_better_pull_review_large_review`. MSVC reports C4702 (unreachable
code) under `/WX` in `tests/end_to_end/large_review_benchmark.cpp` (~180):
on non-Apple hosts the resource sample throws
`"the native memory resource course is currently Mac-only"`, and the lines
after the `#endif` are unreachable there. GCC and AppleClang do not warn.

Reported by `bpr-win` in `task-20260917-win-hardening` (windows-lane
`711d3ddc`), not carded there.

## Scope

- Restructure the non-Apple branch so no statement follows the throw on that
  path. Keep the course Mac-only in behaviour; do not add a Windows memory
  course here.
- Do not silence C4702 globally or for the target.

## Acceptance

- A full `windows-dev` build succeeds on the Windows station.
- The benchmark target still builds on macOS and Linux.
