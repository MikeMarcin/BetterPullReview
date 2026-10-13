# RFC-BPR-0082 diff-engine evaluation report

[RFC-BPR-0082](../rfcs/RFC-BPR-0082-diff-engine-evaluation.md), mandate
`diff-engine-evaluation-r1`. Immutable RFC base
`cc9e9376e1fc02e2cdf3154fa56dfcc14991a8d5`. Station measured: **macOS only**
(Apple M4 Pro, 14 cores, 48 GiB, macOS 26.6.2, Apple clang 21, Release build).
Linux ([[BPR-1468]]) and Windows ([[BPR-1469]]) are not measured; nothing in
this report is a claim about them, and every gate below that says "on every
platform" is so far satisfied or failed on macOS alone.

Raw results: [macOS raw results](RFC-BPR-0082/macOS-raw.json) (every sample
time and every outcome), the [generated macOS tables](RFC-BPR-0082/macOS-results.md)
and the [suite adjudication](RFC-BPR-0082/suite-adjudication.json). The GitHub
oracle is [`diff-oracle-v1`](../../tests/fixtures/github/diff-oracle-v1/README.md).
The harness is described in its
[README](../../tests/evaluation/diff_engine/README.md).

## Recorded outcome per component

The thresholds are the RFC's, fixed before measurement. Each decision follows
mechanically from "Decision rules"; the gate and rule results behind it are in
the generated tables. No owner question is answered here. "Eligible pending"
means the candidate passed every gate and rule this station can decide, and
adoption waits on the named owner answer; it is not adopted.

