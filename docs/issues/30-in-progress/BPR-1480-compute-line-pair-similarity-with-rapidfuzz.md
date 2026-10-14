---
schema: "implement-rfc.card.v1"
id: "BPR-1480"
title: "Compute line-pair similarity with rapidfuzz-cpp"
status: "In Progress"
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

## Built and verified (macOS arm64, 2026-10-13)

Implementation `303f01c2f` and `4654b2181` on `claude/bpr-1480-rapidfuzz`
(based on `bdeaba868`).

**AC1, library.** [`BprRapidFuzzProvider.cmake`](../../../cmake/BprRapidFuzzProvider.cmake)
acquires rapidfuzz-cpp 3.3.4 (archive SHA-256 `a0dd2ef3…9881`, 314,599 bytes;
`LICENSE` digest `529ac8f0…944f` checked; offline honoured; only `rapidfuzz/`
and `LICENSE` re-extracted) as the header-only `better_pull_review_rapidfuzz` /
`BetterPullReview::RapidFuzz`, recorded as
`rapidfuzz-cpp|PINNED_ARCHIVE|BetterPullReview::RapidFuzz`. Only
`better_pull_review_core` links it (privately). `third_party/rapidfuzz-cpp`
holds the MIT licence and provenance; `packaging/dependencies.json`, the
notice list in `packaging/package_runtime.py` (which feeds SPDX and the
corresponding-source inventory) and `docs/development/release-packages.md` name
it. `package_runtime_contract`, `dependency_modernization_contract`,
`release_source_assets_contract`, `release_package_audit_contract`,
`release_security_audit_contract`, `runtime_audit_contract` and
`binary_dependency_audit_contract` pass.

**AC2, engine.** `line_pair_scorer` in `src/core/src/diff_compute.cpp`
replaces `similarity`: `rapidfuzz::CachedIndel<std::uint32_t>`, built once per
left line, gives the Indel distance up to the threshold's cutoff; the score is
`(L + R - distance) * 1000 / (L + R)`, floored, which is the old
`2 * LCS * 1000 / (L + R)`. A pair below the threshold scores 0 (pairing reads
no score under it). The matrix budget is charged `(L + 1) x (R + 1)` per scored
pair as before, and the step meter is ticked in the same increments the
character table ticked (`charge_large_similarity_table` for a large one), so
limits, polls, pauses and phase starts are unchanged. `architecture_boundaries`
refuses `rapidfuzz::`, `rapidfuzz/` or `RAPIDFUZZ_` outside that file, with a
contract test.

- `tool_diff_baseline_digest` (built against each Release core-only tree):
  generated matrix 102,572 lines, SHA-256 `8efc6960…a052`, and matrix plus the
  86-pair evaluation corpus, 1,162 cases, 238,130 lines (38,146 pair records),
  SHA-256 `4c8f0084…a959`, byte-identical before (`bdeaba868`) and after.
- Pairing phase (`refine` column, now in `core_diff_compute_benchmark`; Release,
  9 repetitions, median ms, before -> after): 2K lines 30 edits 0.21 -> 0.02,
  2K 300 edits 3.33 -> 0.21, 10K 30 edits 1.05 -> 0.10, 10K 300 edits
  15.80 -> 0.97; a repeat run agreed. Whole one-call 10K/300: 49.2 -> 34.9 ms.
  The 40K rows never enter refine in either core.
- `core-only` (Debug): 269 of 275 pass, including every `core_diff_*`,
  `core_review_comment_range`, `core_review_cycle_*`, `core_dependency_boundary`,
  `core_boundary_contract`, `architecture_boundaries` and
  `architecture_boundary_contract`. The six others fail on the base too and touch
  no file of this card: `documentation_structure`,
  `documentation_issue_graph_contract` and `issue_id_allocation_contract`
  (worktree lacks `.agents/`), `contained_temporary_usage_contract` and
  `msvc_secure_crt_contract` (evaluation-harness and adapter test files), and
  `coding_format_check` (`tests/evaluation/diff_engine/baseline_digest.cpp`).
  Changed files are clang-format 22 clean; clang-tidy adds no finding.
