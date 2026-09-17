---
schema: "implement-rfc.card.v1"
id: "BPR-0848"
title: "src/app/main.cpp is not clang-format clean"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-epoch-and-startup new defect 3; also seen by dsp-003 workers","found_at":"e3047a116ea68ad7d5591edb7a8f1b9dc713c8b5","run":"run-20260916-stations","dispatch":"dsp-003"}
acceptance: [{"criterion":"AC1 clang-format --dry-run --Werror src/app/main.cpp is clean","outcome":"pass","revision":"08c1aa30d348bf42e6448d1a71de811574d25055","evidence":{"kind":"command","detail":"clang-format 22.1.8 and 23.1.0 --dry-run --Werror pass; _coding-standard/scripts/format_check.py src/app/main.cpp passes. The diff is whitespace-only: removing all spaces, tabs and newlines leaves identical bytes."}},{"criterion":"AC2 the round-trip contract passes after the reseal","outcome":"pass","revision":"08c1aa30d348bf42e6448d1a71de811574d25055","evidence":{"kind":"command","detail":"application_review_round_trip_composition_contract 1/1 after rebuilding build/r2-dev (build/r2-logs/dsp4-0848-roundtrip.log). Only the production app manifest pin moved (827fe58a to d0d6c631); reviewed range 0c58092e..08c1aa30 contains only this formatting diff in the pinned scope."}}]
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

## Fix and review (dsp-004, Mac)

- **Change.** `clang-format` rewrote the 7 reported places in 4 hunks
  (lines 905, 3322, 5481 and 5509): two declaration alignments and four
  spaces after `(`. Homebrew `clang-format` 22.1.8 and 23.1.0 produce
  byte-identical output for the file, and both now pass
  `--dry-run --Werror`.
- **Review of the formatting-only diff.** Deleting every space, tab and
  newline from the file before and after leaves identical bytes, and no
  changed whitespace lies inside a string literal. The change cannot alter
  the program.
- **Reseal.** Since the last reseal (`0c58092e`), no other commit touches
  `src/app` or the contract. Only the production app manifest pin moves,
  from `827fe58a…` to `d0d6c631…`. The full-application and mutation-branch
  pins are unchanged.
- **Runs.** `application_review_round_trip_composition_contract` passes 1/1
  after rebuilding `build/r2-dev`
  (`build/r2-logs/dsp4-0848-roundtrip.log`). `format_check.py
  src/app/main.cpp` passes.
