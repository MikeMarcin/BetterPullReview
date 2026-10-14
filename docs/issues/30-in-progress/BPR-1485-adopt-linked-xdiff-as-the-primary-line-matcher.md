---
schema: "implement-rfc.card.v1"
id: "BPR-1485"
title: "Adopt linked xdiff as the primary line matcher and delete the first-party matcher and pause and resume"
status: "In Progress"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1465", "BPR-1480", "BPR-1482"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r3","source":"RFC-BPR-0082 primary matcher and hunk sliding: adopt l-myers-indent (owner answers 2026-10-13: Q1 dynamic shared library, L speed waiver, approximation clause, restart-only cancellation)"}
---

# BPR-1485: Adopt linked xdiff as the primary line matcher and delete the first-party matcher and pause and resume

The adoption follow-up of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md) for the primary line matcher
and hunk sliding. On macOS the evaluation candidate `l-myers-indent` (Git
2.56.0's xdiff, Myers with the indent heuristic, built as its own shared
library) passed every gate, agreed with GitHub on 78 of 78 recorded files
against the engine's 45, and left 0 comment-range flips after [[BPR-1482]]
([report](../../verification/RFC-BPR-0082.md),
[L tables](../../verification/RFC-BPR-0082/macOS-L-results.md)). The owner
answered on 2026-10-13: Q1, xdiff only as its own dynamically linked shared
library, as Qt is; its 1.60–2.03× time on L inputs is accepted (speed waiver)
and so are its 7 budget-caused approximations (approximation answer); and
restart-only cancellation replaces BPR-1366's pause and resume once xdiff is
adopted. The answer records are under
[`docs/rfcs/assets/diff-engine-evaluation-v1/`](../../rfcs/assets/diff-engine-evaluation-v1).

The evaluation's adapter shape is the starting point:
`tests/evaluation/diff_engine/candidates/xdiff_linked.cpp` (the whitespace
mapping that hands xdiff engine-normalized lines whenever the options are not
exact), the `xdiff_shim/` compatibility header, and the
`libbpr_eval_xdiff` shared-library target with the pinned Git 2.56.0 archive in
`tests/evaluation/diff_engine/CMakeLists.txt`.

## Known and out of scope

- Seven corpus comparisons stay approximate through the shared pairing and
  intraline budgets, not through the matcher: `bench-10000-300`,
  `bench-40000-300`, `bench-100000-30`, `bench-100000-300`,
  `patho-huge-replacement`, `patho-long-lines` and `patho-moved-blocks`. The
  owner accepted them; changing those budgets is not part of this card.
- Review Cycle suggestion hunks (the bounded Myers in
  `src/core/src/review_cycle_suggestion.cpp`) stay as they are; whether L
  replaces them is an open owner question.
- Linux and Windows measurements ([[BPR-1468]], [[BPR-1469]]) are not run here.

## Acceptance

- **AC1, library and linkage:** Git 2.56.0's `xdiff/` is acquired by the pinned
  verified-archive pattern (SHA-256, size, licence digest) and built as its own
  shared library, never compiled or statically linked into another target; one
  adapter links it dynamically and is the only code that calls it, and
  `architecture_boundaries` enforces both. Per-file licences (LGPL-2.1-or-later;
  EDL-1.0 for `xhistogram.c` if it ships), the LGPL notice and written source
  offer, corresponding source, `packaging/dependencies.json`, SPDX, release
  notices and the package audits are updated as for Qt's dynamic arrangement,
  and the macOS package carries the library where the application loads it.
- **AC2, primary matcher and hunk sliding:** every diff computation the
  application host runs gets its primary matches from that adapter through a
  host port and the [[BPR-1465]] seam: Myers with the indent heuristic and the
  evaluation's corrected whitespace mapping. On the evaluation corpus the
  product's alignments equal `l-myers-indent`'s (segments digests), and on
  `tests/fixtures/github/diff-oracle-v1` its hunks agree with GitHub on 78 of 78.
- **AC3, first-party matcher deleted:** the first-party primary matcher (the
  LCS table and its ordered-unique, anchor and patience fallbacks) and anything
  left dead are removed. The step meter and the budgets that pairing, intraline
  and the rapidfuzz-cpp scoring still use stay. Core-only builds and tests that
  need a matcher get a test double or the host port, not the old engine.
