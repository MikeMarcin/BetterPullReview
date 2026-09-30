---
schema: "implement-rfc.card.v1"
id: "BPR-1253"
title: "GCC 15 Release reads the tour submission aggregate as uninitialized"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"origin":"reported by the Linux station (bpr-main-linux) on BPR-1228, 2026-09-30; filed by the live target qualification milestone","reservation":"docs/issues/_reservations/BPR-1253--BPR-1253--4f092f240678.json","related_cards":["BPR-1228"]}
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
- Owed on the Linux station: the tests-off Release graph compiles
  `tour_publication_plan.cpp` under GCC 15.
