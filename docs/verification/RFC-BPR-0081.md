# C++ identifier parser evaluation: report

[RFC-BPR-0081](../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md) asked
whether tree-sitter-cpp should replace the structural guesswork of the shipped
C++ identifier refinement. Mandate `cpp-identifier-parser-evaluation-r1`;
RFC_BASE_SHA `530d92eaff1578413c0e669f476e740387441152`. This page is the
committed report. Raw results are in
[`tests/syntax_evaluation/results/macos/`](../../tests/syntax_evaluation/results/macos/README.md);
the harness, candidate, corpus and labels are described in
[`tests/syntax_evaluation/`](../../tests/syntax_evaluation/README.md).

## Outcome: extend

**Candidate T is rejected at both pins.** On macOS it fails three of the
RFC's gates, each of which rejects it alone: Contract, Bounded work and
Memory. The Contract failure comes from the grammar itself, so it does not
depend on the platform. The `master` pin would also have needed Q1, which
stays open and is now moot.

**The outcome is "extend".** The gap suite confirms seven failures of the
shipped heuristic inside RFC-BPR-0072's scope (H1 to H7), and H8 is confirmed
as well. [BPR-1474](../issues/10-backlog/BPR-1474-fix-the-confirmed-cpp-identifier-gap-classes.md)
fixes those classes in the heuristic. It may not take
`cpp_identifier_context.cpp` past 800 lines or add a pass over the whole blob.

**When to run the evaluation again:**
- if a fix in BPR-1474 would need more than 800 lines or a new whole-blob pass,
  the parser decision reopens with this report's numbers;
- if the refinement reaches 800 lines;
- if a new identifier category needs structural recognition;
- if tree-sitter-cpp gains enumerator attributes and recovers from
  `NAMESPACE_BEGIN`-style macros, so that the contract twin passes and T's
  false negatives fall below the baseline's.

**What is still owed.** Windows x64
([BPR-1443](../issues/10-backlog/BPR-1443-measure-the-parser-evaluation-on-windows.md))
and Linux x64
([BPR-1444](../issues/10-backlog/BPR-1444-measure-the-parser-evaluation-on-linux.md))
have not been measured. AC03 requires their numbers, but they cannot change
the outcome: a gate that fails on one platform rejects T whatever the others
show. Nothing below claims a Windows or Linux result.

## Gates

Thresholds are the RFC's, fixed before measurement. Every number in this page
was measured on macOS. T means the candidate at the pin named.

| Gate | T at v0.23.4 | T at `master` | Evidence (macOS) |
|---|---|---|---|
| Contract | **Fail** | **Fail** | `syntax_classification_contract@ts-*` fails "nested initializers and attributes preserve enumerator boundaries". Neither grammar accepts an attribute on an enumerator (`enumerator: name (= value)?` in both `grammar.json` files), so `FIFTH [[deprecated(...)]] = …` closes the enum with a MISSING `}` and `LAST` is lost. The twins of `syntax_refinement_cancellation`, `qt_syntax_pane_composition` and `application_cpp_identifier_appearance` pass. `application_appearance_host` does not link the syntax adapter. |
| No new errors | Pass | Pass | No category gains a false positive (table below). The set of 617 `macro_keep` misses is the same for the baseline and for T. |
| Bounded work | **Fail** | **Fail** | Measured from a stop requested at a uniform point of the refinement. On L inputs, T's p95 is 7.3 ms at v0.23.4 and 12.1 ms at `master`; with the span limit raised it is 35.1 ms and 28.5 ms. On pathological inputs the worst case is 233 ms and 608 ms, from 8 MiB of `{`. No stop published a partial answer. The slowest single input took 7.2 s. Sanitizers and fuzzing reported nothing. Deep nesting completes. |
| Memory | **Fail** | **Fail** | Peak refinement heap is 2.2 to 2.75 times the baseline in S, M and L. 8 MiB of `{` takes 2.24 GB, over the 512 MiB cap. |
| Determinism | Pass on macOS | Pass on macOS | Three rounds gave identical spans, and the Release and ASan/UBSan builds gave identical spans for all 10,536 file and engine results. The comparison across platforms is owed to BPR-1443 and BPR-1444. |
| Placement | Pass | Pass | The candidate's objects are the only tree-sitter link input outside `tests/syntax_evaluation`. The adapter archive has no `ts_` symbol. `core-only`, `architecture_boundaries` and `architecture_boundary_contract` pass. |
| Supply | Pass | Open (Q1) | The runtime and grammar are MIT and pinned by SHA-256, size and licence digest. `master` is an untagged commit. |

