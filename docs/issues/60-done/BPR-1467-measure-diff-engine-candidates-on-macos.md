---
schema: "implement-rfc.card.v1"
id: "BPR-1467"
title: "Measure the diff-engine candidates on macOS and record each decision"
status: "Done"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1466"]
priority: "P2"
platforms: ["mac"]
metadata: {"mandate_revision":"diff-engine-evaluation-r1"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"28360f075","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0082.md 'Gates, with commands and counts (macOS)' and the generated gate tables in RFC-BPR-0082/macOS-results.md (first pass, regenerated with inventories and every candidate's suites) and macOS-L-results.md (L re-measurement) show correctness, determinism, interruption, placement and licence pass/fail for every candidate-component pairing with the commands (run_measurements.py, run_suites.py, run_inventories.py, make_report.py) and counts."}},{"criterion":"AC2","outcome":"pass","revision":"28360f075","evidence":{"kind":"artifact","detail":"macOS (Apple M4 Pro, 14 cores, 48 GiB, macOS 26.6.2, Apple clang 21, Release) measurements cover S, M, L, pathological and real classes: time to result, peak memory, stop-to-return including kill and reap, per-file process overhead, approximation and lost intraline, edit size against minimal, GitHub hunk agreement and comment-range flips; the baseline ran in the same runs (first pass and again in the L re-measurement); P ran against Git 2.39.5, 2.56.0 and the installed 2.50.1. Load averages recorded beside every sample."}},{"criterion":"AC3","outcome":"pass","revision":"28360f075","evidence":{"kind":"artifact","detail":"GitHub hunk agreement (GitHub-join, Git-join and BPR comment hunks) and comment-range flips per candidate against diff-oracle-v1 (78 files, including bpr0625-comparison, bpr0719-replica and patho-lone-cr), refreshed after BPR-1482 in the report's 'Comment hunks after BPR-1482' table."}},{"criterion":"AC4","outcome":"pass","revision":"28360f075","evidence":{"kind":"artifact","detail":"Report 'Licence, linkage and obligations' records licence per shipped file, linkage, obligations, platform and Git-version dependence and the owner answer for P, L (own shared library, Q1), dtl, rapidfuzz-cpp, diff-match-patch and first-party code. Package and boundary check: a default configuration has BPR_DIFF_ENGINE_EVALUATION OFF and no evaluation stage or target; core_dependency_boundary, core_boundary_contract, architecture_boundaries, architecture_boundary_contract and git_protocol_boundary pass on the integrated build."}},{"criterion":"AC5","outcome":"pass","revision":"28360f075","evidence":{"kind":"artifact","detail":"Each component's decision follows mechanically from make_report.py's fixed rules; deviations are owner decisions recorded with before/after text: Q1 and Q2, the L speed waiver, its extension to the approximation clause, and restart-only cancellation (docs/rfcs/assets/diff-engine-evaluation-v1/). Outcomes: primary matcher and hunk sliding adopt l-myers-indent (BPR-1485); pairing adopts rapidfuzz-cpp (BPR-1480, done); intraline dtl got its own attended-review session (3dcb3e5660345ff127118964cc309faa) and the owner's Accept (decision 0552741d-9e4e-4c8b-8a7b-350e9d0326a8), adopted through BPR-1481. The report states that Linux and Windows are unmeasured (BPR-1468, BPR-1469)."}}]
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
