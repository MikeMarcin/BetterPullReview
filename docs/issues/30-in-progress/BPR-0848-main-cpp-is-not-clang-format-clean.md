---
schema: "implement-rfc.card.v1"
id: "BPR-0848"
title: "src/app/main.cpp is not clang-format clean"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-epoch-and-startup new defect 3; also seen by dsp-003 workers","found_at":"e3047a116ea68ad7d5591edb7a8f1b9dc713c8b5","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0848: src/app/main.cpp is not clang-format clean

## Defect

`clang-format` 22.1.8 reports 7 places in `src/app/main.cpp` at `e3047a11`,
introduced by `9de50d46`, `0ec1136a` and `284d1eac`. The dsp-003 BPR-0820
and BPR-0829 workers saw the same pre-existing violations. `coding-format-check`
should be red on this file.

## Scope

- Format only those places, with no other change.
- The file is pinned by `application_review_round_trip_composition_contract`:
  reseal with a recorded review of the formatting-only diff.

## Acceptance

- `clang-format --dry-run --Werror src/app/main.cpp` is clean, and the
  round-trip contract passes after the reseal.
