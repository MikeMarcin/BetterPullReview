---
schema: "implement-rfc.card.v1"
id: "BPR-1466"
title: "Build the opt-in diff-engine evaluation harness, candidates and corpus"
status: "Done"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1465"]
priority: "P2"
platforms: ["mac"]
metadata: {"mandate_revision":"diff-engine-evaluation-r1"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"28360f075","evidence":{"kind":"observation","detail":"BPR_DIFF_ENGINE_EVALUATION defaults OFF (core-only CMakeCache: BPR_DIFF_ENGINE_EVALUATION:BOOL=OFF; its _bpr holds only product dependencies, no diff-evaluation stage). Git 2.39.5 and 2.56.0, dtl 1.21, rapidfuzz-cpp 3.3.4 and the diff-match-patch port are acquired only by verified archive (SHA-256, size, licence digest) under the build tree's _bpr/diff-evaluation. The evaluation adds nothing to third_party/ or packaging/dependencies.json; the only changes there since the RFC base are BPR-1480's product adoption of rapidfuzz-cpp."}},{"criterion":"AC2","outcome":"pass","revision":"28360f075","evidence":{"kind":"command","detail":"The harness ran the baseline and all 38 candidate-component pairings (P with 4 algorithms x indent heuristic x Git 2.39.5/2.56.0/installed, L xdiff with 4 algorithms x heuristic as its own shared library, C O(NP), C histogram, C slide, rapidfuzz Indel, diff-match-patch, O(NP) over scalars) through the BPR-1465 seam and first-party validation over the 86-comparison corpus (measure, phases, intraline, stops, overhead, determinism, rss) and over the RFC's full suite list (run_suites.py: 23 tests including the diff fuzz corpus replay, for every candidate; the last 12 P variants in the supplement run) with inventories for core_diff_compute and application_diff_host. Results: docs/verification/RFC-BPR-0082.md, RFC-BPR-0082/macOS-results.md and macOS-L-results.md."}},{"criterion":"AC3","outcome":"pass","revision":"28360f075","evidence":{"kind":"artifact","detail":"Corpus (prepare_corpus.py, 86 comparisons: real 56, pathological 18, benchmark L 5, M 3, S 4) holds large-review-v1 (bpr0625-comparison, bpr0625-generated-table), the BPR-0719 replica, the BPR-1366 benchmark sizes, every generated pathological class including CRLF and lone CR, and UIforETW#74's recorded patches; 78 comparisons carry a GitHub oracle, including bpr0625-comparison, bpr0625-generated-table, bpr0719-replica, patho-crlf and patho-lone-cr recorded on MikeMarcin/BPR-Test (tests/fixtures/github/diff-oracle-v1)."}}]
---

# BPR-1466: Build the opt-in diff-engine evaluation harness, candidates and corpus

Part of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md). Builds on the seam of
[BPR-1465](BPR-1465-diff-engine-evaluation-seam.md). The harness and
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
