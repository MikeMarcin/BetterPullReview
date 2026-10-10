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
acceptance: [{"criterion":"Prompt stop: core and host tests honour a stop within a stated step bound, with a wall-clock figure; both fail before the change","outcome":"pass","revision":"63816aedc","evidence":{"kind":"command","path":"tests/core/diff_compute_tests.cpp","detail":"core_diff_compute test_bpr1366_a_stop_is_honoured_within_the_bound: seeded stops in all six phases of a 40,000-line pair (optimized; 6,000 in Debug) pause at the first poll at or after the request, within CheckInterval 65,536 + LargestPollDelay (67,522 on this pair) steps; observed +2,609 to +104,857 steps. Optimized: 191 ms uninterrupted, 3,285 polls, longest 3.08 ms between polls. Host (application_diff_host test_bpr1366_a_preparation_stops_within_milliseconds_of_the_reviewer): a stop a third of the way into a 177 ms prepared computation returned in 1.16 ms (optimized; Debug 1.4-2.0 ms of 740 ms). Controls: polls disabled, the core test fails at 'a stop requested mid-computation pauses it'; the host test against the base host (cfd807f1b) fails, the stopped preparation returning after 155 ms."}},{"criterion":"Identical results, interrupted once and recomputed","outcome":"pass","revision":"63816aedc","evidence":{"kind":"command","path":"tests/core/diff_compute_tests.cpp","detail":"test_bpr1366_interrupted_once_then_recomputed: five seeded cases (moves, CRLF and Unicode, whitespace and line-ending normalization, pairing off, budgets tight enough to degrade) stopped at a seeded step in every phase; the recomputation equals the uninterrupted computed_file_diff, AlignmentApproximate and work counters included. A state is refused (Resumed false, result still equal) for another comparison, other bytes, file, limits, options, and a material comparison."}},{"criterion":"Identical results, resumed one or many times, in every phase and inside restart units","outcome":"pass","revision":"63816aedc","evidence":{"kind":"command","path":"tests/core/diff_compute_tests.cpp","detail":"test_bpr1366_resumed_results_are_identical: per case, one pause in each phase, many pauses at seeded intervals, and repeated 257-step pauses inside one restart unit, polling every step: 156 pauses (optimized; 161 Debug), every result equal including work counters, AlignmentApproximate, Steps, LargestRestartUnit and PhaseStarts. Pauses land in every phase and inside the restart units of primary, refine, moves, assemble and validate. A material comparison resumes equally."}},{"criterion":"Repeated interruption every K steps completes within a stated overhead","outcome":"pass","revision":"63816aedc","evidence":{"kind":"command","path":"tests/core/diff_compute_tests.cpp","detail":"test_bpr1366_repeated_interruption_completes, 20,000-line pair (optimized): K = largest restart unit + 1 = 18,348,721 (15.6% of the run), 7 pauses, uninterrupted 117,965,000 steps, interrupted 128,651,401 (+9.06%); K = 2 x unit: 3 pauses, +0.013%. Debug 5,000 lines: K = 10.0%, 10 pauses, +4.8%. Asserted: every slice passes a resume point, and the overhead is at most one restart unit per pause. The largest restart unit is a whole-file ordered index (the card's restart unit), 10-20% of a large run."}},{"criterion":"No slowdown when nothing stops it: before/after timings, several sizes, medians and spread","outcome":"pending","revision":"63816aedc","evidence":{"kind":"command","path":"tests/core/diff_compute_benchmark.cpp","detail":"Same benchmark source (one-call entry point), same flags, base core cfd807f1b vs candidate, three interleaved rounds of 9 repetitions on an idle station: median differences +0.2% to +2.9% (2K/30 +1.9%, 2K/300 +1.8%, 10K/30 +1.0%, 10K/300 +0.2%, 40K/30 +2.9%, 40K/300 +0.4%, 100K/30 +0.7%, 100K/300 +2.1%); the base's own round-to-round median spread is up to 2.2%. The candidate is never faster in median, so a residual of about 1-2% remains. The stoppable entry point with a predicate costs nothing over the one call. Table on the card."}},{"criterion":"Reviewer diffs are never cancelled by preparation","outcome":"pass","revision":"63816aedc","evidence":{"kind":"command","path":"tests/application/diff_application_host_tests.cpp","detail":"test_bpr1366_reviewer_diffs_are_never_stopped_by_preparation: preparation paused, resumed and paused again around two large reviewer diffs, with the window changed while each computes on a worker thread; both reviewer diffs publish whole results equal to a direct computation, no failure published; the twice-paused preparation then finishes without reading again and equals the uninterrupted result."}},{"criterion":"Bounded paused state: entry and byte bounds; supersession, eviction and completion discard it","outcome":"pass","revision":"63816aedc","evidence":{"kind":"command","path":"tests/application/diff_application_host_tests.cpp","detail":"test_bpr1366_paused_states_stay_bounded_and_are_discarded: a state over the prepared byte share is not kept and the next window starts again (reads repeated); one that fits is kept within entry and byte bounds and finishes first; an empty window, another window, the authority, the epoch and shutdown discard it; the reviewer's comparison needing the cache's room discards it; continued completion and reviewer adoption discard it (adoption reads nothing again)."}},{"criterion":"No regression from BPR-1344: history and active rows, application_diff_host, application_epoch_host, diff_compute and BPR-1327's selection pass","outcome":"pending","revision":"63816aedc","evidence":{"kind":"command","detail":"Optimized tests-on tree, -j1, load below 2: BPR-1327's selection plus core_diff_compute and its benchmark row, 27 of 28; application_large_review_active_work and _history pass. application_large_review_cache_bytes failed with 'stale comparison published while superseding file B remained selected', the message BPR-1344 records as intermittent on its controls and the selection race BPR-1376 has parked; rerun alone it failed 1 of 4, then passed 17 in a row (20 of 22 overall, both failures in the first minutes after the build). Not run against the base on this station. build/dev (Debug), -j1, before the rebase: all 25 dev rows pass (cache_bytes skips in Debug)."}},{"criterion":"coding-format-check and coding-tidy pass on the touched files; core-only builds with no thread or clock dependency","outcome":"pass","revision":"63816aedc","evidence":{"kind":"command","detail":"/usr/bin/clang-format 22.1.8: coding-format-check passes. tidy.py --warnings-as-errors on the seven touched translation units: clean. cmake --preset core-only, build and ctest --preset core-only: 261 of 261, including type_read_binding_contract, architecture_boundaries and architecture_boundary_contract (before the rebase). The core reads no clock or thread; stops come only from the caller's predicate."}}]
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

