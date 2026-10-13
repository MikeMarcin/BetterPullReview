---
schema: "implement-rfc.card.v1"
id: "BPR-1469"
title: "Measure the diff-engine candidates on Windows"
status: "Backlog"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1467"]
priority: "P2"
platforms: ["win"]
metadata: {"mandate_revision":"diff-engine-evaluation-r1"}
---

# BPR-1469: Measure the diff-engine candidates on Windows

Part of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md). Runs the harness of
[BPR-1466](BPR-1466-opt-in-diff-engine-evaluation-harness.md) on the Windows
station with the thresholds unchanged. The process candidate's Windows launch
uses the Git adapter's job and handle restriction.

## Acceptance

- **AC1:** Windows results for every size class and every measurement item,
  including P against Git 2.39 and the current release and stop-to-return with
  process kill and reap, are committed beside the macOS results, with the
  baseline in the same runs and the hardware recorded.
- **AC2:** Cross-platform determinism is checked against the macOS outputs, and
  the report's gate table and decisions are updated with the Windows results.
