---
id: "RFC-DRAFT-0018"
title: "Evaluate replacing the hand-written diff engine"
status: "Proposed"
---

# Evaluate replacing the hand-written diff engine

## Problem and grounded current behavior

On 2026-10-13 the owner asked for an RFC "to investigate if those replacements
are suitable through our tests and measurements" for the main diff engine. This
RFC proposes that evaluation: a measured comparison that ends in a recorded
adopt-or-reject decision for each component. It does not itself replace the
engine.

**Primary line diff.** In [the engine](../../../../src/core/src/diff_compute.cpp),
`advance_region` (about lines 1836–1915) trims the common prefix and suffix of
each region. It then tries `lcs_matches` (about 435–514), which fills an
(N+1)×(M+1) `uint32` LCS table: O(NM) time and memory for the changed region.
A region over `MaximumMatrixCells`, 16 Mi cells
([`diff_compute.hpp`](../../../../src/core/include/bpr/core/diff_compute.hpp)
line 76, so roughly 4,096 × 4,096 changed lines), falls back in order. First
comes `ordered_unique_matches` (about 701–754), which is exact only when every
common line is unique and ordered. Then come `patience_anchors` (about
760–852), recursing into the gaps between anchors. A gap with no anchor stays
one plain replacement. Each fallback step sets `AlignmentApproximate`. There is
no Myers, histogram, hunk sliding or indent heuristic.

**Pairing and intraline.** `secondary_pairs` (about 549–648) scores every
left/right line pair of a replacement. Each score is a character LCS (`similarity`,
about 519–547) giving 2·LCS/(L+R) per mille, which is exactly normalized Indel
similarity. `intraline_ranges` (about 1226–1268) is a further character LCS over
Unicode scalars, O(L·R) per pair, with no semantic cleanup. Primary, pairing
and intraline draw on one per-file matrix budget (set at about line 2038 and
consumed at about 2076 and 2251). Once it is spent, later replacements stay
unpaired and later similar pairs lose intraline emphasis. The file then reads
"(approximate alignment)" ([`diff_workbench.cpp`](../../../../src/adapters/qt/src/diff_workbench.cpp)),
but nothing says which changes degraded.

**Interruption.** [BPR-1366](../../../issues/60-done/BPR-1366-a-prepared-diff-computation-stops-within-milliseconds-when-the-reviewer-acts.md)
rewrote the engine as a resumable state machine (about 1521–2397, plus
[`diff_step_meter.hpp`](../../../../src/core/src/diff_step_meter.hpp)). The
reason was that one large file takes hundreds of milliseconds: an optimized
40,000-line pair took 191 ms uninterrupted. After the change, a host stop
returned in 1.16 ms.

**Duplicates and emulation.** Review Cycle suggestions carry a second, bounded
Myers line diff ([`review_cycle_suggestion.cpp`](../../../../src/core/src/review_cycle_suggestion.cpp),
`matched_lines`, about 497–547, D ≤ 512). Past that bound the whole middle
becomes one hunk. Comment-range eligibility emulates GitHub's hunks from BPR's
own change groups with ±3 lines
([`review_comment_range.cpp`](../../../../src/core/src/review_comment_range.cpp),
about 29–57). [Its header](../../../../src/core/include/bpr/core/review_comment_range.hpp)
concedes that where the diffs differ, GitHub's refusal at Publish stands.
Recorded GitHub file listings keep the provider's `patch`
([`github_read_contract.cpp`](../../../../src/adapters/github/src/github_read_contract.cpp),
about 1233), which gives a real hunk oracle.

**Production history.** In [BPR-0625](../../../issues/60-done/BPR-0625-bounded-ordered-line-matching.md),
a 20,000-line file with 20 changes exceeded the matrix budget. The card added the
ordered-unique path and called it "not a general replacement diff algorithm."
In [BPR-0719](../../../issues/60-done/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md),
the owner could not open `ScreenBufferTests.cpp` (about 8,000 lines, +291) in
microsoft/terminal#20607. That card's scope asked for "patience or Myers", but
only trimming and patience landed. Its synthetic replica is
`test_large_files_degrade_instead_of_refusing` in
[`diff_compute_tests.cpp`](../../../../tests/core/diff_compute_tests.cpp).
BPR-1366 is the third case, described above.

