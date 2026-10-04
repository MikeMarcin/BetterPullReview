---
schema: "implement-rfc.card.v1"
id: "BPR-1274"
title: "A 514-item round costs about 3 s per note change, 4.3 s to freeze and 235 ms to open Share notes"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1184 large-project course, macOS arm64 Release at 397908dec","reservation":"docs/issues/_reservations/ BPR-1274--BPR-1274--d78638593452.json","related_cards":["BPR-1184","BPR-1161"]}
---

# BPR-1274: A 514-item round costs about 3 s per note change, 4.3 s to freeze and 235 ms to open Share notes

## Observation

[[BPR-1184]]'s `application_review_cycle_large_project` stress round (514 items)
measured, on macOS arm64 Release at `397908dec`
(`docs/verification/BPR-1184-large-project-2026-10-04.md`):

- each note change about 3 s;
- freezing the round 4.3 s;
- opening Share notes freezes the window for 234.8 ms, over the 100 ms input
  target.

BPR-1184 recorded these without changing them.

## Outcome

Find where each cost goes (profile first), make note changes and freezing scale
with the change rather than the round, and bring Share notes' open under the
100 ms target at 514 rows, or record a measured, named reason it cannot. Add the
case to the large-project course so it stays enforced in optimized builds.

## Result on macOS (2026-10-04)

Profiled with `sample(1)` on the Release stress round first; the findings, the
changes, before and after numbers, the checks and their negative controls are
in [the BPR-1274 record](../../verification/BPR-1274-large-project-2026-10-04.md),
with the report
[`mac-release-report.json`](../../verification/BPR-1274-large-project/mac-release-report.json)
at `80e72fd70`.

- **Where it went.** Every store call opened and schema-checked its own SQLite
  connection (68% of the effect lane); a note change re-read every note, two
  runtime round trips a note, and reconciled every note; a share assembled the
  round three times; Share notes built and laid out 514 rows in one turn and
  then relaid them as each note's code arrived.
- **macOS Release, 514 items, before → after:** a note change 3,026 → 67 ms
  (2.32 times the cycle's first, was about 66); Copy, which freezes,
  4,268 → 92 ms; Save 4,339 → 115 ms; resume 3,034 → 212 ms; Share notes
  holds the window 234.8 → 34.2 ms, listing its rows 64 a turn.
- **Freezing** now assembles, encodes and digests the round once, which is the
  least a freeze of the whole round can do; it no longer costs a store
  connection, or a round trip, a note.
- **Enforced:** the stress round's 100 ms window turns and 250 ms note change,
  Copy and Save in optimized macOS builds; a note change within 7.0 times the
  cycle's first in every optimized build. `review_cycle_large_project_targets`
  shows the BPR-1184 report failing them and this one meeting them.

## Checkpoint

- Candidate: `80e72fd70` (product, contracts, course; on `main`); the record,
  report, targets row and this note follow it.
- Verified on macOS arm64 Release: the large-project row 1/1 (172.65 s,
  targets enforced); 58 `cycle` rows with `review_cycle_large_project_targets`,
  `architecture_boundaries`, `architecture_boundary_contract`,
  `test_network_isolation_manifest`, `cycle_cli_public_path`;
  `coding-format-check`. Each new contract was shown to fail with its change
  removed.
- Pending: the Linux and Windows Release row and Debug rows, as the record's
  station requests give them. Off macOS the timed stress targets are reported
  only; the scaling check is enforced there too, and its first station
  measurements decide whether 7.0 holds.
- Next: commit the station reports with their figures here, then finish.

**Note: Linux station run at `91a9265b9` (station `bpr-main-linux`, WSL2, GCC
15.2.0; the station was idle, load 0.03). The Release tree does not build, so
there is no Linux report yet.** `cmake --build build/bpr1274-release` (the
`app-release` preset, `-O3 -Werror`) stops at:

```text
src/application/src/review_cycle_promotion.cpp:342:52: error: '*(bpr::application::review_cycle_tour_failure*)((char*)&<unnamed> + offsetof(std::review_cycle_tour_result, ...))' may be used uninitialized [-Werror=maybe-uninitialized]
  342 |                                 switch (aDone.error()) {
review_cycle_promotion.cpp:133:45: note: '<anonymous>' declared here
  133 |                                         task(*host, std::move(value));
```

The flagged object is the `review_cycle_tour_result` temporary that
`guarded` move-constructs for the by-value parameter of the `Tours.execute`
lambda ([[BPR-1273]] code). This is GCC's `-O3` `std::expected` false
positive; the Debug `dev` tree compiles it. Reading the error into a local
first does not silence it. Taking that lambda's parameter as
`const review_cycle_tour_result&` (line 332) does: the object then compiles
at `-O3 -Werror`. That change is not committed here, and the Release row was
not run.

The Debug tree (`build/dev-tour-cli`) passed 11/11 in 117.07 s, including
`application_review_cycle_large_project` (26.2 s, its 2,000-file correctness
pass) and `review_cycle_large_project_targets`.
