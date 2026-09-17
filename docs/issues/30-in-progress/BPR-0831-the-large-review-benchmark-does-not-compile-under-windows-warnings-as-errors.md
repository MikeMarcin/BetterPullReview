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

## Windows station, 2026-09-17 (task-20260917-win-startup-roots)

The base is `559377700d52f37d6c094cd646200c67ab20eeb5`.

**Change.** `sample_resources` in `tests/end_to_end/large_review_benchmark.cpp`
used to throw on non-Apple platforms and then continue with the shared
sampling code, which MSVC reports as unreachable (C4702, an error under `/WX`).
The non-Apple branch now contains only the refusal:
`static_cast<void>(aApplication)` followed by the same `std::runtime_error`.
Everything the Mac course does, the memory sample and the cache and footprint
checks included, sits inside the Apple branch. Behaviour is unchanged on every
platform: macOS runs the same statements in the same order, and Linux and
Windows throw the same error at the same point. No warning is disabled or
suppressed.

**Verification.** `cmake --preset windows-dev` and a full `cmake --build
build\windows-dev --config Debug -- /m` succeed; this station had reported
`build-failed` on this target before. The build log contains no C4702.
Log: `build/logs/task-20260917-win-startup-roots/build-dev-base831.log`.

**Portable sources touched:** `tests/end_to_end/large_review_benchmark.cpp`.
The macOS and Linux builds belong to the integrator.
