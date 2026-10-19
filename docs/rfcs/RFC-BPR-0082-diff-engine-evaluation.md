---
id: "RFC-BPR-0082"
title: "Evaluate replacing the hand-written diff engine"
status: "Proposed"
depends_on: ["RFC-BPR-0004"]
metadata: {"rfc_base_sha":"cc9e9376e1fc02e2cdf3154fa56dfcc14991a8d5","mandate_revision":"diff-engine-evaluation-r5"}
---

# Evaluate replacing the hand-written diff engine

## Current core policy — 2026-10-18

The [adopted external-determinism amendment](amendments/RFC-BPR-0001-external-determinism-r1.md)
supersedes library-specific purity-exception rationale. Dependency approval,
implementation constraints and original acceptance obligations remain.

## Owner placement amendment — 2026-10-18

Mandate `core-boundaries-r1`, [[BPR-1619]]: the owner authorizes moving pure
xdiff matching from its adapter into core. This supersedes only the original
adapter-only placement in Q1/shape L. xdiff remains the pinned separately
replaceable dynamic shared library; licensing, corresponding source, relinking,
whitespace mapping, line identities and memory contracts are unchanged.
Application retains copied-input scheduling and abandonable cancellation. The
[additive plan](plans/core-boundaries-v1/milestone-plan.json) does not replace
historical evaluation evidence or the open Linux/Windows qualification and
closeout obligations on BPR-1468, BPR-1469 and BPR-1470.

## Execution status

Mandate `diff-engine-evaluation-r1` adopts RFC-DRAFT-0018 unchanged in scope.
The [live import](assets/diff-engine-evaluation-v1.LIVE_IMPORT.md) retains the
original draft byte-for-byte. Cards: [[BPR-1465]] (core seam), [[BPR-1466]]
(opt-in harness and corpus), [[BPR-1467]] (macOS measurements and decisions),
[[BPR-1468]] (Linux), [[BPR-1469]] (Windows) and [[BPR-1470]] (closeout).
Since mandate `diff-engine-evaluation-r4`, [[BPR-1468]] and [[BPR-1469]]
qualify the adopted stack rather than measure candidates.

Owner decisions supplied with the mandate, 2026-10-13: the core may link only
the owner-approved, pinned, pure libraries glaze, utf8proc, zlib and fmt. Any
other library in the core still needs the owner's explicit approval for that
library. Q2 below is therefore answered for those four only; dtl,
rapidfuzz-cpp, a diff-match-patch port and a JGit-derived matcher remain
"eligible pending owner approval (Q2)" when they pass. Q1 and Q3 are open. A
shape that passes but needs an open answer is recorded as "eligible pending
Qn" and is not adopted. The CR line-identity fix the draft mentions has since
landed ([[BPR-1432]]), so the engine already splits lines at LF only.

## Owner answer — 2026-10-13

Mandate `diff-engine-evaluation-r2`. The owner answered, as relayed by the
coordinating session on 2026-10-13:

- **Q2, Accept:** rapidfuzz-cpp (MIT) and dtl (BSD-3-Clause) may both go into
  the core as approved libraries. The owner was told that dtl misbehaved as a
  whole-file line matcher in this evaluation and would be used only for the
  per-line intraline comparison, where it passed every macOS gate; the
  approval is for that use.

- **Q1, Accept with a condition:** linking LGPL xdiff is acceptable "if xdiff
  is dynamically linked as its own shared library, as Qt is". Shape L may be
  adopted only as a separately built shared library that the adapter links
  dynamically, never compiled or statically linked into a BPR binary, and only
  if every gate passes. The owner also asked for L to be re-measured with the
  whitespace flags corrected (the CRLF and trailing-whitespace mapping that
  caused its two correctness refusals) before that decision.

The answers change Q1's and Q2's text, marked below; the
[Q2 record](assets/diff-engine-evaluation-v1/owner-answer-2026-10-13.md) and
the [Q1 record](assets/diff-engine-evaluation-v1/owner-answer-q1-2026-10-13.md)
keep the text before and after. Nothing else changes: the thresholds,
decision rules and acceptance criteria are as adopted, Q3 stays open,
and dtl's intraline adoption still needs the owner's Accept of its render set
(AC06). Core's link allow-list, both `AGENTS.md` rules and the system
architecture now name both libraries. With every gate passed on macOS and no
owner answer outstanding, rapidfuzz-cpp's line-pair similarity is adopted
through [[BPR-1480]]; dtl's intraline adoption waits on AC06 in [[BPR-1481]].
*AC06 answered 2026-10-13: the owner accepted the dtl render set in its own
attended-review session (`3dcb3e5660345ff127118964cc309faa`, decision
`0552741d-9e4e-4c8b-8a7b-350e9d0326a8`, no note); [[BPR-1481]] adopts dtl for
per-line intraline ranges, never as a whole-file matcher.*