## Built and verified (bpr-main-linux, 2026-10-09)

Commit `63816aedc`, on `main` after BPR-1344. WSL2, GCC 15.2.0, i9-10900KF. No
live GitHub call and no model call; offscreen only.

### Design

- **Core API.** `compute_file_diff` and `compute_material_diff` gain an
  overload taking a `diff_compute_control` (a `diff_compute_stop*` predicate,
  polled with the step count, and a `CheckInterval`) and a
  `paused_diff_compute` to resume from. It returns a slice: the finished
  outcome, or a paused state, plus `diff_compute_statistics` (steps, work
  steps across slices, largest restart unit, largest poll delay, phase, phase
  start steps). `build_diff_semantic_index` gains a stoppable overload
  (`diff_semantic_index_build`). The one-call entry points and the result
  types are unchanged.
- **No threads or clocks in the core.** The private `diff_step_meter`
  (`src/core/src/diff_step_meter.hpp`) counts steps with the existing
  budget accounting: matrix cells, primary-index probe values, move-detection
  work, bytes scanned or normalized (16 steps a byte), validation records
  (256 steps). It polls every **N = 64 Ki steps**. A stop throws a private
  exception that only the slice driver and the index build catch, so it can
  never be mistaken for a work limit.
- **State and keying.** The paused state holds copies of both sides and the
  progress at the last resume point. It is refused unless the comparison key
  (blob identities, options digest, algorithm revision), the options, the
  limits and both sides' bytes match.
