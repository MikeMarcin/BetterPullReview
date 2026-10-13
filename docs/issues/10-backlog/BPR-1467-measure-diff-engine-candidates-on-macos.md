---
schema: "implement-rfc.card.v1"
id: "BPR-1467"
title: "Measure the diff-engine candidates on macOS and record each decision"
status: "Backlog"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1466"]
priority: "P2"
platforms: ["mac"]
metadata: {"mandate_revision":"diff-engine-evaluation-r1"}
---

# BPR-1467: Measure the diff-engine candidates on macOS and record each decision

Part of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md). Runs the harness of
[BPR-1466](BPR-1466-opt-in-diff-engine-evaluation-harness.md) on the macOS
station, with the thresholds fixed by the RFC, and commits the report with its
raw results.

## Acceptance

- **AC1:** The committed report shows each gate's pass or fail for every
  candidate-component pairing, with its commands and counts (RFC AC01).
- **AC2:** macOS measurements cover every size class and every item under
  "Corpus and measurement", with the baseline in the same runs and conditions,
  the hardware recorded, and P run against Git 2.39 and the current release
  (RFC AC02, macOS part).
- **AC3:** GitHub hunk agreement and comment-range flips are reported per
  candidate against the recorded provider patches, including the BPR-0625,
  BPR-0719 and lone-CR inputs (RFC AC03).
- **AC4:** For each candidate the report records licence per shipped file,
  linkage, obligations, platform and Git-version dependence and the owner answer
  it needs; a package and boundary check shows no shipped target links a
  candidate (RFC AC04).
- **AC5:** Each component's recorded decision follows mechanically from
  "Decision rules", with "eligible pending Qn" where an open question blocks
  adoption; any deviation is an owner decision (RFC AC05). An intraline candidate
  that passes the mechanical gates gets its own attended-review session (RFC
  AC06); a macOS-only result says that Linux and Windows are still unmeasured.
