---
schema: "implement-rfc.card.v1"
id: "BPR-1253"
title: "GCC 15 Release reads the tour submission aggregate as uninitialized"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"reported by the Linux station (bpr-main-linux) on BPR-1228, 2026-09-30; filed by the live target qualification milestone","reservation":"docs/issues/_reservations/BPR-1253--BPR-1253--4f092f240678.json","related_cards":["BPR-1228"]}
acceptance: [{"criterion":"The designated aggregate is replaced by member-by-member assignment of the same values, and the tour plan tests pass","outcome":"pass","revision":"4169e0973","evidence":{"kind":"command","detail":"macOS arm64 dev preset: the nine application_review_tour_publication_* rows and application_review_tour_composition_public_path pass; coding-format-check (clang-format 23.1.1) and tidy.py on the file pass. Linux GCC 15 Release compile is owed on the station."}},{"criterion":"application_review_round_trip_composition_contract passes, with a reseal if the file is inside a pinned digest","outcome":"pass","revision":"4169e0973","evidence":{"kind":"command","detail":"Passes (13.8 s) with the pins unchanged: tour_publication_plan.cpp is only in the source-tree inventory, in no digest, so no reseal was needed."}},{"criterion":"python3 scripts/verify_documentation.py --root . passes","outcome":"pass","revision":"4169e0973","evidence":{"kind":"command","detail":"DOCUMENTATION_OK"}}]
---

# BPR-1253: GCC 15 Release reads the tour submission aggregate as uninitialized

## Observation

The Linux station's tests-off Release graph (GCC 15.2, `-O3 -D_FORTIFY_SOURCE=3`,
warnings as errors) stops at `src/app/src/tour_publication_plan.cpp:35` with
`-Wmaybe-uninitialized` on `submission.Proposal.Body`. It is the pattern
`4519a44f8` fixed in four other files: a nested designated aggregate whose
`std::string` member GCC reads as possibly uninitialized. It is the last
Release compile failure the station found (recorded on [[BPR-1228]]).

The station left the file alone because it believed the file was inside the
round-trip composition pin.

## Outcome

`review_submission_request` in `prepare_tour_plan` is value-initialized and
filled member by member with the same values. No behavior change.

## Direct acceptance

- The designated aggregate is replaced by member-by-member assignment of the
  same values, and the tour plan tests pass.
- `application_review_round_trip_composition_contract` passes, with a reseal
  if the file is inside a pinned digest.
- `python3 scripts/verify_documentation.py --root .` passes.

## Resolution (live target qualification milestone, macOS arm64, 2026-09-30)

`prepare_tour_plan` now value-initializes `submission` and assigns the draft,
attempt, target, acting account, body, disposition, idempotency key and live
binding one by one, with the same expressions as before. clang-format 23 moves
the following `value` declaration out of the old column alignment; nothing
else changes.

**No reseal was needed.** `tour_publication_plan.cpp` is not in any digest of
`application_review_round_trip_composition_contract.cmake`. It is listed only
in the application source-tree inventory, which checks file names, as the
BPR-1228 reseal comment already records ("join the source-tree inventory
only, like `tour_publication_plan`"). The contract passes with the pins
untouched, so the station's belief that the file was pinned was mistaken and
nothing here needs review beyond the change itself.

| Check (macOS arm64, Apple clang, dev preset) | Result |
|---|---|
| `ctest --test-dir build/dev -R "^application_review_tour_publication_\|^application_review_round_trip_composition_contract$\|^application_review_tour_composition_public_path$"` | 11 of 11 pass |
| `cmake --build --preset dev --target coding-format-check` (clang-format 23.1.1, 1159 files) | Pass |
| `python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths src/app/src/tour_publication_plan.cpp` | Pass |

## Owed platform run

Linux station (GCC 15.2, `-O3 -D_FORTIFY_SOURCE=3`): rebuild the tests-off
Release tree (`build/station-release`) with `-k 0` and confirm
`tour_publication_plan.cpp` compiles with no `-Wmaybe-uninitialized`, then
`ctest --test-dir build/dev -R "^application_composition_contract$|^application_review_round_trip_composition_contract$" --output-on-failure`.

## Linux — bpr-main-linux, 2026-09-30

Ubuntu 26.04 on WSL2, GCC 15.2, at `4b6592d83`, which contains `4169e0973`.

- **The tests-off Release graph compiles.** `build/station-release`
  (`-DBPR_BUILD_QT=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release`, as the
  contract configures it) rebuilt with `-k 0` has no failed step and no
  warning. It links `src/app/better_pull_review`. The last GCC 15 `-O3`
  failure is gone.
- `application_review_round_trip_composition_contract` passes, 2.2 s.
- `application_composition_contract` hits ctest's 600 s limit on this
  station: the row configures and builds a fresh Release graph in a temporary
  directory. The same registered command, run directly through the network
  sandbox, passes: `Ran 8 tests in 672.643s`, `OK`, including
  `test_tests_off_release_graph_omits_public_path_fixture`. The row's limit
  is too short for this machine's full Release build. Nothing else fails.

The logs are station records in `build/logs/bpr-station-1253/` and are not
committed.
