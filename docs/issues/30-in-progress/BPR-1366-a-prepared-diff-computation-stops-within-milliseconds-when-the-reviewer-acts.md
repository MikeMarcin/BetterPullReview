---
schema: "implement-rfc.card.v1"
id: "BPR-1366"
title: "A prepared diff computation stops within milliseconds when the reviewer acts"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"owner_request":{"date":"2026-10-09","channel":"chat","quotes":["it seems like diff loop as an async task could have interruption points, no?","And then it could be resumed later?"]},"reservation":"docs/issues/_reservations/BPR-1366--BPR-1366--799f7f909a93.json","related_cards":["BPR-1344","BPR-1359","BPR-1327","BPR-0719"],"sequence_after":"BPR-1344","filed_by":"rfc-implementer, at the coordinator's request"}
---

# BPR-1366: A prepared diff computation stops within milliseconds when the reviewer acts

## Observation

The owner asked on 2026-10-09: "it seems like diff loop as an async task could
have interruption points, no?" and then "And then it could be resumed later?"

Preparation already yields in two places:
- [[BPR-1344]] (being landed by the Windows station) makes preparation wait for
  an idle reviewer and puts the reviewer's work first on the actor.
- [[BPR-1359]] stops preparation's in-flight `gh` reads.

A diff computation that is already running cannot be interrupted. In
`perform` (`src/application/src/diff_application_host.cpp`), the job checks
its `std::stop_token` before and after `core::compute_file_diff(input)`, but
never during it. The call runs to completion, which takes hundreds of
milliseconds on a large file, and only then is the result thrown away as
`cancelled`. The reviewer waits for work nobody will use. A large file that
is interrupted on every click starts over each time and might never finish.

## Where the diff loop lives

- `src/core/src/diff_compute.cpp`: the engine. `compute_diff<TComparison>`
  (shared by `compute_file_diff` and `compute_material_diff`) runs these
  phases:
  1. validation and `prepare_document` for each side (UTF-8 decode, line
     split, `normalize_line`);
  2. `primary_segments` → `primary_matches`, a work list of `region`s
     (`pending`) over `patience_anchors` / `ordered_unique_matches`, with
     `lcs_matches` for small regions;
  3. `primary_segments`' refinement loop: `secondary_pairs` for each
     replacement segment (a similarity matrix and a DP table);
  4. `refine_moved_subblocks` (move detection, bounded by
     `MaximumMoveDetectionWork`);
  5. group assembly, then the pair loop that calls `intraline_ranges` →
     `lcs_matches` for each similar pair;
  6. `validate_alignment` and `validate_result`.
  The existing `matrix_budget` and `work_budget` structs are the step
  accounting the checkpoints build on. They already track `Consumed`, and
  [[BPR-0719]]'s degradation decisions depend on that count.
- `src/core/include/bpr/core/diff_compute.hpp`: `diff_compute_input`,
  `diff_compute_limits`, `diff_compute_outcome`, `compute_file_diff`,
  `compute_material_diff`.
- `src/application/src/diff_application_host.cpp`: `perform` (the call site,
  shared by reviewer and prepared jobs), `start_preparation`,
  `pump_preparation`, `prepare_now`, `discard_preparation`, the prepared
  share (`prepared_entry_capacity`, `prepared_byte_capacity`,
  `preparation_room`), and each job's `Stop` source.
- `src/core/include/bpr/core/diff_semantics.hpp`: `build_diff_semantic_index`,
  which `perform` runs after the compute. It is linear and restarts if
  interrupted.
- Tests: `tests/core/diff_compute_tests.cpp`, and
  `application_diff_host` / `application_epoch_host` under
  `tests/application/`.

## Outcome

A prepared diff computation gets cooperative stop checkpoints. When reviewer
work arrives, it stops within a bounded number of steps and **pauses**: its
progress is kept as resumable state, and the next preparation slice continues
from where it stopped instead of starting over.

### Core (no threads, no clocks)

- The core takes a caller-supplied stop predicate, or a step budget, through
  `diff_compute_input` or a resumable entry point. It checks that every N
  steps, counting steps with the existing budget accounting. The core never
  reads a clock or a thread primitive. The application owns the stop source.
- A stopped computation returns a paused outcome that carries an opaque,
  movable resumable state. Resuming it with the same bytes continues the
  computation. The state carries the `matrix_budget` / `work_budget`
  consumption and the `approximate` flag, so degradation decisions come out
  the same as in an uninterrupted run.
- The state is keyed by the comparison's blob identities (`LeftBlob`,
  `RightBlob`), together with the options digest, the algorithm revision and
  the compute limits, because the result depends on all of them. Resuming
  with any other key or bytes is refused, and the comparison restarts from
  scratch.
- The uninterrupted path, with no predicate or a predicate that never fires,
  stays one call with the same result as today.

### Resume points and restart units, by phase

