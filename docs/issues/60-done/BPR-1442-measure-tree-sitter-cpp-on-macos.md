---
schema: "implement-rfc.card.v1"
id: "BPR-1442"
title: "Build the parser evaluation harness and measure tree-sitter-cpp on macOS"
status: "Done"
rfcs: ["RFC-BPR-0081"]
depends_on: ["BPR-1440", "BPR-1441"]
priority: "P2"
platforms: ["mac"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"02da27cbd","evidence":{"kind":"artifact","detail":"Option off: no tree-sitter in the configure log, link-option-off.json ok; option on: tree-sitter link edges only under tests/syntax_evaluation, adapter archive has no ts_ symbols; archives verified by SHA-256, size and licence digest.","path":"docs/issues/60-done/BPR-1442-measure-tree-sitter-cpp-on-macos.md"}},{"criterion":"AC2","outcome":"pass","revision":"02da27cbd","evidence":{"kind":"observation","detail":"Harness runs baseline and both pins over the 5,270-file corpus through the unchanged public classifier; contract twins run with the candidate compiled in (contract twin fails on enumerator attributes, others pass).","path":"docs/issues/60-done/BPR-1442-measure-tree-sitter-cpp-on-macos.md"}},{"criterion":"AC3","outcome":"pass","revision":"02da27cbd","evidence":{"kind":"artifact","detail":"Per-category FP/FN for baseline and each pin (summary.json); H1-H8 confirmed on the baseline, T refutes H1-H3 and H5-H7.","path":"docs/issues/60-done/BPR-1442-measure-tree-sitter-cpp-on-macos.md"}},{"criterion":"AC4","outcome":"pass","revision":"02da27cbd","evidence":{"kind":"artifact","detail":"macOS timing, heap, RSS, stop latency, composer, large-review syntax phases, ASan/UBSan corpus, 4 CPU-hour fuzz per candidate, deep nesting, 8 MiB inputs and determinism, baseline in the same runs; results in tests/syntax_evaluation/results/macos.","path":"docs/issues/60-done/BPR-1442-measure-tree-sitter-cpp-on-macos.md"}},{"criterion":"AC5","outcome":"pass","revision":"02da27cbd","evidence":{"kind":"artifact","detail":"MIT licences and notice digests, archive digests, +3.40/+5.48 MiB arm64 and +3.35/+5.40 MiB x86_64, clean dev build at most +0.78%, adoption's notice/SPDX/corresponding-source/dependency-check needs recorded in the report.","path":"docs/issues/60-done/BPR-1442-measure-tree-sitter-cpp-on-macos.md"}},{"criterion":"AC6","outcome":"pass","revision":"02da27cbd","evidence":{"kind":"observation","detail":"Decision rules applied: T rejected (Contract, Bounded work, Memory) at both pins, outcome extend with follow-up BPR-1474; Windows and Linux named as owed and not claimed; core-only and both architecture-boundary checks pass apart from unrelated rows recorded on the card.","path":"docs/issues/60-done/BPR-1442-measure-tree-sitter-cpp-on-macos.md"}}]
---

# BPR-1442: Build the parser evaluation harness and measure tree-sitter-cpp on macOS

Part of [RFC-BPR-0081](../../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md), milestone `macos-evaluation`. Thresholds are the RFC's, fixed
before measurement. Q1 is open, so if only the `master` pin passes the outcome
is "eligible pending Q1" and nothing is adopted.

## Acceptance

- **AC1:** `BPR_BUILD_SYNTAX_EVALUATION` is off by default and valid only with
  `BPR_BUILD_QT`. With it off, no target acquires or links tree-sitter, as the
  configure log and a link-dependency check show. With it on, the tree-sitter
  v0.27.1 runtime and the tree-sitter-cpp v0.23.4 and `master` archives are
  acquired in the RE2 provider's pattern and verified by SHA-256, size and
  licence digest.
- **AC2:** The harness runs the baseline and each tree-sitter pin over the whole
  corpus through the unchanged public classifier, with the same grammar spans,
  painting, publication, limits and status rules. The RFC's contract rows run
  with each candidate compiled in.