| Component | Outcome | Deciding evidence |
|---|---|---|
| Primary line matcher | **No change.** Every candidate rejected. | P and L fail the correctness gate (2 refusals each, see below) and the primary rule; C O(NP) (dtl) fails correctness (crash, 6 invalid results refused, a resumed run that never ends); the first-party histogram fails the rule (slower on L, GitHub agreement below the baseline's). No candidate is 2× faster or smaller on L and pathological inputs. |
| Hunk sliding | **No change.** Every candidate rejected. | Git's indent heuristic raises GitHub agreement (P 65→77, L 66→78 of 78) at no measurable cost, but the P and L matchers it comes with fail the correctness gate. The core post-pass raises agreement (45→51) but costs 5–9% of compute per class, above the 5% limit. |
| Line-pair similarity | **Eligible pending Q2** (rapidfuzz-cpp is not an approved core library). | rapidfuzz-cpp 3.3.4 Indel: pairing decisions bit-identical to the baseline's on all 86 comparisons; pairing phase 0.10× the baseline's on M and L; every gate passes on macOS. |
| Intraline ranges | **O(NP) over scalars (dtl): eligible pending the owner's render review (AC06) and Q2.** diff-match-patch: rejected. | dtl: every span valid, p95 per pair 0.0227 ms against 0.0250 ms, every gate passes on macOS. diff-match-patch with semantic cleanup: p95 per pair 0.0315 ms, worse than the baseline. |
| Review Cycle suggestion hunks | **No change.** | No primary matcher is adopted, so nothing replaces the bounded Myers. |

A result of no change keeps the current engine. Restart-only cancellation is
**not** enough for any candidate: the primary candidates' worst observed times
on this corpus run from 214 ms (dtl) through 264 ms (L histogram) to 460 ms
(the first-party histogram, the only primary candidate without a refusal),
against the 10 ms stop budget, so BPR-1366's resumable machinery stays
whatever is adopted.

## What was evaluated

| Name | Component | Shape | Source and pin |
|---|---|---|---|
| `baseline` | all | — | the engine at RFC_BASE_SHA |
| `p-<git>-<alg>` | primary | P | Git 2.39.5 and 2.56.0 built from kernel.org archives (SHA-256 checked against kernel.org's list), and the installed Git 2.50.1 (Apple Git-155); `myers`, `minimal`, `patience`, `histogram`; `--no-indent-heuristic` |
| `p-<git>-<alg>-indent` | sliding | P | the same with `--indent-heuristic` |
| `l-<alg>[-indent]` | primary / sliding | L | xdiff from Git 2.56.0, compiled with a first-party shim and called through `xdl_diff` |
| `c-onp` | primary | C | dtl 1.21 (O(NP), Wu–Manber–Myers) over the engine's line classes |
| `c-histogram` | primary | C | first-party histogram matcher written for the evaluation, falling back to dtl |
| `c-slide` | sliding | C | first-party post-pass written for the evaluation |
| `c-rapidfuzz` | pairing | C | rapidfuzz-cpp 3.3.4 `CachedIndel`, integer scores from the Indel distance |
| `c-dmp` | intraline | C | diff-match-patch-cpp-stl `ba3bbe3` (Apache-2.0 port of Google's), semantic cleanup |
| `c-onp-intraline` | intraline | C | dtl 1.21 over Unicode scalars |

The JGit-derived histogram of the RFC is measured as xdiff's
`xhistogram.c` inside L (that file is JGit's, under EDL-1.0); the C histogram
is first-party, not a port. Every archive is acquired by the verified-archive
pattern (SHA-256, size and licence digest, re-extracted under the build tree)
behind `BPR_DIFF_ENGINE_EVALUATION`, off by default.

## Method

**Seam** ([[BPR-1465]]). New core overloads take a supplied primary match list
(the "bounded observation") or components that supply primary matches, a
post-pass over them, pair scores or intraline scalar matches. The engine
assigns line classes, validates every supplied value and refuses a comparison
whose values break its rules, so a candidate never publishes an invalid
alignment. The existing entry points were shown byte-identical to
RFC_BASE_SHA over 1,162 comparisons.

**Hook.** In the evaluation build the core is compiled with
`BPR_DIFF_ENGINE_EVALUATION`, and every executable links a hook that hands the
ordinary entry points the selected candidate. The existing suites therefore run
unmodified against each candidate. The process shape inside the hook runs Git
from the core's primary step (`git diff --no-index` over temporary files); the
harness instead measures P as it would ship: blobs already in a retained
repository, `git diff <blob> <blob>`, launched, stopped, killed and reaped
through the Git adapter's `process_support` supervision, then
`compute_file_diff(input, observation)`.

**Corpus** (86 comparisons). 12 BPR-1366 benchmark pairs (200 to 100,000 lines,
30 and 300 edits per thousand; byte-identical to the C++ benchmark's), 56 real
pairs (BPR-0625's large-review-v1 comparison and generated table, the BPR-0719
replica, google/UIforETW#74's five files, and 48 files changed between Git
2.39.5 and 2.56.0), and 18 pathological comparisons (identical lines,
repetitive, phase-shifted, one huge replacement, 12,000-scalar lines, moved
blocks, whitespace-only, Unicode, CRLF and lone CR; the whitespace and CRLF
pairs also under `ignore_trailing`, `ignore_all` and normalized endings). The
real `ScreenBufferTests.cpp` is not committable, so the BPR-0719 replica stands
in for it.

**GitHub oracle** (78 comparisons). UIforETW#74's recorded patches and 73
pairs recorded on `MikeMarcin/BPR-Test` (pull requests 52–69; see the
fixture README). Directxmath-336 is not used: its recording has no base
contents. The other `contracts/v1/*/files.json` patches are written by the
fixture generators, not by GitHub, so they are not an oracle.

**Statistics**, fixed in `make_report.py` before the final pass: a pair's
time is the median of 9 samples (3 interleaved rounds × 3 repetitions, the
candidate order rotated per pair and round); a class's ratio is the median over
its comparisons of candidate time over baseline time; memory ratios the same
from the peak memory the comparison added (Git's included for P);
stop-to-return is taken over samples a stop actually interrupted, at 15–95% of
the uninterrupted time. The station was shared with other agents' builds: the
one-minute load average during the timing pass had a median of 4.6 and peaked
at 35 on 14 cores, so absolute times are upper bounds; the interleaving keeps
the ratios comparable.

**Suites.** The RFC's list ran against every candidate through `ctest` in the
evaluation build: `core_diff_compute`, `core_diff_semantics`,
`core_diff_transition`, `core_review_comment_range`,
`core_review_cycle_comparison`, `core_review_cycle_suggestion`,
`core_diff_compute_benchmark`, `application_diff_host`, all 15
`application_large_review_*` courses, and the diff fuzz target's corpus through
an uninstrumented replay driver (the libFuzzer graph is a separate
configuration). `core_diff_compute` and `application_diff_host` stop at their
first failed expectation, so generated copies that count failures instead
(inventories) list every changed expectation; each is adjudicated in
[suite-adjudication.json](RFC-BPR-0082/suite-adjudication.json).

## Gates, with commands and counts (macOS)

| Gate | How it was decided | Result |
|---|---|---|
| Correctness | `run_suites.py` and `run_inventories.py` against each candidate; `measure` validates all 86 comparisons per candidate (`validate_computed_file_diff`) and compares the reason with the baseline's. | Every P and L candidate: 2 refusals (`patho-crlf` under `ignore_trailing` and `ignore_all`: Git counts the CR of a CRLF line as trailing whitespace, the engine keeps line endings exact, and its class check refuses the matches). dtl as primary: 6 refusals (its restart past 2,000,000 path points returns common elements that join unequal lines), a crash (`std::length_error`) in `application_diff_host`, and a resumed run that never ends. Every other candidate: zero invalid results, zero new refusals, and only changed expectations adjudicated as equally valid, interruption-model or hook artefacts. |
| Determinism | `determinism` runs every candidate twice per comparison; P's digests compared across Git 2.39.5, 2.50.1 and 2.56.0. | Repeatable on 86/86 for every candidate. P `myers` and `patience` identical across the three Gits; `minimal` differs on 14 comparisons between 2.39.5 and the two newer Gits; `histogram` differs on 1 between 2.56.0 and the two older. Cross-platform: not yet measured. |
| Interruption | `stops`, on the 28 L-size and pathological comparisons, 10 stops each. | Baseline p95 0.40 ms. C candidates 0.22–0.77 ms. L returns at once by abandoning its worker, whose abandoned work runs on (p95 102–163 ms). P 8.1–12.0 ms: the Git adapter's supervision polls every 10 ms wait slice, so P's latency is bounded by that slice plus the kill and reap; 2.39.5/2.56.0 variants sit around the 10 ms line, the installed Git's above it. |
| Placement | Boundary checks in the proposed placement. | P and L live in an adapter; C candidates would sit in the core, where dtl, rapidfuzz-cpp and diff-match-patch need an owner library approval (Q2) and the first-party matcher and post-pass need the owner's approval of hand-written diff code in the core (AGENTS.md). P keeps the engine as its fallback. The shipping configuration acquires and links nothing new (below). |
| Licence | Per shipped file, below. | Recorded; owner answers named per candidate. |

The package and boundary check: a fresh `dev` configuration with the option
off has `BPR_DIFF_ENGINE_EVALUATION:BOOL=OFF`, no `_bpr/diff-evaluation`
stage and no reference to an evaluation target or definition in
`build.ninja`; `packaging/`, `third_party/` and `cmake/` are unchanged by this
work; `core-only` (269 of 273 tests pass, the four others are worktree
environment and pre-format effects) and `core_dependency_boundary`,
`core_boundary_contract`, `architecture_boundaries`,
`architecture_boundary_contract` and `git_protocol_boundary` pass.

## Licence, linkage and obligations

| Candidate | Files that would ship and their licence | Linkage | Obligations | Platform and Git-version dependence | Owner answer needed |
|---|---|---|---|---|---|
| P | none: runs the user's Git (GPL-2.0, a separate program) | process | none for BPR's distribution while Git is not bundled | needs Git ≥ 2.39 on every platform, with the engine as fallback; results depend on the Git version (`minimal` and `histogram` above) and the per-file launch costs 15 ms (pinned Git) to 28 ms (Apple's `/usr/bin/git` shim) | Q3 |
| L | `xdiff/*.c,*.h` of Git 2.56.0: LGPL-2.1-or-later, except `xhistogram.c`, EDL-1.0 | static, in an adapter | LGPL relinking (object files or a relinkable build) and a source offer; LGPL and EDL notices | none beyond the compiler; no stop hook (a stop abandons a worker) | Q1 |
| C dtl | `dtl/*.hpp` 1.21: BSD-3-Clause ("so-called BSD licence") | header-only, core | notice | the defects above would need fixing in a modified copy | Q2 |
| C rapidfuzz | `rapidfuzz/**` 3.3.4: MIT | header-only, core | notice | none | Q2 |
| C diff-match-patch | `diff_match_patch.h` `ba3bbe3`: Apache-2.0 | header-only, core | licence and NOTICE; a modified copy must mark its changes, and a core copy needs one: the port calls `clock()` in its bisection loop even with its deadline disabled | none | Q2 |
| C first-party | evaluation code, BPR's licence | core | none | none | owner approval of a hand-written diff algorithm in the core |

## Findings outside the decision rules

- **GitHub's hunks are xdiff Myers with the indent heuristic, joined at one
  line.** Linked xdiff Myers with the indent heuristic agrees with GitHub on
  78 of 78 recorded files once hunks one unchanged line apart are joined, which
  is what GitHub's REST file list does (`--inter-hunk-context=1`); its raw
  `.diff` does not join them, and on all 58 files with both the join turns one
  into the other exactly. BPR's comment-range emulation
  (`github_review_hunks`) joins only touching hunks, so it disagrees with
  GitHub wherever two hunks are one line apart, whatever the matcher: the
  baseline's BPR hunks match GitHub on 32 of 78 files, xdiff Myers with the
  heuristic on 46. A follow-up can fix the join without touching the matcher.
- **The Git command line is not GitHub.** `git diff` with Myers and the
  heuristic agrees on 77 of 78: on the BPR-0719 replica it slides the insertion
  differently, consistent with the command line trimming the common tail before
  xdiff, which `xdl_diff` and GitHub do not.
- **dtl 1.21 is unsafe as shipped**: invalid edit scripts past 2,000,000 path
  points, an uncaught `std::length_error`, and a plain `new[]` that a stop
  unwinding through it leaks.
- **GitHub's pull-request diff budget.** One pull request with all 73 files
  stopped producing patches after about 160,000 changed lines (`+0 -0`, no
  patch); large single files lose their file-list patch but keep a raw diff.
- **Pre-existing failures in this optimized build**, with the baseline:
  `core_diff_compute` fails "[[BPR-1366]] every phase of the large pair does
  work" in Release, also at RFC_BASE_SHA, and passes in the Debug `core-only`
  build; `application_large_review_responsiveness` and `_launch` fail "Down
  from a line out of view did not reveal the next", and `_cache_bytes` its
  footprint budget. They are the same for every candidate and are not
  attributed to any.
- **An exact primary costs pairing budget.** Exact matchers leave more and
  larger replacements for pairing, which spends the shared matrix budget: P and
  L lose intraline emphasis on 11,134 pairs against the baseline's 5,399 and
  stay approximate on 7 of the baseline's 11 approximate L and pathological
  comparisons. The intraline candidates do not charge that budget and lose
  emphasis on 858.

## Open questions and the evidence for them

- **Q1 (linking LGPL xdiff).** Blocks every L shape. Evidence: L is the most
  GitHub-faithful matcher measured (78/78 with the heuristic), 0.67–0.76× the
  baseline's time on M and pathological inputs but about 2× slower on L, and
  needs relinking and source-offer arrangements beyond today's Qt and
  KSyntaxHighlighting ones. Its correctness failure here is the whitespace
  mapping (CR as trailing whitespace), which an adapter can avoid by mapping
  fewer flags; that variant was not measured.
- **Q2 (libraries in the core).** Blocks `c-rapidfuzz` and `c-onp-intraline`,
  the two candidates that pass every macOS gate. Evidence: rapidfuzz-cpp gives
  identical pairing decisions with a pairing phase 10× faster (MIT,
  header-only); dtl over scalars gives valid, slightly faster intraline spans
  (BSD-3-Clause, header-only, with the defects above in its line-scale use).
- **Q3 (results depending on the installed Git).** Blocks every P shape.
  Evidence: `myers` and `patience` were identical across 2.39.5, 2.50.1 and
  2.56.0 on all 86 comparisons, `minimal` changed on 14 and `histogram` on 1;
  the installed Apple Git adds 13 ms per launch over a plain build.
- **AC06.** The dtl intraline candidate's render set (10 corpus pairs whose
  emphasis differs from the baseline's, rendered by the product's diff
  workbench) is prepared for its own attended-review session; the owner's
  Accept or Reject is pending.

## Platform status

macOS: measured (this report). Linux and Windows: the cards [[BPR-1468]] and
[[BPR-1469]] are eligible for those stations and unmeasured; cross-platform
determinism is therefore open. The process shape's hook uses `posix_spawn`, so
the Windows card also ports the hook's launch.
