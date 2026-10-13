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
[BPR-1442](BPR-1442-measure-tree-sitter-cpp-on-macos.md) commits, on the Windows
x64 station, with the thresholds unchanged.

## Acceptance

- **AC1:** Windows x64 results for every size class and every item under the
  RFC's "Measurements" that applies to Windows are committed beside the macOS
  results, with the baseline in the same runs and the hardware recorded.
  libFuzzer does not run on Windows; the corpus still runs.
- **AC2:** The Windows executable size added per candidate, the clean build cost
  and the span comparison against the macOS results for determinism are recorded,
  and the report's gate table is updated with the Windows results.