**Contract room.** [RFC-BPR-0004 §9](../../RFC-BPR-0004-diff-computation-line-mapping-grouping.md#9-diff-and-refinement-pipeline)
says "The specific line-diff algorithm is not a public product contract". It
requires validation of coverage, monotonicity, identity and bounds before
publication, which `validate_computed_file_diff` performs, including
equal-content checks. The comparison key carries an algorithm revision
(`supported_diff_algorithm_revision`, currently 1, `diff_compute.hpp` line 17).

**Boundaries.** The core is standard/domain C++ only
([dependency direction](../../../architecture/system-architecture.md#build-target-and-dependency-direction),
[`VerifyArchitectureBoundaries.cmake`](../../../../cmake/VerifyArchitectureBoundaries.cmake)),
and [its target](../../../../src/core/CMakeLists.txt) links no library.
Third-party code lives under [`third_party/`](../../../../third_party/) with a
`PROVENANCE.md`, and is inventoried in
[`packaging/dependencies.json`](../../../../packaging/dependencies.json). The
core so far contains generated Unicode data but no third-party code.
The [installed-Git adapter](../../../../src/adapters/git/src/installed_git_process.cpp)
already runs Git as an isolated child: `GIT_CONFIG_NOSYSTEM`, a controlled
global config, and a kill on stop. It already runs `diff-tree -p` for
publication. Git is admitted from 2.39 onward
([build and test](../../../development/build-and-test.md#retained-git-history-and-epoch-storage)).
Review Cycle material comparisons (`review_cycle_comparison.cpp`) and agent
realignment (`agent_realignment.cpp`) call the core engine directly and use no
Git.

**Line identity.** The split step (about lines 1976–1983) ends a line at a lone
CR; Git and GitHub do not. That divergence is being fixed separately. This
evaluation uses LF-only line identity throughout: a line boundary comes only
from LF, and CRLF otherwise follows the existing line-ending policy.

## Proposed behavior and boundaries

### Outcome and scope

The evaluation builds an opt-in harness that runs each candidate component
against the existing engine on one corpus. Every result goes through the same
first-party pipeline and validation. The harness lives in a non-shipping target,
off by default. No shipped target, package or release script may link or
bundle a candidate during evaluation.

Pinned, hash-verified sources may be acquired the way the build acquires CPM
sources. Nothing enters `third_party/` or `packaging/dependencies.json` until
an adoption follow-up does so. The evaluation may add a core entry point that
takes a supplied primary match list as a bounded observation and runs the
remaining first-party pipeline. Existing entry points must keep producing
identical results. Whether that seam ships is part of adoption.

The deliverable is a committed report with raw results. For each component it
records adopt, reject, or eligible pending an owner answer. Production
replacement, including any change to the algorithm revision or the cache
identity, is a follow-up card or RFC that cites this report.

### Components and what stays first-party

| Component | Current | Evaluated against |
|---|---|---|
| Primary line matcher (monotonic matches over normalized lines) | LCS table, ordered-unique and patience fallbacks | Git histogram/patience/minimal; xdiff linked; a core histogram or O(NP) port |
| Hunk sliding / indent heuristic (post-pass on the alignment) | none | Git's `--indent-heuristic`; a core post-pass |
| Line-pair similarity | per-pair character LCS | rapidfuzz-cpp Indel |
| Intraline ranges | minimal character LCS | diff-match-patch with semantic cleanup; Myers or O(NP) over scalars |
| Review Cycle suggestion hunks | bounded Myers, D ≤ 512 | whichever primary matcher is adopted |

A number of pieces stay first-party whatever the results:

- decoding, binary and UTF-8 checks, and LF-only line splitting
- whitespace and line-ending normalization (`diff_options`)
- segment canonicalization and the pairing policy: threshold, monotonicity and
  tie-breaking
- move detection
- stable segment, pair, group and move identities
- `validate_computed_file_diff`
- the comparison key, the host cache and its budgets
- explicit degradation reporting
- the link map and alignment-space geometry
- comment-range decisions

A candidate supplies matches, scores or spans; it never supplies identities or
geometry.

### Candidates and integration shapes

- **Process (P).** Installed Git through the existing Git adapter. A PR
  comparison runs blob-to-blob `git diff` inside the retained repository, so no
  temporary files are written. Review Cycle material needs `--no-index` on
  host-written temporary files, or an object import. Each run pins its flags
  explicitly: the algorithm, `--indent-heuristic`, `-U0`, `--no-ext-diff`,
  `--no-textconv`, `--no-color`, and whitespace flags mapped from
  `diff_options`. The host parses the hunks into matches. The core assembles
  and validates them, and its equal-content check catches any normalization
  mismatch. Stopping means killing the process. Running the user's installed
  Git ships no Git code and is not linking. The output can, however, vary with
  the installed Git version.
- **Linked (L).** git's or libgit2's copy of xdiff, compiled into an adapter
  behind a host port. git's xdiff is LGPL-2.1-or-later, apart from per-file
  notices to be confirmed (its histogram derives from JGit). Static linking
  brings relinking and source-offer obligations. xdiff has no stop hook, so a
  stop needs an abandonable worker or a modified copy.
- **Core (C).** Standard-only source wrapped by the existing step meter:
  - dtl (BSD-3-Clause, O(NP))
  - a port of JGit's HistogramDiff (EDL-1.0)
  - rapidfuzz-cpp (MIT), whose normalized Indel similarity has the same formula
    as `similarity`
  - a standard-only diff-match-patch port (Apache-2.0). Google's C++ port uses
    Qt and a `clock()` deadline, both forbidden in the core.
- **Baseline.** The current engine runs in every measurement.

Considered and not proposed: imara-diff, which adds a Rust toolchain, and GNU
diff, which is GPL-3.0 and offers nothing over Git as a process.

### Corpus and measurement

**Suites.** Run `core_diff_compute`, `core_diff_semantics`, `core_diff_transition`,
`core_review_comment_range`, `core_review_cycle_comparison`,
`core_review_cycle_suggestion`, the `diff` fuzz target,
`application_diff_host`, the `application_large_review_*` courses
(including `pathological`, `active_work` and `cache_bytes`), and
`core_diff_compute_benchmark`. The benchmark covers the BPR-1366 sizes, 2K to
100K lines with 30 or 300 changes.

**Fixtures.** [large-review-v1](../../../../tests/fixtures/github/pull-requests/large-review-v1/README.md)
supplies BPR-0625's actual 20,000-line comparison and an 8,192-line generated
table. Use the BPR-0719 replica. The real `ScreenBufferTests.cpp` pair may
replace it only if it can be committed under the fixture policy; otherwise the
report says the replica stood in. Add generated pathological classes:

- all-identical and highly repetitive lines
- phase-shifted repeats
- one huge replacement
- lines of 10,000 or more scalars
- moved blocks
- whitespace-only edits
- Unicode, CRLF and lone CR (to show LF-only identity)

**GitHub oracle.** Use the recorded provider `patch` fields: uiforetw-74,
directxmath-336, and `contracts/v1/*/responses/files.json`. More oracle pairs
may be recorded on `MikeMarcin/BPR-Test` under the repository's live-GitHub
and rate-limit rules.

**Measurement.** Use the [RFC-BPR-0013 convention](../../RFC-BPR-0013-performance-resilience-resource-limits.md#performance-budgets)
and BPR-1366's benchmark practice: an optimized build, interleaved rounds of
repetitions on an idle station, medians and spread, and recorded hardware. For
each size class (S < 2K lines, M 2K–10K, L > 10K, pathological, real) record:

- time to final result
- peak resident memory
- stop-to-return latency at the host, including process kill and reap
- per-file process start overhead
- approximate results and pairs that lost intraline emphasis
- line edit size relative to a minimal edit, where one is computable
- exact agreement with GitHub's hunk ranges
- comment-range decisions that change against the oracle

Run on macOS and on the Linux and Windows stations. Run P against Git 2.39 and
the current release.

### Decision rules

The thresholds below are fixed before measurement. Changing one afterwards is an
owner decision, and the report records it.

A candidate is rejected for a component if any gate fails:

| Gate | Requirement |
|---|---|
| Correctness | All suites pass. Every changed expectation is listed as an equally valid alignment. The whole corpus has zero validation failures and no new refusals. |
| Determinism | Output is identical across runs and platforms. For P, it is identical across admitted Git versions, or the report costs adding the version to comparison identity. |
| Interruption | Stop-to-return p95 is at most 10 ms on L and pathological inputs, on every platform. |
| Placement | The architecture-boundary gates pass in the proposed placement. P keeps a first-party fallback wherever Git is absent or not admitted, including Review Cycle material. |
| Licence | Each shipped file's licence, linkage and notice or source obligations are recorded. Where an owner answer is required (below), the result can only be eligible pending that answer. |

A candidate that passes the gates is then adopted only under its component's
rule:

| Component | Adopt only if |
|---|---|
| Primary matcher | On L and pathological inputs it produces no approximate result where the baseline does, and is at least 2× faster or 2× smaller in median. No class regresses its median by more than 10%. GitHub hunk agreement is at least the baseline's, and comment-range flips do not increase. |
| Hunk sliding | GitHub hunk agreement rises, and it costs at most 5% of compute. |
| Pair similarity | Pairing decisions are bit-identical to the baseline's over the corpus, and the pairing phase is at least 2× faster on M and L. |
| Intraline | Every span is valid, p95 time per pair is no worse, and the owner accepts a fixed render set in an attended review. Semantic cleanup changes what is emphasized, so this is a look-and-judge criterion. |
| Suggestion hunks | The adopted primary matcher passes the Review Cycle suggestion tests, and its behavior past the current bound is no worse. |

A rejected component keeps the current engine, and a result of no change is
valid. The report also states whether the best candidate's worst observed time
would make restart-only cancellation enough. It does not remove BPR-1366's
resumable machinery.

### Open questions

- **Q1. Is linking LGPL xdiff acceptable?** Statically linking LGPL-2.1
  code into a distributed desktop app brings relinking and source-offer duties
  beyond the current Qt (dynamic) and KSyntaxHighlighting arrangements. Shape L
  is measured but cannot be adopted until the owner answers.
- **Q2. May vendored pure-computation third-party code enter the core?**
  This affects dtl, rapidfuzz-cpp, a diff-match-patch port and a JGit-derived
  histogram port. The owner question is pending in the parent session. Until
  it is answered, these are evaluated only in the opt-in target, and core
  placement cannot be adopted.
- **Q3. May comparison results depend on the user's installed Git version?**
  If Git is upgraded, the same comparison key could yield a different valid
  alignment, unless the version enters comparison identity. This affects P
  adoption only. The evaluation reports cross-version agreement so the
  question can be answered on evidence.

## Acceptance

- **AC01:** The opt-in harness runs every candidate–component pairing and the
  baseline over the full suite list and corpus through first-party validation.
  The committed report shows each gate's pass or fail with its commands and
  counts.
- **AC02:** Measurements on macOS, Linux and Windows cover every size class
  and the measurement list under "Corpus and measurement", with the baseline
  measured in the same runs and conditions.
- **AC03:** GitHub agreement and comment-range flips are reported per candidate
  against recorded provider patches. This includes the BPR-0625 and BPR-0719
  inputs and a lone-CR case showing LF-only line identity.
- **AC04:** For each candidate the report records licence per shipped file,
  linkage, obligations, platform and Git-version dependence, and the owner
  answer it needs. A package and boundary check shows that no shipped target
  links an evaluation candidate.
- **AC05:** Each component's recorded decision follows mechanically from
  "Decision rules", and any deviation is recorded as an owner decision.
- **AC06:** Each intraline candidate that passes the mechanical gates receives
  an owner Accept or Reject on its render set, in its own attended-review
  session.
- **AC07:** Existing engine entry points produce identical results before and
  after the evaluation work. `core-only` and both architecture-boundary checks
  pass.