- **Stop bound.** A stop is honoured within N plus the largest single
  increment (the poll delay, reported). On the 40,000-line pair that is
  65,536 + 67,522 steps. The longest wall time between polls is up to
  3.5 ms for pairs up to 2 MiB a side (preparation's read limit) and 8.9 ms at
  5 MiB a side (one ordered-index teardown, which only a reviewer diff reaches).

### Phases

As the card's table. No phase moved from "resumes" to "restarts".
Validation resumes at its stage (alignment, each document, moves, intraline,
equal content by line, pairs), so only one alignment validation is a restart
unit. The host's semantic index build restarts; a prepared computation that
finished keeps its result while the index is rebuilt. Pairing's small tables
(at most 64 Ki cells) are counted in bulk by their caller, because the
pairing loop fills a very large number of them.

### Host

A preparation job polls its own stop token. A stop marks the job superseded
(new window, invalidation, cancellation, shutdown) or is a yield to the
reviewer. A yielded job pauses: it keeps its bytes, its engine state or its
finished result, and continues before new preparation. A reviewer request for
it adopts it. A reviewer job polls only its own cancellation. Paused states
are at most the prepared entry capacity, and their bytes count with
prepared comparisons inside the prepared byte share and the cache.
Supersession, the reviewer's need for room, and completion discard them;
one that does not fit is not kept.

Measured paused-state sizes are about 16 times one side: 4.7 MB for a
6,000-line pair (270 KB a side) and 22 MB for a 30,000-line pair. Under the
default 8 MiB share, pairs up to about 6,000 lines keep their pause; larger
ones start again when next prepared.

### Timing (before `cfd807f1b`, after this change)

Same benchmark source on the one-call entry point, built the same way, three
interleaved rounds of 9 repetitions on an idle station. Each figure is the
median of the three round medians, in ms.

| Pair | Before | After | Difference |
|---|---|---|---|
| 2,000 lines, 3% edited | 12.87 | 13.11 | +1.9% |
| 2,000 lines, 30% edited | 20.38 | 20.75 | +1.8% |
| 10,000 lines, 3% | 31.12 | 31.43 | +1.0% |
| 10,000 lines, 30% | 62.94 | 63.07 | +0.2% |
| 40,000 lines, 3% | 136.01 | 139.89 | +2.9% |
| 40,000 lines, 30% | 187.12 | 187.84 | +0.4% |
| 100,000 lines, 3% | 151.64 | 152.67 | +0.7% |
| 100,000 lines, 30% | 153.61 | 156.90 | +2.1% |

The base's own round-to-round medians vary by up to 2.2%, and the
interquartile spread within a run is 1-2%. The candidate is never faster in
median, so a residual of about 1-2% remains. Per-step timing puts it in the
large primary table. In isolation the new fill of that table is 2-3% faster
than the old one, so the residual looks like heap layout. In the
optimized tree, the stoppable entry point with a predicate matches the
one call (for example 187.94 against 189.71 ms at 40,000 lines and 30%).

### Rows

- Optimized tests-on tree: the results are in the acceptance records. The tree
  was deleted afterwards.
- On the merged tree (`build/dev`, Debug), after the rebase onto
  `9cab2ad75`:
  - pass: `core_diff_compute`, the benchmark row, `core_diff_model`,
    `core_diff_semantics`, `core_comparison_preparation`,
    `application_diff_host`, `application_epoch_host`,
    `application_workbench_host`, `application_first_tab_comparison`,
    `application_large_review_prepared_open`, `large_review_fixture_contract`
    and `test_network_isolation_manifest`;
  - fail on `origin/main` itself, so not this card's: `architecture_boundaries`
    (BPR-1222's `src/core/src/review_tour_markdown.cpp` matches `https?://`)
    and `application_review_round_trip_composition_contract` (the
    `github_application.cpp` digest is `f69fb520…`, pinned `0ca22569…`).
- This card changes no `src/app` source, so it reseals nothing.

## Coordinator decisions (2026-10-09)

The coordinator accepted both pending items:
- **Uninterrupted timing:** the 0.2–2.9% slowdown measured above is accepted.
  It is systematic, not noise, and attributed to heap layout.
- **`application_large_review_cache_bytes`:** its intermittent "stale
  comparison published while superseding file B remained selected" (2 of 22
  runs here) is the pre-existing failure recorded on [[BPR-1344]] and in
  [[BPR-1376]]'s parked race, not this card's.
- **Known limitation:** a paused state is about 16× one side's size, so under
  the default 8 MiB prepared share only pairs up to about 6K lines keep their
  pause. Larger ones start again when next prepared.