- **AC4, pause and resume deleted:** BPR-1366's pause-and-resume machinery is
  removed, per the owner's restart-only answer: a stopped diff is abandoned and
  recomputed from scratch the next time it is needed, and the 10 ms stop budget
  holds through abandonment (a test shows it). Every test that asserted
  resumption is changed to the restart behaviour and listed in the closeout,
  and `docs/architecture/diff-pipeline-and-rendering.md` and the "Pausing"
  section of `docs/architecture/system-architecture.md` describe the new
  behaviour.
- **AC5, verification:** the diff suites the RFC lists, `core-only`, both
  architecture-boundary checks and the documentation gate pass; the opt-in
  evaluation build either still configures and builds or the parts that
  measured deleted code are retired, with what was retired recorded here.

## Evidence (rfc-worker, 2026-10-13, branch `claude/bpr-1485-xdiff`)

Recorded for the integrator's closeout; the card stays in progress.

### AC1: library and linkage

- `cmake/BprXdiffProvider.cmake` acquires Git 2.56.0 by the verified-archive
  pattern. It checks the archive's SHA-256 `26c56c29…789d3` and size 8,489,656,
  then the extracted `LGPL-2.1` (`1922f45d…e76a`) and `COPYING` digests. It
  re-extracts only `xdiff/` and those notices, and builds
  `better_pull_review_xdiff` (`libbpr_xdiff.dylib`, `.so` or `bpr_xdiff.dll`)
  as a SHARED library only. The library is hardened and has no VERSION or
  SOVERSION. `third_party/git-xdiff/git-compat-util.h` and
  `bpr-xdiff-regex.h` replace Git's compatibility header for that build. Its
  allocators return null instead of aborting, and it links no regex library.
- `src/adapters/xdiff` (`BetterPullReview::AdapterXdiff`) is the only target
  that links it. It links it PRIVATE, so consumers load it dynamically.
  `xdiff_line_matcher.cpp` is the only caller.
- `architecture_boundaries` enforces both rules
  (`cmake/VerifyArchitectureBoundaries.cmake`):
  - an xdiff symbol or header outside the adapter source is refused;
  - a non-SHARED `better_pull_review_xdiff` is refused;
  - a second build of `xdiff/x*.c` in any product build file is refused;
  - any other `target_link_libraries` of the library is refused.

  `architecture_boundary_contract` has
  `test_linked_xdiff_has_one_adapter_owner` covering each refusal. Both
  checks pass in the dev tree.
- Packaging:
  - `packaging/Install.cmake` installs the library beside the application on
    Windows and Linux (`$ORIGIN` RPATH). The Windows import library goes to an
    unpackaged component.
  - `tools/release.sh` copies `libbpr_xdiff.dylib` into
    `Contents/Frameworks` (install name `@rpath/libbpr_xdiff.dylib`) and fails
    if the built library is absent. Its existing dependency policy then
    requires the `@rpath` reference to resolve there.
  - Windows tests find the DLL through a `PATH` entry in
    `cmake/BprTestIsolation.cmake`.
  - The notices, source offer, SPDX and `dependencies.json` work came from
    `claude/bpr-1485-notices` (merged at `d44d44fc6`). The provider's
    build-input digest was re-pinned after hardening (`22b2d559…1bb4`).

### AC2: primary matcher and hunk sliding

- The application's `diff_line_matcher_port`
  (`bpr/application/diff_line_matcher.hpp`) carries the BPR-1465 seam's
  primary matches. They are Myers with `XDF_INDENT_HEURISTIC`, with the
  evaluation's corrected whitespace mapping: engine-normalized lines whenever
  the options are not exact, and no xdiff whitespace flag. These computations
  get their matches through it:
  - `diff_application_host` (abandonable);
  - `composed_agent_realignment`;
  - the Review Cycle comparison host, runtime, workbench, desktop and CLI
    session.