| Phase | Stop checks | On resume |
|---|---|---|
| 1. `prepare_document` (each side) | every N lines | **resumes** at the line boundary (state: the lines done so far) |
| 2. `primary_matches` work list | every N steps, including inside `patience_anchors` and `lcs_matches` | **resumes** at the region boundary (state: `pending`, `matches`, budgets). One region's anchor index or LCS table interrupted midway is a **restart unit**: that region is redone |
| 3. `secondary_pairs` per replacement segment | every N rows of the similarity and DP tables | **resumes** at the segment boundary (state: `refined` so far, budgets). One segment's tables are a **restart unit** (keeping a partial table of up to `MaximumMatrixCells` is not worth its memory) |
| 4. `refine_moved_subblocks` | every N work units | **restarts** the phase (it is bounded by `MaximumMoveDetectionWork`, 1M units by default) unless measurement shows it needs checkpoints |
| 5. group assembly and intraline pairs | every N pairs, and inside `lcs_matches` | **resumes** at the pair boundary (state: `alignment` and `intraline` so far, budgets). One pair's intraline table is a **restart unit** |
| 6. validation, and `build_diff_semantic_index` in the host | every N records | **restarts** (linear over the result) |

The implementer may move a phase from "resumes" to "restarts" only with
measurements showing that its restart cost is within the stop bound. The
change and the numbers go on the card.

### Application

- A preparation job's `perform` passes a predicate over its `Stop` token.
  BPR-1344's reviewer-first path requests a stop on the running preparation
  job when reviewer work arrives.
- A reviewer diff is never stopped by preparation. Its job either passes no
  predicate or passes one that only its own cancel can fire. A reviewer
  request for a comparison that has a paused state may adopt that state,
  because the result is identical.
- Paused states stay bounded. Keep at most the number of comparisons in the
  window being prepared (`prepared_entry_capacity`), and count their bytes
  (the held blob bytes plus the state) inside the prepared byte share. Discard
  a paused state on supersession (`discard_preparation`: the authority,
  epoch or cancel paths, and a `prepare_now` that drops its key), on cache
  eviction or memory pressure, and when the comparison completes. A discarded
  state means a later restart, never a wrong result.
- When the slice resumes, the paused comparison is picked up again ahead of
  new preparation work, so a large file is not starved by later ones.

## Acceptance

- **Prompt stop.** A core test computes a large synthetic file pair that
  takes hundreds of milliseconds uninterrupted, requests a stop mid-diff
  (through the predicate, at seeded step counts that land in each phase), and
  shows the stop is honoured within a bounded number of steps. The bound is
  stated, and a wall-clock figure is recorded beside it. A host test shows
  the same through `perform`: a preparation job stopped mid-compute returns
  within the stated bound. Both tests fail before the change, when the stop
  is seen only after the whole compute, and pass after it.
- **Identical results, interrupted once.** A property test over seeded inputs
  compares an uninterrupted run with a run stopped at a seeded step and then
  recomputed from scratch. `computed_file_diff` (and `computed_material_diff`)
  compare equal, including `AlignmentApproximate` and the work counters.
- **Identical results, resumed.** A property test over seeded inputs and
  seeded interruption points (one or many per run, landing in every phase in
  the table, including inside restart units) resumes each pause from its
  state. The final result compares equal to the uninterrupted run, including
  the work counters.
- **Repeated interruption still completes.** A large diff interrupted
  repeatedly, every K steps with K well below the full run, completes. Its
  total work (steps consumed across all slices) is close to an uninterrupted
  run's: within a stated small overhead, plus at most one restart unit per
  interruption. The test records both totals. A resume that is given at least
  the largest restart unit of steps always passes at least one resume point,
  so progress is guaranteed.
- **No slowdown when nothing stops it.** Before and after timings of
  uninterrupted diffs over the same large synthetic inputs (several sizes,
  with medians and spread), on the same machine and preset, recorded on the
  card. The difference stays within measurement noise.
- **Reviewer diffs are never cancelled by preparation.** A host test runs a
  reviewer diff while preparation is paused, resumed and stopped around it.
  The reviewer diff completes with its full result.
- **Bounded paused state.** A host test fills the prepared window and shows
  that paused states never exceed the entry or byte bound. It also shows that
  supersession, eviction and completion each discard the state.
- **No regression from BPR-1344.** BPR-1344's history and active rows do not
  regress. `application_diff_host`, `application_epoch_host`,
  `diff_compute` and the BPR-1327 large-review selection pass.
- `coding-format-check` and `coding-tidy` pass on the touched files, and
  `core-only` builds: the core gains no thread or clock dependency.

## Notes

- **Sequencing.** Start after [[BPR-1344]] lands. Both cards touch
  `diff_application_host.cpp`'s preparation path, and this card's stop
  signal comes from BPR-1344's reviewer-first ordering. The dependency is
  recorded in `metadata.sequence_after` rather than `depends_on`, because
  BPR-1344's card is not yet on `main`.
- [[BPR-1359]] already stops preparation's reads. This card covers only the
  compute that follows them.
- Not in scope: making reviewer diffs themselves pausable, or changing the
  alignment the engine produces.