- **AC3:** For every run, the report gives false positives and false negatives
  per category against the committed labels, and marks H1 to H8 confirmed or
  refuted on the baseline and on each pin.
- **AC4:** The macOS measurements cover every size class and every item under the
  RFC's "Measurements", with the baseline in the same runs, including sanitizer,
  fuzz, deep-nesting and 8 MiB results for the baseline and each pin.
- **AC5:** The report records each shipped file's licence, the notice, SPDX and
  corresponding-source changes adoption would need, the archive digests, the
  size added per macOS slice and the build cost.
- **AC6:** The report applies the decision rules mechanically to what macOS can
  decide, names the gates still waiting on the Windows and Linux stations, and
  claims no unmeasured platform. `core-only`, both architecture-boundary checks
  and the option-off check pass.

## Evidence

Candidate `02da27cbd` on macOS (Apple M4 Pro). Report:
[docs/verification/RFC-BPR-0081.md](../../verification/RFC-BPR-0081.md);
raw results: [tests/syntax_evaluation/results/macos/](../../../tests/syntax_evaluation/results/macos/README.md).

- **AC1, option and providers.** The option is off by default and refused
  without Qt and tests. With it off, the configure log has no tree-sitter
  mention, and `check_link_dependencies.py` finds no target, stage or link
  input (`link-option-off.json`). With it on, the only tree-sitter link edges
  are under `tests/syntax_evaluation`, and the adapter archive has no `ts_`
  symbol. `cmake/BprTreeSitterProvider.cmake` verifies the runtime v0.27.1 and
  the cpp v0.23.4 and `c0092228` archives by SHA-256, size and licence digest.
  The runtime digest matches Homebrew's formula.
- **AC2, harness and twins.** The harness runs the baseline and each pin over
  all 5,270 corpus files through the unchanged `packaged_syntax_classifier`.
  Of the contract twins, `syntax_classification_contract@ts-*` fail (no
  enumerator attributes in the grammar); the cancellation, pane-composition and
  appearance twins pass.
- **AC3, accuracy and hypotheses.** False positives and negatives per category
  are reported for every run: baseline 136/3,576, T 77/8,405 (v0.23.4) and
  80/8,355 (`master`). H1-H8 are confirmed on the baseline. T refutes H1-H3
  and H5-H7, and confirms H4 and H8.
- **AC4, macOS measurements.** Every size class and every RFC measurement item
  is covered, with the baseline in the same runs:
  - timing (3 rounds) and stop latency (3,840 trials per pin, no partial
    publication);
  - peak heap and peak resident memory;
  - composer replay;
  - `syntax_first_file` and `syntax_navigation` (0.96-1.02×);
  - ASan/UBSan over the corpus, clean and identical to Release;
  - 4 CPU-hours of fuzzing per candidate, clean;
  - deep nesting and 8 MiB inputs;
  - determinism across rounds and builds.

  The station was loaded by other agents, and the load is recorded.
- **AC5, licences and cost.** MIT notices and digests are recorded. Size added:
  +3.40/+5.48 MiB on arm64 and +3.35/+5.40 MiB on x86_64. Clean `dev` build:
  at most +0.78%. The SPDX, notice, corresponding-source and dependency-check
  changes adoption would need are recorded.
- **AC6, outcome.** Rejected on Contract, Bounded work and Memory at both
  pins, so the outcome is **extend**:
  [BPR-1474](../30-in-progress/BPR-1474-fix-the-confirmed-cpp-identifier-gap-classes.md).
  Windows and Linux are named as owed (BPR-1443, BPR-1444) and are not claimed.
  `core-only` passes except five rows, all from other work: three
  documentation/issue-graph rows (tree drift), `msvc_secure_crt_contract`
  (`conversation_cards_view_tests.cpp`) and `contained_temporary_usage_contract`.
  The last was this card's own two tool uses, now fixed and passing.
  `architecture_boundaries`, `architecture_boundary_contract`,
  `core_dependency_boundary` and `core_boundary_contract` pass.