Interpretation recorded: the Bounded-work gate is applied to a stop that lands
inside the refinement, the stage T replaces. A stop inside the grammar pass
waits for KSyntaxHighlighting's 512-line check, which is the same code for
both engines; the "whole call" figures below show it.

### Adoption conditions (moot; recorded for completeness)

| Condition | T at v0.23.4 | T at `master` |
|---|---|---|
| Gain: 25% fewer false negatives in scope | No. Baseline 2,959, T 7,788 (+163%) | No. Baseline 2,959, T 7,738 (+162%) |
| Gain, alternative: passes half the hypotheses confirmed on the baseline | Yes, 6 of 8 (H1-H3, H5-H7) | Yes, 6 of 8 |
| Cost: median total per class at most 1.25 times the baseline | No. S 1.65, M 1.65, pathological 2.02, fragment 1.91 | No. S 1.68, M 1.76, pathological 2.01, fragment 1.96 |
| Cost: `syntax_first_file` and `syntax_navigation` at most 1.10 times | Yes, 0.96 and 1.02 | Yes, 0.97 and 1.02 |
| Cost: at most 8 MiB per slice | Yes. +3.40 MiB arm64, +3.35 MiB x86_64 | Yes. +5.48 MiB arm64, +5.40 MiB x86_64 |
| Cost: clean `dev` build at most 10% longer | Yes, at most +0.78% | Yes, at most +0.77% |

## What T is

T is
[`candidate/cpp_identifier_context_tree_sitter.cpp`](../../tests/syntax_evaluation/candidate/cpp_identifier_context_tree_sitter.cpp).
It defines `refine_cpp_identifiers` with the shipped signature, and its
division of labour is the RFC's.

- **From tree-sitter:**
  - tokenization and delimiter and angle pairing;
  - recognition of namespace, alias, `using namespace`, enum and type heads;
  - enumerator boundaries;
  - declarators that shadow a name.
- **Unchanged from the shipped code:**
  - grammar protection;
  - the scope and symbol table;
  - the conservative rules;
  - painting precedence;
  - the lexical macro convention;
  - publication;
  - the span and cancellation rules.

Each classification uses one parser on the calling thread, with a progress
callback that reports the stop token. The parser is reset after a cancelled
parse, and `ts_set_allocator` is never called (only the harness counts
allocations).

Two decisions were made while T was developed, before its final measurement.
Both are recorded here.

- **A qualified chain counts only as far as the source text joins its names
  with `::`.** Error recovery formed `qualified_identifier` nodes across
  `ABSL_DLL absl::…`, which painted macros as types; this check fixed that.
- **Declarations tree-sitter recovered inside ERROR nodes still supply facts
  ("lenient").** The alternative ("strict") gives the same false positives and
  more false negatives: 9,018 at v0.23.4 and 8,971 at `master`, against 8,405
  and 8,355. It is kept as the diagnostic harness `bench_syntax_evaluation_<pin>_strict`.

The large gap in false negatives comes from macros and error recovery:

- **Namespace macros.** On `namespace absl { ABSL_NAMESPACE_BEGIN namespace
  time_internal {`, tree-sitter parses the macro line as the start of a
  function definition, and the inner namespace is lost. The lexical baseline
  still sees `namespace time_internal {`. Abseil, quill and fmt open
  namespaces this way.
- **Whole-file failures.** For 9 to 11 of the 3,884 labelled files, the
  parse root is an ERROR node. One of them is the largest first-party file,
  `github_application.cpp`.
- **Partial failures.** About a third of those files contain at least one
  ERROR node.

## Corpus and labels

Labels were committed in `309375c5a` before any candidate ran over the corpus
([BPR-1440](../issues/60-done/BPR-1440-commit-the-labelled-cpp-identifier-corpus.md);
[corpus README](../../tests/syntax_evaluation/corpus/README.md)). The corpus has
5,270 files:

| Class | Files |
|---|---|
| contract | 1,101 |
| first-party at `530d92eaf` | 999 (4 of them L-size concatenations) |
| abseil, RE2, glaze, quill, KSyntaxHighlighting | 1,788, named by archive digest |
| gap suite | 11 |
| fragments | 420 |
| malformed and pathological | 45 |
| composer replay | 906 |

Clang 23's AST labels the contract, first-party and third-party classes, and
the accepted conservative rules override it where they differ. Twelve
independent labellers, blind to the machine labels, reviewed 1,994 sampled
items and agreed with 98.4% of them. Adjudication corrected the fragment rule
and is kept in `labels/hand-review.jsonl`. The gap suite is labelled by hand.

