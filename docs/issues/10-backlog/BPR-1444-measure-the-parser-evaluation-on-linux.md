---
schema: "implement-rfc.card.v1"
id: "BPR-1444"
title: "Measure the C++ parser evaluation on Linux x64 and run its Linux fuzz campaign"
status: "Backlog"
rfcs: ["RFC-BPR-0081"]
depends_on: ["BPR-1442"]
priority: "P2"
platforms: ["linux"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
---

# BPR-1444: Measure the C++ parser evaluation on Linux x64 and run its Linux fuzz campaign

Part of [RFC-BPR-0081](../../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md), milestone `linux-measurement`. Runs the harness that
[BPR-1442](BPR-1442-measure-tree-sitter-cpp-on-macos.md) commits, on the Linux x64
station, with the thresholds unchanged.

## Acceptance

- **AC1:** Linux x64 results for every size class and every item under the RFC's
  "Measurements" are committed beside the macOS results, with the baseline in the
  same runs and the hardware recorded.
- **AC2:** The syntax fuzz target runs for 4 CPU-hours per candidate on Linux,
  and the corpus runs under AddressSanitizer and UndefinedBehaviorSanitizer.
- **AC3:** The Linux executable size added per candidate, the clean build cost
  and the span comparison against the macOS results for determinism are recorded,
  and the report's gate table is updated with the Linux results.
