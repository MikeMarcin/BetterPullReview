---
schema: "implement-rfc.card.v1"
id: "BPR-1480"
title: "Compute line-pair similarity with rapidfuzz-cpp"
status: "Ready"
rfcs: ["RFC-BPR-0082"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r2","source":"RFC-BPR-0082 line-pair similarity decision: adopt (owner Q2 Accept, 2026-10-13)"}
---

# BPR-1480: Compute line-pair similarity with rapidfuzz-cpp

The adoption follow-up of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md) for line-pair similarity. On macOS the
evaluation candidate `c-rapidfuzz` gave pairing decisions bit-identical to the
engine's on all 86 corpus comparisons with a pairing phase 0.10x the engine's
on M and L inputs, and passed every gate
([report](../../verification/RFC-BPR-0082.md)). The owner approved
rapidfuzz-cpp for the core on 2026-10-13 (Q2). This replaces the engine's
per-pair character LCS in `similarity`/`secondary_pairs`
(`src/core/src/diff_compute.cpp`) with rapidfuzz-cpp's Indel distance, scored
in integers exactly as today (2·LCS·1000/(L+R), floored), and keeps the matrix
budget charged as today so a limit leaves the same replacements unpaired.

## Acceptance

- **AC1:** rapidfuzz-cpp 3.3.4 is acquired by a pinned verified-archive
  provider (SHA-256, size, licence digest) as `BetterPullReview::RapidFuzz`,
  linked only by `better_pull_review_core`, with provenance, licence,
  `packaging/dependencies.json`, release notices, SPDX and corresponding source
  updated as BPR-1450 and BPR-1451 did, and the package audits passing.
- **AC2:** The engine scores line pairs with rapidfuzz-cpp's Indel distance;
  `architecture_boundaries` keeps rapidfuzz symbols to the one core file that
  does so. `tool_diff_baseline_digest` over its generated matrix and the
  evaluation corpus is byte-identical before and after (outcomes, identities and
  counters), the diff suites and `core-only` pass, and the pairing phase is
  measured faster on the benchmark.