## Accuracy, per category

A label's predicted role is the role every one of its bytes carries. False
positives also count unlabelled namespace and enum-member runs in fully
labelled files.

| Category | Baseline FP | Baseline FN | T v0.23.4 FP | T v0.23.4 FN | T `master` FP | T `master` FN |
|---|---|---|---|---|---|---|
| ns_decl | 0 | 243 | 0 | 815 | 0 | 803 |
| enumerator_decl | 1 | 8 | 1 | 335 | 1 | 324 |
| enum_use | 12 | 438 | 4 | 698 | 4 | 698 |
| ns_qualifier | 82 | 2,270 | 34 | 5,940 | 37 | 5,913 |
| conservative_negative | 41 | 0 | 38 | 0 | 38 | 0 |
| macro_keep | 0 | 617 | 0 | 617 | 0 | 617 |
| protected | 0 | 0 | 0 | 0 | 0 | 0 |
| **all** | **136** | **3,576** | **77** | **8,405** | **80** | **8,355** |

The per-source tables and every label disagreement are in
`results/macos/summary.json` and `results/macos/failures-*.jsonl.gz`.

### Gap hypotheses

| Hypothesis | Baseline | T (both pins) | What happened |
|---|---|---|---|
| H1 `namespace A::inline B {` | confirmed | refuted | The baseline recognizes nothing; T recognizes `A`, `B` and the qualified use |
| H2 `namespace [[deprecated]] Legacy {` | confirmed | refuted | |
| H3 `enum { A = X < 3, B = Y > 2, C }` | confirmed | refuted | The baseline loses `High` |
| H4 X-macro in an enum body | confirmed | confirmed | Both paint `COLOUR_LIST` as an enumerator |
| H5 enum opened in both `#if` branches | confirmed | refuted | The baseline paints `b` and `c` as enumerators after the enum |
| H6 `int Reason{ 0 };` shadow | confirmed | refuted | |
| H7 `enum E { café }` | confirmed | refuted | The baseline paints only `caf` |
| H8 largest temporary footprint | confirmed: 2.02 GB on `brace-flood-8388608.cpp` | confirmed: 2.24 GB on the same input | `enum-names-8mib.cpp` exceeds the 262,144-span limit in the grammar pass, so no refinement runs on it |

The gap suite also shows two limits. The baseline does not find an enumerator
across a split `extern "C" {`. T loses the enumerators under merge-conflict
markers.

## Measurements (macOS)

**Station:**
- Apple M4 Pro, 14 cores, 48 GB, macOS 26.6.2;
- Apple clang 21 in Release (`-O3`) for timing;
- Qt 6.11.2 and KSyntaxHighlighting 6.28.1.

**Method:**
- one process per corpus class and pin;
- the baseline and T interleaved in the same process, alternating order;
- three rounds per file, medians;
- 20 stop trials per engine and mode on L and pathological inputs.

The station was not idle. Other agents' builds held the load average between 10
and 100 during the runs, and `run.json` records it. Ratios measured within one
process are the robust figures; absolute times are noisy.

### Classification time (ms per file)

These are medians of 3 rounds per file, then the median and p95 across files.

| Size class | Files | Stage | Baseline median | T v0.23.4 median | T `master` median | Baseline p95 | T v0.23.4 p95 | T `master` p95 |
|---|---|---|---|---|---|---|---|---|
| S | 4,684 | total | 0.58 | 0.96 | 0.95 | 4.5 | 7.2 | 7.8 |
| S | | refinement | 0.031 | 0.389 | 0.392 | 0.23 | 3.04 | 3.10 |
| M | 114 | total | 18.0 | 29.7 | 32.5 | 76.0 | 123.8 | 158.2 |
| M | | refinement | 0.85 | 11.96 | 11.93 | 3.3 | 41.3 | 56.3 |
| L (limit raised) | 5 | total | 538 | 764 | 767 | 1,460 | 2,118 | 2,086 |
| L (limit raised) | | refinement | 24.0 | 255.7 | 255.5 | 66.3 | 709.4 | 697.8 |
| pathological | 45 | total | 1.48 | 2.99 | 2.71 | 310 | 1,515 | 1,903 |
| pathological | | refinement | 0.10 | 1.54 | 1.28 | 13.5 | 1,216 | 1,612 |
| fragment | 420 | total | 0.096 | 0.184 | 0.181 | 0.79 | 1.35 | 1.37 |