- Corpus equivalence, using `tool_diff_xdiff_equivalence` and
  `tests/evaluation/xdiff_equivalence/compare_with_l_myers_indent.py`. The tool
  computes the product's diff (`tests/support/product_diff.hpp`, the same
  matcher and engine) for every pair of the RFC-BPR-0082 corpus
  (`diff-evaluation-corpus`, copied read-only from the evaluation worktree). It
  compares the harness's FNV digests with the `l-myers-indent|<pair>` entries
  of `docs/verification/RFC-BPR-0082/macOS-L-raw.json`:
  - pairs compared: 86 of 86;
  - segments, pairs and intraline digests, publication, reason and
    approximate flag equal: 86 of 86;
  - GitHub hunk agreement (the corpus's recorded `diff-oracle-v1` patches,
    joined as GitHub's file list joins them): 78 of 78.

  Command:
  `python3 tests/evaluation/xdiff_equivalence/compare_with_l_myers_indent.py build/dev/tests/evaluation/xdiff_equivalence/diff_xdiff_equivalence build/dev/diff-evaluation-corpus docs/verification/RFC-BPR-0082/macOS-L-raw.json`
  (exit 0, Debug dev build).

### AC3: first-party matcher deleted

- Removed from `src/core/src/diff_compute.cpp`:
  - the primary LCS region work list (`advance_region`, `region`,
    `region_stage`);
  - `ordered_unique_matches`, `patience_anchors` and `budgeted_line_less`;
  - the primary index budget (`MaximumPrimaryIndexWork`,
    `PrimaryIndexWorkConsumed`).
- `lcs_matches` stays for intraline. The step meter, the matrix budget, the
  pairing, intraline and rapidfuzz-cpp scoring, and the move budget stay.
- The one-argument `compute_file_diff` and `compute_material_diff` are gone.
  Every entry point takes a `diff_components&` with primary matches. A missing
  or declined primary refuses the comparison as `host_failure`, and invalid
  matches refuse it as `invalid_alignment`.
- Core-only and every other test computes through the host port. The
  `bpr_test_product_diff` target links the real xdiff adapter, which builds in
  every graph including `core-only`. No test keeps the old engine.

### AC4: pause and resume deleted

- Core: removed `paused_diff_compute`, `diff_compute_progress`, the slices,
  resume cursors, `held_bytes`, `progress_bytes`, `tick_kept`/`commit` and the
  staged validation cursor. A stopped run returns no outcome
  (`file_diff_compute_run`). `diff_component_meter::poll` was added for a
  component that waits.
- Host: removed `paused_preparation`, the paused deque, `PausedBytes`,
  `take`/`keep`/`discard_paused`, and the `paused_entries()`/`paused_bytes()`
  accessors.
  - A stopped job is abandoned.
  - A preparation the reviewer overtook is queued again at the front and
    computed from the start. A superseded one is not.
  - `abandonable_diff_line_matching` runs xdiff on its own detached thread
    with its own copies and polls the stop every millisecond.
- 10 ms through abandonment:
  `test_bpr1485_a_stop_inside_the_matcher_returns_within_10_ms` uses a matcher
  held 400 ms. The host returned after 1.10 ms. The matcher was still running
  and its result was discarded. The thread ended later and released the
  matcher. The large-pair prompt-stop test returned after 4.19 ms (Debug,
  shared station); its limit is now 10 ms.
- `docs/architecture/diff-pipeline-and-rendering.md` §3.2 and the "Pausing"
  item of `docs/architecture/system-architecture.md` describe the new
  behaviour.
- Tests changed to the restart behaviour:
  - `tests/core/diff_compute_tests.cpp`:
    - `test_bpr1366_an_uninterrupted_slice_is_the_one_call` became
      `…_run_is_the_one_call`;
    - `test_bpr1366_a_stop_is_honoured_within_the_bound` now says a stop
      abandons the run;
    - `test_bpr1366_interrupted_once_then_recomputed` lost its state-refusal
      cases and gained a restarted stoppable run equal to an uninterrupted one;
    - `test_bpr1366_resumed_results_are_identical` and
      `test_bpr1366_repeated_interruption_completes` were deleted, because
      they asserted resumption;
    - `test_ordered_unique_line_fallback` was replaced by
      `test_bpr1485_primary_matches_come_from_the_line_matcher`;
    - in `test_moves_identity_and_limits` the over-cells primary-budget
      expectation became "the primary matches charge no matrix cells";
    - `test_large_files_degrade_instead_of_refusing` no longer requires the
      matcher to approximate. Its one-cell case now degrades through pairing.
  - `tests/core/diff_compute_components_tests.cpp`:
    - `…_supply_nothing_change_nothing` became `…_supply_nothing_are_refused`;
    - components that supply another part now take the product's primary;
    - `test_bpr1465_a_metered_component_is_stopped` now restarts instead of
      resuming.
  - `tests/application/diff_application_host_tests.cpp`:
    - `test_bpr1366_a_preparation_stops_within_milliseconds_of_the_reviewer`
      expects the restart (blobs read again, 4 reads) and a 10 ms limit;
    - `test_bpr1366_reviewer_diffs_are_never_stopped_by_preparation` expects
      two restarts (6 reads);
    - `test_bpr1366_paused_states_stay_bounded_and_are_discarded` was replaced
      by `test_bpr1485_an_abandoned_preparation_restarts_only_while_wanted`;
    - `test_bpr1485_a_stop_inside_the_matcher_returns_within_10_ms` was added;
    - `test_preparation_yields_to_the_reviewer` now expects the overtaken
      preparation to be prepared again (2 prepared, not 1).
  - Approximate-path tests that forced the primary budget now force pairing:
    the host compute-limit case, `large_review_benchmark`'s approximate course
    (`MaximumMatrixCells = 1`) and `tests/fuzz/diff_fuzz.cpp` limits.

### AC5: verification

- core-only (`cmake --preset core-only`, Debug): builds; 267 of 275 pass.
  The failures:
  - the known ones: `documentation_structure`,
    `documentation_issue_graph_contract`, `issue_id_allocation_contract`,
    `contained_temporary_usage_contract`, `msvc_secure_crt_contract`;
  - `coding_format_check`, before the clang-format 22 commit;
  - `dependency_modernization_contract`, before the notices merge (it passes
    in dev after the merge);
  - `review_cycle_workbench`, fixed afterwards: the workbench now takes the
    matcher.
- dev (Debug):
  - 67 of 67 focused tests pass: `core_diff_*`, `core_review_comment_range`,
    `core_review_cycle_*`, `application_diff_host`,
    `application_anonymous_diff_authority`, `application_epoch_host`,
    `review_cycle_*`, `core_source_matcher`, `core_workbench_interaction`,
    `architecture_boundaries`, `architecture_boundary_contract`,
    `core_dependency_boundary`, `core_boundary_contract`.
  - Serial Qt and large-review run: 21 of 24 pass, 1 skipped
    (`application_large_review_cache_bytes`), 2 fail:
    - `application_large_review_launch` fails. It also failed for the
      baseline in the evaluation's suites (timing under load).
    - `qt_appearance_profile` fails "the reprojected position keeps its
      comparison and visible reading line within one row". Its fixture has
      only unique lines, so the alignment is the same under either matcher.
      Not adjudicated.
  - Package and dependency contracts: `package_runtime_contract`,
    `native_release_contract`, `release_package_audit_contract`,
    `release_security_audit_contract`, `dependency_modernization_contract` all
    pass.
- `core_diff_compute`'s "every phase of the large pair does work" passes in
  Debug. It was not run in a Release build.
- Format: `_coding-standard/scripts/format.py` (clang-format 22.1.8) on every
  changed C++ file. clang-tidy (`tidy.py`) on the changed sources exits 0. Its
  only warnings are `bugprone-std-exception-baseclass` on the engine's private
  control exceptions, the same pattern the deleted pause exception had.
- Documentation gate (main checkout's script against this root): one error,
  `docs/issues/NEXT_ID: next_id: BPR-1486 rewinds historical high water
  BPR-1492`. That comes from IDs allocated on main after this branch, not from
  this card. `verify_documentation_tests.py`: 67 tests OK.
- Opt-in evaluation build: retired. `BPR_DIFF_ENGINE_EVALUATION=ON` now stops
  configure with a message pointing at `cd4860e30`. The harness measured every
  candidate against the deleted first-party matcher, through a hook on the
  removed one-argument entry points. That covers the hook, the expectation
  inventories, `tool_diff_baseline_digest`, the fuzz replay driver, the
  intraline render-set tool and the harness. Their sources and scripts stay
  as the record of the measurement. The new
  `tests/evaluation/xdiff_equivalence` tool replaces the harness for AC2.

### Not done here

- `supported_diff_algorithm_revision` stays at 1. Alignments change with the
  matcher, and Review Cycle material keys write the revision into a digest.
  Whether to bump it is an integrator or owner decision.
- Linux and Windows were not built or run (BPR-1468, BPR-1469).

### Review fixes (after the independent review)

- B1, fixed: a final line without an ending that normalizes to nothing
  (spaces or tabs only, under `ignore_trailing` or `ignore_all`) produced no
  comparison bytes. xdiff then saw one record too few, and the comparison was
  refused as `host_failure`. The adapter now gives such a line the single byte
  0xFF, which valid UTF-8 never holds (the engine refuses invalid UTF-8). So it
  equals only another such line, and differs from an empty line that has an
  ending.

  `test_bpr1485_whitespace_only_final_lines` (core_diff_compute, through the
  product matcher) covers both policies and these cases:
  - the line on the left, on the right, against a non-empty line, and on both
    sides (all equal);
  - against an empty LF line (not equal);
  - an added and a deleted whitespace-only file.

  The test failed on the previous adapter and passes now.
- N1, fixed: a preparation is queued again only when the host itself stopped
  it (`PreparedJob->Stop.stop_requested()`), not on any `cancelled`. A blob
  port that declines on its own is not retried.
  `test_a_port_cancellation_is_not_prepared_again` covers it; it was not
  mutation-checked.

### Known follow-ups, not fixed here

- N2: nothing caps the number of detached matcher threads. At maximum size,
  the copy of both documents made before the thread starts counts against the
  10 ms budget.
- N3: xdiff's `BUG()` maps to `abort()`, which can run on a detached thread.
- N4: the linkage rules check build files and sources as text, not binaries.
  There is no scan for `xdl_` exports, and `tests/` is not scanned.
- N5: the matcher arguments of `review_cycle_runtime`,
  `review_cycle_comparison_host` and `review_cycle_workbench` default to
  nullptr. Without a matcher, a pair is refused as `host_failure` without any
  other sign.

### macOS package audits (final HEAD, before this evidence commit)

- `tools/release.sh --version 0.1.10 --prepare-only --skip-tests` passed,
  including its relocated self-test launch from the sealed bundle.
  `libbpr_xdiff.dylib` is universal (x86_64 and arm64) in
  `Contents/Frameworks`, with install name `@rpath/libbpr_xdiff.dylib`, and the
  application references it by that name.
- The first audit run failed `untrusted-library-search-path`. The release
  script gave a non-framework dylib in `Contents/Frameworks` the search path
  `@loader_path`, which the audits cannot resolve. It is now `@loader_path/.`
  (commit `52645b3ff`).
- `scripts/verify_release_package.py`: `RELEASE_PACKAGE_OK`.
- `scripts/verify_release_security.py`: `RELEASE_SECURITY_MECHANICS_OK`. The
  xdiff library is PIE with no writable executable segments, and the legal
  approval named as pending includes the LGPL xdiff library's.
- `packaging/verify_release_sources.py`: `RELEASE_SOURCE_ASSET_OK`, with 17
  source archives including `git-xdiff-2.56.0.tar.xz`.
- `packaging/verify_native_release.py`: not applicable. It verifies the
  Windows and Linux signed-production `*.release.json` manifests, which a
  macOS prepare-only run does not produce. `native_release_contract` passes.
- The release-source archives were seeded from local, digest-verified copies.
  Every archive is digest-checked again before use. The first attempt timed
  out downloading.
- After the B1 and N1 fixes, the corpus equivalence was re-run: 86 of 86
  digests equal and 78 of 78 GitHub agreement. Re-run in dev and all passing:
  `core_diff_*`, `application_diff_host`, `architecture_boundaries`,
  `core_review_comment_range`, `core_review_cycle_comparison`,
  `review_cycle_workbench`, `review_cycle_comparison`,
  `qt_workbench_line_draft` and `qt_diff_workbench_scale_100`.