## Owner answer on shape L's speed — 2026-10-13

Mandate `diff-engine-evaluation-r3`. After the L re-measurement (no refusals or
invalid results, but 1.60–2.03× the baseline's median time on L inputs, failing
the primary rule's 2× requirement), the owner answered, verbatim: "I don't care
about that, just use it anyways so we don't have to maintain ours." The answer
waives the primary rule's speed and size conditions for shape L only (the 2×
requirement and the 10% median-time regression): the owner accepts L being
slower on large inputs to stop maintaining the first-party primary matcher.
Nothing else is waived: correctness, determinism, the 10 ms stop budget,
memory, placement, Q1's condition and the rest of the primary rule still apply,
and a failure of any of them stops adoption. The
[record](assets/diff-engine-evaluation-v1/owner-answer-l-speed-2026-10-13.md)
keeps the rule's text before and after. Under it, L still fails the rule's
first clause on 7 comparisons, through the shared pairing and intraline budgets
rather than its own matcher; the owner's later answer below extends the waiver
to that clause.

## Owner answer on shape L's approximation clause — 2026-10-13

Mandate `diff-engine-evaluation-r3`. Asked whether the speed waiver also covers
the primary rule's first clause, which L failed on 7 comparisons through the
shared pairing and intraline budgets (its own matcher never approximates, and
it removes 4 of the baseline's 11 approximations), the owner answered
**Accept**. With it L passes every gate and every rule clause it is held to on
macOS: `l-myers-indent` is adopted as the primary matcher and hunk sliding
through [[BPR-1485]], with the 7 approximations as known and out of scope. The
[record](assets/diff-engine-evaluation-v1/owner-answer-l-approximation-2026-10-13.md)
keeps the amended note before and after.

## Owner answer on restart-only cancellation — 2026-10-13

Mandate `diff-engine-evaluation-r3`. Told that the worst recomputation seen on
the corpus with linked xdiff was about 264 ms, the owner answered **Accept**:
with xdiff adopted, the first-party pause-and-resume machinery of
[[BPR-1366]] is deleted, and a stopped diff is abandoned and recomputed from
scratch the next time it is needed. The 10 ms stop budget must still hold
through abandonment; only what becomes dead is removed (the step meter and the
budgets intraline, pairing and the rapidfuzz-cpp scoring use stay); tests that
assert resumption change to the restart behaviour. It takes effect with L's
adoption in [[BPR-1485]]. The
[record](assets/diff-engine-evaluation-v1/owner-answer-restart-2026-10-13.md)
keeps the amended paragraph before and after.

## Owner delegation on Linux and Windows qualification — 2026-10-13

Mandate `diff-engine-evaluation-r4`. Asked what [[BPR-1468]] (Linux) and
[[BPR-1469]] (Windows) should do now that the macOS decisions are adopted, the
owner answered "decide". The coordinating session decided: measuring the
candidates against the first-party baseline is moot, because that engine is
deleted ([[BPR-1485]]) and the opt-in evaluation build is retired. Both cards
instead qualify the adopted stack on their platform: xdiff built as its own
shared library, plus dtl and rapidfuzz-cpp; the diff suites and `core_diff_*`;
the BPR-1485 equivalence tool, reproducing the macOS digests (cross-platform
determinism) and GitHub agreement 78 of 78; and the package audits. AC02 is
amended accordingly, marked below; the
[record](assets/diff-engine-evaluation-v1/owner-delegation-platforms-2026-10-13.md)
keeps its text before and after. Nothing else changes: the macOS measurements,
decisions and owner answers stand, and Q3 stays open.

## Owner answer on dtl for Review Cycle suggestion hunks — 2026-10-13

Mandate `diff-engine-evaluation-r5`. Asked whether dtl's core approval may
extend beyond per-line intraline comparison to the Review Cycle suggestion
hunks, whose first-party Myers in `matched_lines` aligns a suggestion's
changed lines with a cap of 512 edits, the owner answered **Accept**. dtl
now has exactly two approved uses in the core: the diff engine's per-line
intraline comparison and that bounded suggestion-hunk line edit. It is still
never the diff engine's whole-file line matcher. [[BPR-1498]] carries the
change: inputs bounded as before, the 512-edit cap kept, dtl's 2,000,000-point
restart never reached, and the architecture-boundary rule allowing dtl in
exactly those two sources. Q2's text changes, marked below; the
[record](assets/diff-engine-evaluation-v1/owner-answer-dtl-suggestion-hunks-2026-10-13.md)
keeps it before and after. Nothing else changes.

## Problem and grounded current behavior

On 2026-10-13 the owner asked for an RFC "to investigate if those replacements
are suitable through our tests and measurements" for the main diff engine. This
RFC proposes that evaluation: a measured comparison that ends in a recorded
adopt-or-reject decision for each component. It does not itself replace the
engine.

**Primary line diff.** In [the engine](../../src/core/src/diff_compute.cpp),
`advance_region` (about lines 1836–1915) trims the common prefix and suffix of
each region. It then tries `lcs_matches` (about 435–514), which fills an
(N+1)×(M+1) `uint32` LCS table: O(NM) time and memory for the changed region.
A region over `MaximumMatrixCells`, 16 Mi cells
([`diff_compute.hpp`](../../src/core/include/bpr/core/diff_compute.hpp)
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
"(approximate alignment)" ([`diff_workbench.cpp`](../../src/adapters/qt/src/diff_workbench.cpp)),
but nothing says which changes degraded.

**Interruption.** [BPR-1366](../issues/60-done/BPR-1366-a-prepared-diff-computation-stops-within-milliseconds-when-the-reviewer-acts.md)
rewrote the engine as a resumable state machine (about 1521–2397, plus
[`diff_step_meter.hpp`](../../src/core/src/diff_step_meter.hpp)). The
reason was that one large file takes hundreds of milliseconds: an optimized
40,000-line pair took 191 ms uninterrupted. After the change, a host stop
returned in 1.16 ms.

**Duplicates and emulation.** Review Cycle suggestions carry a second, bounded
Myers line diff ([`review_cycle_suggestion.cpp`](../../src/core/src/review_cycle_suggestion.cpp),
`matched_lines`, about 497–547, D ≤ 512). Past that bound the whole middle
becomes one hunk. Comment-range eligibility emulates GitHub's hunks from BPR's
own change groups with ±3 lines
([`review_comment_range.cpp`](../../src/core/src/review_comment_range.cpp),
about 29–57). [Its header](../../src/core/include/bpr/core/review_comment_range.hpp)
concedes that where the diffs differ, GitHub's refusal at Publish stands.
Recorded GitHub file listings keep the provider's `patch`
([`github_read_contract.cpp`](../../src/adapters/github/src/github_read_contract.cpp),
about 1233), which gives a real hunk oracle.

**Production history.** In [BPR-0625](../issues/60-done/BPR-0625-bounded-ordered-line-matching.md),
a 20,000-line file with 20 changes exceeded the matrix budget. The card added the
ordered-unique path and called it "not a general replacement diff algorithm."
In [BPR-0719](../issues/60-done/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md),
the owner could not open `ScreenBufferTests.cpp` (about 8,000 lines, +291) in
microsoft/terminal#20607. That card's scope asked for "patience or Myers", but
only trimming and patience landed. Its synthetic replica is
`test_large_files_degrade_instead_of_refusing` in
[`diff_compute_tests.cpp`](../../tests/core/diff_compute_tests.cpp).
BPR-1366 is the third case, described above.

**Contract room.** [RFC-BPR-0004 §9](RFC-BPR-0004-diff-computation-line-mapping-grouping.md#9-diff-and-refinement-pipeline)
says "The specific line-diff algorithm is not a public product contract". It
requires validation of coverage, monotonicity, identity and bounds before
publication, which `validate_computed_file_diff` performs, including
equal-content checks. The comparison key carries an algorithm revision
(`supported_diff_algorithm_revision`, currently 1, `diff_compute.hpp` line 17).

**Boundaries.** The core is standard/domain C++ only
([dependency direction](../architecture/system-architecture.md#build-target-and-dependency-direction),
[`VerifyArchitectureBoundaries.cmake`](../../cmake/VerifyArchitectureBoundaries.cmake)),
and [its target](../../src/core/CMakeLists.txt) links no library.
Third-party code lives under [`third_party/`](../../third_party/) with a
`PROVENANCE.md`, and is inventoried in
[`packaging/dependencies.json`](../../packaging/dependencies.json). The
core so far contains generated Unicode data but no third-party code.
The [installed-Git adapter](../../src/adapters/git/src/installed_git_process.cpp)
already runs Git as an isolated child: `GIT_CONFIG_NOSYSTEM`, a controlled
global config, and a kill on stop. It already runs `diff-tree -p` for
publication. Git is admitted from 2.39 onward
([build and test](../development/build-and-test.md#retained-git-history-and-epoch-storage)).
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

**Fixtures.** [large-review-v1](../../tests/fixtures/github/pull-requests/large-review-v1/README.md)
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

**Measurement.** Use the [RFC-BPR-0013 convention](RFC-BPR-0013-performance-resilience-resource-limits.md#performance-budgets)
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
| Primary matcher | On L and pathological inputs it produces no approximate result where the baseline does, and is at least 2× faster or 2× smaller in median. No class regresses its median by more than 10%. GitHub hunk agreement is at least the baseline's, and comment-range flips do not increase. *For shape L the owner waived the speed and size conditions on 2026-10-13 (the 2× requirement and the 10% median-time regression) and the approximation clause (7 comparisons stay approximate through the shared pairing and intraline budgets); the rest of the row and every gate still apply.* |
| Hunk sliding | GitHub hunk agreement rises, and it costs at most 5% of compute. |
| Pair similarity | Pairing decisions are bit-identical to the baseline's over the corpus, and the pairing phase is at least 2× faster on M and L. |
| Intraline | Every span is valid, p95 time per pair is no worse, and the owner accepts a fixed render set in an attended review. Semantic cleanup changes what is emphasized, so this is a look-and-judge criterion. |
| Suggestion hunks | The adopted primary matcher passes the Review Cycle suggestion tests, and its behavior past the current bound is no worse. |

A rejected component keeps the current engine, and a result of no change is
valid. The report also states whether the best candidate's worst observed time
would make restart-only cancellation enough. It does not remove BPR-1366's
resumable machinery. *Answered 2026-10-13: restart-only cancellation is
accepted once xdiff is adopted. The BPR-1366 pause-and-resume machinery is
then deleted; a stopped diff is abandoned and recomputed from scratch when
next needed, and the 10 ms stop budget holds through abandonment* (see "Owner
answer on restart-only cancellation" above).

### Open questions

- **Q1. Is linking LGPL xdiff acceptable?** Statically linking LGPL-2.1
  code into a distributed desktop app brings relinking and source-offer duties
  beyond the current Qt (dynamic) and KSyntaxHighlighting arrangements. Shape L
  is measured but cannot be adopted until the owner answers. *Answered
  2026-10-13: Accept, if xdiff is dynamically linked as its own shared
  library, as Qt is* (see "Owner answer" above). Static linking stays
  refused. Shape L is re-measured with corrected whitespace flags and is
  adopted only if every gate passes.
- **Q2. May vendored pure-computation third-party code enter the core?**
  This affects dtl, rapidfuzz-cpp, a diff-match-patch port and a JGit-derived
  histogram port. *Answered 2026-10-13: Accept for rapidfuzz-cpp and for dtl,
  dtl for per-line intraline comparison only* (see "Owner answer" above). The
  diff-match-patch port and a JGit-derived histogram port were not asked about
  and remain unapproved; both were rejected on measurement. *Extended
  2026-10-13: dtl may also compute the Review Cycle suggestion hunks' line
  edit, bounded at 512 changes, and never the diff engine's whole-file line
  matcher* (see "Owner answer on dtl for Review Cycle suggestion hunks"
  above).
- **Q3. May comparison results depend on the user's installed Git version?**
  If Git is upgraded, the same comparison key could yield a different valid
  alignment, unless the version enters comparison identity. This affects P
  adoption only. The evaluation reports cross-version agreement so the
  question can be answered on evidence.

## Acceptance criteria

- **AC01:** The opt-in harness runs every candidate–component pairing and the
  baseline over the full suite list and corpus through first-party validation.
  The committed report shows each gate's pass or fail with its commands and
  counts.
- **AC02:** Measurements on macOS, Linux and Windows cover every size class
  and the measurement list under "Corpus and measurement", with the baseline
  measured in the same runs and conditions. *Amended 2026-10-13 under the owner's delegation: macOS carries the
  measurements; on Linux and Windows the adopted stack is qualified instead
  (shared-library xdiff, dtl and rapidfuzz-cpp built; the diff suites pass; the
  BPR-1485 equivalence tool reproduces the macOS digests and GitHub agreement
  78 of 78; the package audits pass), because the baseline is deleted* (see
  "Owner delegation on Linux and Windows qualification" above).
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