**L inputs at the product's limit.** Four of the five L inputs exceed the
product's 262,144-span limit in the grammar pass, so the refinement never runs
on them. The "limit raised" rows come from a separate run with a 4,000,000-span
limit, labelled as a diagnostic. At the product's limit, the L refinement p95
is 13.2 ms for the baseline against 140.9 ms for T at v0.23.4, and 30.4 ms
against 280.4 ms at `master`.

**Grammar pass.** It took the same time for both engines in every class
(ratios 0.95 to 1.10).

**Composer replay**, per edit (906 edits, whole texts):

| Engine | Median | p95 |
|---|---|---|
| Baseline | 1.35 ms | 2.08 ms |
| T | 2.41 ms | 3.46 ms |

**`application_large_review_benchmark --prepared-open`.** Five interleaved
rounds of each binary; one baseline round failed the course's own prepared-open
tolerance and is excluded.

| Phase | Baseline | T v0.23.4 | T `master` |
|---|---|---|---|
| `syntax_first_file` median | 200.2 ms | 193.1 ms | 194.4 ms |
| `syntax_navigation`, median of run medians | 469.7 ms | 478.1 ms | 480.6 ms |

### Memory

**Peak refinement heap** comes from counting allocators: the harness's global
`operator new` and tree-sitter's allocator.

| Size class | Baseline max | T v0.23.4 max | T `master` max |
|---|---|---|---|
| S | 0.88 MB | 2.35 MB (2.68x) | 2.35 MB (2.68x) |
| M | 8.6 MB | 19.2 MB (2.23x) | 19.2 MB (2.23x) |
| L (limit raised) | 90.8 MB | 217.6 MB (2.40x) | 220.1 MB (2.36x) |
| pathological | 2,023 MB | 2,243 MB (1.11x) | 2,243 MB (1.11x) |
| fragment | 0.14 MB | 0.25 MB (1.83x) | 0.25 MB (1.83x) |

**Peak resident memory per corpus-class process:**

| Process | Peak resident memory |
|---|---|
| first-party | 158 MB |
| third-party | 75 MB |
| contract | 68 to 75 MB |
| malformed and pathological | 4.5 GB (`master`) and 5.4 GB (v0.23.4), dominated by the brace flood |

### Time from stop to return (ms)

| Inputs / where the stop lands | Engine | v0.23.4 p95 | v0.23.4 max | `master` p95 | `master` max |
|---|---|---|---|---|---|
| L, refinement | baseline | 0.16 | 0.17 | 2.21 | 10.6 |
| L, refinement | T | 7.32 | 11.5 | 12.13 | 12.2 |
| L (limit raised), refinement | baseline | 0.75 | 0.81 | 0.59 | 1.03 |
| L (limit raised), refinement | T | 35.1 | 39.2 | 28.5 | 37.7 |
| pathological, refinement | baseline | 0.30 | 33.8 | 1.94 | 559 |
| pathological, refinement | T | 7.20 | 233 | 8.80 | 608 |
| pathological, whole call | baseline | 28.1 | 288 | 39.7 | 758 |
| pathological, whole call | T | 29.5 | 255 | 31.9 | 497 |

**T's worst cases are all 8 MiB of `{`.** The per-input median there is 58 ms
at v0.23.4 and 122 ms at `master`.

**The baseline exceeds 50 ms on the same input in one run.** Its worst values
vary from 34 ms to 559 ms between runs, which points at load as well as the
2 GB teardown.

**No stop ever published a partial answer.** Each pin had 3,840 stop trials,
1,920 per engine:

| Pin, engine | `cancelled`, no spans | Complete answer (stop after the last check) | Finished before the stop |
|---|---|---|---|
| v0.23.4, baseline | 1,678 | 24 | 218 |
| v0.23.4, T | 1,728 | 32 | 160 |
| `master`, baseline | 1,502 | 26 | 392 |
| `master`, T | 1,604 | 32 | 284 |

A complete answer is an `available` result with the uncancelled run's span
count: the stop arrived after the refinement's last check. The stop records of
this run carry no span digest (the harness now records one). Neither
refinement can return `available` with a truncated list, because both assign
the finished list only when they return.

### Sanitizers and fuzzing

**Sanitizers.** The whole corpus ran through both pins under
AddressSanitizer and UndefinedBehaviorSanitizer (the `BPR_BUILD_FUZZERS`
graph, `-O1`). It reported nothing, and every result matched the Release
spans.

**Fuzzing.** `fuzz_syntax` and its twins ran for 4 CPU-hours per candidate
(3 workers × 4,800 s, `-max_len=65536`). None found anything:

