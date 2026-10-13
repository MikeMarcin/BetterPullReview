---
schema: "implement-rfc.card.v1"
id: "BPR-1442"
title: "Build the parser evaluation harness and measure tree-sitter-cpp on macOS"
status: "In Progress"
rfcs: ["RFC-BPR-0081"]
depends_on: ["BPR-1440", "BPR-1441"]
priority: "P2"
platforms: ["mac"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
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
