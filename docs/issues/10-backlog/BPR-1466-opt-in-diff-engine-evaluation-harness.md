---
schema: "implement-rfc.card.v1"
id: "BPR-1466"
title: "Build the opt-in diff-engine evaluation harness, candidates and corpus"
status: "Backlog"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1465"]
priority: "P2"
platforms: ["mac"]
metadata: {"mandate_revision":"diff-engine-evaluation-r1"}
---

# BPR-1466: Build the opt-in diff-engine evaluation harness, candidates and corpus

Part of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md). Builds on the seam of
[BPR-1465](../30-in-progress/BPR-1465-diff-engine-evaluation-seam.md). The harness and
its candidates live in a non-shipping target that is off by default. The macOS
station builds it first; the Linux and Windows cards run it there.

## Acceptance

- **AC1:** `BPR_DIFF_ENGINE_EVALUATION` is off by default. Candidate sources
  (Git releases for the process and linked shapes, dtl, rapidfuzz-cpp and a
  standard-only diff-match-patch port) are acquired only through the
  verified-archive pattern (SHA-256, size and licence digest, re-extracted under
  the build tree). Nothing enters `third_party/` or
  `packaging/dependencies.json`, and the shipping configuration acquires
  nothing new.
- **AC2:** The harness runs the baseline and every candidate-component pairing of
  the RFC (P with each Git algorithm and the indent heuristic, L xdiff, C O(NP),
  a core histogram, rapidfuzz Indel, diff-match-patch with semantic cleanup, O(NP)
  over scalars and a core sliding post-pass) through the BPR-1465 seam and
  first-party validation, over the RFC's suite list and corpus.
- **AC3:** The corpus holds large-review-v1, the BPR-0719 replica, the benchmark
  sizes, every generated pathological class and the recorded provider patches,
  plus oracle patches recorded on `MikeMarcin/BPR-Test` for the BPR-0625,
  BPR-0719 and lone-CR inputs under the live-GitHub and rate-limit rules.
