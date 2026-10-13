---
schema: "implement-rfc.card.v1"
id: "BPR-1443"
title: "Measure the C++ parser evaluation on Windows x64"
status: "Backlog"
rfcs: ["RFC-BPR-0081"]
depends_on: ["BPR-1442"]
priority: "P2"
platforms: ["win"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
---

# BPR-1443: Measure the C++ parser evaluation on Windows x64

Part of [RFC-BPR-0081](../../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md), milestone `windows-measurement`. Runs the harness that
[BPR-1442](../30-in-progress/BPR-1442-measure-tree-sitter-cpp-on-macos.md) commits, on the Windows
x64 station, with the thresholds unchanged.

## Acceptance

- **AC1:** Windows x64 results for every size class and every item under the
  RFC's "Measurements" that applies to Windows are committed beside the macOS
  results, with the baseline in the same runs and the hardware recorded.
  libFuzzer does not run on Windows; the corpus still runs.
- **AC2:** The Windows executable size added per candidate, the clean build cost
  and the span comparison against the macOS results for determinism are recorded,
  and the report's gate table is updated with the Windows results.

## How to run it

The macOS lane committed everything this station needs: the option, the
provider, the harness and its twins, the tools, and the corpus manifest and
labels. The [report](../../verification/RFC-BPR-0081.md) and
[`tests/syntax_evaluation/README.md`](../../../tests/syntax_evaluation/README.md)
give the commands. In short:

1. Configure a Release tree with tests and `-DBPR_BUILD_SYNTAX_EVALUATION=ON`,
   then build the `syntax-evaluation` target. The provider downloads and
   verifies the three pinned archives unless they are already in the CPM cache.
2. Materialize the corpus with `tools/materialize_corpus.py` into a scratch
   directory outside the checkout. The third-party archives come from the CPM
   cache.
3. Run `tools/run_evaluation.py` (3 rounds, 20 stop trials), then
   `tools/score_evaluation.py`, `tools/measure_size.py` and
   `tools/check_link_dependencies.py`. Also run the `@ts-*` ctest rows and the
   interleaved `--prepared-open` rounds of the large-review benchmark and its
   two twins.
4. Commit the results beside `tests/syntax_evaluation/results/macos/` and
   compare the span digests with the macOS metrics files for determinism.

The outcome is already fixed: T fails the Contract gate in its grammar, and
fails Bounded work and Memory on macOS. This station's numbers complete AC03;
they cannot change the outcome.

Windows notes:
- The twins of `application_cpp_identifier_appearance` and
  `application_large_review_benchmark` are registered without
  `WINDOWS_UNCONTAINED`. Mark them as the originals are, and add their
  `WINDOWS_UNCONTAINED_TESTS` entries, in the same commit.
- `measure_size.py` is written for macOS slices (`size -m`, `-arch`). On
  Windows, compare the linked twins' executable sizes instead.
- libFuzzer is not built on Windows.