| Campaign | Runs |
|---|---|
| baseline (seed 1442) | 1,247,857 |
| T at v0.23.4, final candidate (seed 1443) | 7,269,601 |
| T at `master`, final candidate (seed 1443) | 6,902,838 |

An earlier T, before the chain check and the lenient policy, also ran
4 CPU-hours per pin and was clean (587,500 and 574,396 runs).

### Size and build cost

**Size.** A minimal optimized and dead-stripped executable that calls
`refine_cpp_identifiers`, linked once with the shipped refinement and once with
T, the runtime and one grammar:

| Slice | v0.23.4 adds | `master` adds |
|---|---|---|
| arm64 | 3,563,352 bytes (3.40 MiB) | 5,744,184 bytes (5.48 MiB) |
| x86_64 | 3,515,368 bytes (3.35 MiB) | 5,667,000 bytes (5.40 MiB) |

The generated `parser.c` turns its own optimization off. At `-O3` it compiles
in 2.4 s (v0.23.4) or 3.7 s (`master`), and the runtime's `lib.c` in 7.9 s
(arm64).

**Build cost.** A fresh `dev` tree (Debug, built with `-g0` to fit the
station's free disk) took 238 s to build 1,639 steps, at a load average of 20
to 54, using 3,795 CPU-seconds.

In the same tree with the option on, compiling T's extra translation units one
at a time took:

| Pin | Total | `lib.c` | `parser.c` | `scanner.c` | Candidate translation unit |
|---|---|---|---|---|---|
| v0.23.4 | 1.86 s | 0.19 s | 0.56 s | 0.03 s | 0.90 s |
| `master` | 1.84 s | 0.19 s | 0.80 s | 0.04 s | 0.84 s |

So adoption would add at most 0.78% to the clean build's wall time, and 0.05%
of its CPU time. This is an upper bound: the work is counted serially, and the
shipped refinement's own 1.85 s translation unit, which T would replace, is
not subtracted. See `build-cost.json`.

### Licences and what adoption would need

**Files that would ship:**

| File | Licence |
|---|---|
| tree-sitter 0.27.1 `lib/src/lib.c` and the headers it includes | MIT, `LICENSE` SHA-256 `c5cfb430…14fc78` |
| tree-sitter-cpp `src/parser.c`, `src/scanner.c` and `src/tree_sitter/*.h` | MIT, `LICENSE` SHA-256 `2e0110e0…c711d1` |

The provider writes both notices to `_bpr/TREE-SITTER-NOTICES.txt`.

**Archives:**

| Archive | SHA-256 | Bytes |
|---|---|---|
| runtime v0.27.1 | `982cd3d4d9eb7be18c243240a622423fd2e4ecb4bec2cd98832c2f3fa1f0f333` | 1,026,138 |
| tree-sitter-cpp v0.23.4 | `7a2c55afe3028f4105f25762ea58cc16537d1f5a1dcd9cca90410b3cd5d46051` | 1,135,921 |
| tree-sitter-cpp `master` (`c0092228`, 2026-09-17) | `6c6efaf024f5c02fee0403e4e2373346eeadc40828587bcfad9351df2f0a7986` | 1,740,811 |

The runtime digest matches Homebrew's formula for the same tag.

**Adoption would also have needed:**
- entries in `third_party/` and `packaging/dependencies.json`;
- both notices in `NOTICE_FILES` in `package_runtime.py`;
- SPDX packages for the statically compiled runtime and grammar;
- their sources in the separate corresponding-source asset;
- the five dependency checks BPR-1431 passed: `package_runtime_contract`,
  `release_security_audit_contract`, `binary_dependency_audit_contract`,
  `binary_dependency_surface` and `dependency_modernization_contract`.

None of that was done, because T was not adopted.

## Shipping configuration and placement

**AC07: shipping spans are unchanged.** `bench_syntax_span_dump` links only the
shipped classifier. Built at RFC base `530d92eaf` and again on the evaluation
branch, it gave byte-identical status, span counts and digests for all 5,268
C++ corpus files (`results/macos/ac07-*.tsv.gz`).

**AC01: tree-sitter only where the option asks for it.**
- With the option off, the configure log mentions no tree-sitter, and the link
  check finds no tree-sitter target, stage or link input (`link-option-off.json`).
- With it on, the only link edges that name tree-sitter are under
  `tests/syntax_evaluation` (`link-option-on.json`).
- The option refuses a configuration without `BPR_BUILD_QT` and
  `BUILD_TESTING`, so a tests-disabled package cannot enable it.
