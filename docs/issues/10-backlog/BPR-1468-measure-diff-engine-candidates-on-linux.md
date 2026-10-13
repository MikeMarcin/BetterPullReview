---
schema: "implement-rfc.card.v1"
id: "BPR-1468"
title: "Measure the diff-engine candidates on Linux"
status: "Backlog"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1467"]
priority: "P2"
platforms: ["linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r1"}
---

# BPR-1468: Measure the diff-engine candidates on Linux

Part of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md). Runs the harness of
[BPR-1466](../20-ready/BPR-1466-opt-in-diff-engine-evaluation-harness.md) on the Linux
station with the thresholds unchanged, after the macOS report of
[BPR-1467](BPR-1467-measure-diff-engine-candidates-on-macos.md).

## Acceptance

- **AC1:** Linux results for every size class and every measurement item,
  including P against Git 2.39 and the current release and stop-to-return with
  process kill and reap, are committed beside the macOS results, with the
  baseline in the same runs and the hardware recorded.
- **AC2:** Cross-platform determinism is checked against the macOS outputs, and
  the report's gate table and decisions are updated with the Linux results.
