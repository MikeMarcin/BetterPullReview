---
schema: "implement-rfc.card.v1"
id: "BPR-1481"
title: "Compute intraline ranges with dtl after the owner's render review"
status: "Done"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1467"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r2","source":"RFC-BPR-0082 intraline decision: eligible pending AC06 (owner Q2 Accept for dtl, per-line use only)"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a66d996df","evidence":{"kind":"observation","detail":"Owner Q2 Accept (2026-10-13, dtl for per-line intraline comparison only) and AC06 Accept of the dtl render set (session 3dcb3e5660345ff127118964cc309faa, decision 0552741d-9e4e-4c8b-8a7b-350e9d0326a8, 2026-10-14T04:16:42Z) recorded in a66d996df, an ancestor of every BPR-1481 production commit.","path":"docs/issues/60-done/BPR-1481-compute-intraline-ranges-with-dtl.md"}},{"criterion":"AC2","outcome":"pass","revision":"2f3d76525, 162db34a2, 4427d5947 (claude/bpr-1481-dtl); merged with claude/bpr-1485-xdiff at 22cdc35c6","evidence":{"kind":"command","detail":"BprDtlProvider.cmake acquires dtl 1.21 by pinned verified archive (SHA-256 90ed2dbf...3dbd, 26,321 bytes, COPYING digest 73311650...7fec; the evaluation's pin; offline honoured) as header-only BetterPullReview::Dtl, linked privately by better_pull_review_core only. third_party/dtl COPYING (BSD-3-Clause) and PROVENANCE, packaging/dependencies.json, package_runtime.py notices (SPDX, corresponding source), release-packages.md. architecture_boundaries refuses dtl::, dtl/ or DTL_ outside src/core/src/diff_intraline.cpp (contract test added), whose one entry point takes a single line pair's scalars, so dtl is never a whole-file line matcher. package_runtime, dependency_modernization, release_source_assets, release_package_audit, release_security_audit, runtime_audit and binary_dependency_audit contracts pass in core-only and in a dev configuration.","path":"docs/issues/60-done/BPR-1481-compute-intraline-ranges-with-dtl.md"}},{"criterion":"AC3","outcome":"pass","revision":"2f3d76525, 162db34a2, 4427d5947 (claude/bpr-1481-dtl); merged with claude/bpr-1485-xdiff at 22cdc35c6","evidence":{"kind":"command","detail":"intraline_ranges takes dtl's common scalars (detail::intraline_common_scalars), comparisons counted on the step meter in batches of 4,096, matrix budget not charged; stops caught in the comparator and rethrown after dtl; grids at dtl's 2,000,000-point restart bound keep the budgeted table; every common scalar checked. 86 evaluation-corpus comparisons through product_file_diff: 86 published and valid, 25,420 spans; vs l-myers-indent lost emphasis 11,706 -> 858 (all patho-crlf line-ending-only pairs), approximate 9 -> 8, matrix cells 176,974,001 -> 159,739,202; stated in docs/verification/RFC-BPR-0082.md and tested by new core_diff_intraline. core-only 270/276 with the six failures pre-existing and outside this card (worktree .agents, tests/hash_evaluation tempfile, conversation_cards getenv, BPR-1485 format fixed by d978cad54); after the merge the 26 diff, format, boundary and audit tests pass.","path":"docs/issues/60-done/BPR-1481-compute-intraline-ranges-with-dtl.md"}}]
---

# BPR-1481: Compute intraline ranges with dtl after the owner's render review

The adoption follow-up of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md) for intraline ranges. The candidate
`c-onp-intraline` (dtl 1.21, O(NP) over a pair's Unicode scalars) produced
valid spans on the whole corpus with a p95 per-pair time of 0.0227 ms against
the engine's 0.0250 ms and passed every macOS gate. The owner approved dtl for
the core on 2026-10-13 for per-line intraline comparison only. Adoption waits
on the owner's Accept of the render set (RFC AC06, prepared under
[BPR-1467](BPR-1467-measure-diff-engine-candidates-on-macos.md)); a Reject
closes this card without change.

## Acceptance

- **AC1:** The owner's Accept of the dtl render set is recorded before any
  production change.
- **AC2:** dtl 1.21 is acquired by a pinned verified-archive provider as
  `BetterPullReview::Dtl`, linked only by `better_pull_review_core`, with
  provenance, licence, `dependencies.json`, notices, SPDX and corresponding
  source, and the package audits passing. `architecture_boundaries` keeps dtl
  to the one core file that computes intraline ranges; dtl is never used as a
  whole-file line matcher.
- **AC3:** Intraline ranges come from dtl's common scalars, counted through the
  engine's step meter; every span validates on the evaluation corpus, the diff
  suites and `core-only` pass, and the report's change in lost intraline
  emphasis and matrix accounting is stated and tested.

## Built and verified (macOS arm64, 2026-10-13)

Implementation `2f3d76525`, `162db34a2` and `4427d5947` on
`claude/bpr-1481-dtl`, built on `claude/bpr-1485-xdiff` (merged through
`22cdc35c6`) and `main` at `9df29e98c`.

**AC1, approval.** The owner's Q2 Accept (2026-10-13, dtl for per-line
intraline comparison only) and AC06 Accept of the dtl render set (session
`3dcb3e5660345ff127118964cc309faa`, decision
`0552741d-9e4e-4c8b-8a7b-350e9d0326a8`, 2026-10-14T04:16:42Z) are recorded in
`a66d996df`, an ancestor of every implementation commit.

**AC2, library.** [`BprDtlProvider.cmake`](../../../cmake/BprDtlProvider.cmake)
acquires dtl 1.21 (archive SHA-256 `90ed2dbf…3dbd`, 26,321 bytes, the
evaluation's pin; `COPYING` digest `73311650…7fec` checked; offline honoured;
only `dtl/` and `COPYING` re-extracted) as the header-only
`better_pull_review_dtl` / `BetterPullReview::Dtl`, recorded as
`dtl|PINNED_ARCHIVE|BetterPullReview::Dtl`. Only `better_pull_review_core`
links it (privately). `third_party/dtl` holds the BSD-3-Clause `COPYING` and
provenance; `packaging/dependencies.json`, the notice list in
`packaging/package_runtime.py` (SPDX and corresponding source) and
`docs/development/release-packages.md` name it. `architecture_boundaries`
refuses `dtl::`, `dtl/` or `DTL_` outside `src/core/src/diff_intraline.cpp`,
with a contract test; that file's one entry point takes one line pair's
scalars, so dtl is never a whole-file line matcher. `package_runtime_contract`,
`dependency_modernization_contract`, `release_source_assets_contract`,
`release_package_audit_contract`, `release_security_audit_contract`,
`runtime_audit_contract` and `binary_dependency_audit_contract` pass in
`core-only` and in a `dev` configuration.

**AC3, engine.** `intraline_ranges` in `diff_compute.cpp` takes its matches
from `detail::intraline_common_scalars` (`diff_intraline.cpp`): `dtl::Diff`
over the pair's scalars, each comparison a step of the engine's meter (batches
of 4,096), the matrix budget not charged. A stop is caught in the comparator,
which then compares everything equal so dtl ends in one pass, and is thrown
again after it, so no stop unwinds through dtl's `new[]`. A pair whose
(left + 1) x (right + 1) grid reaches dtl's 2,000,000-point restart bound keeps
the budgeted table, and every common scalar is checked before use.

- Corpus: the 86 evaluation comparisons through `product_file_diff` (a scratch
  driver, `core-only` Debug, at `4427d5947` and again at `22cdc35c6`, identical):
  86 published and valid, 25,420 intraline spans, 18,645 similar pairs.
  Against `l-myers-indent` (same xdiff alignment, table intraline):
  pairs without emphasis 11,706 -> 858, approximate 9 -> 8, matrix cells
  176,974,001 -> 159,739,202. The 858 are `patho-crlf`'s 286 line-ending-only
  pairs under three options. Stated in
  [the report](../../verification/RFC-BPR-0082.md#intraline-ranges-after-bpr-1481).
- `core_diff_intraline` (new): shortest-edit ranges, a budget pairing exactly
  fills keeps every pair's emphasis and `MatrixCellsConsumed` equals pairing's,
  a 1,500-scalar pair keeps the charged table and degrades past it, and 15
  stops inside dtl abandon within one batch and recompute identically.
- `core-only` (Debug, at `4427d5947`): 270 of 276 pass, including every
  `core_diff_*`, `application_diff_host`, the boundary and package audits. The
  six others fail on the base and touch no file of this card:
  `documentation_structure`, `documentation_issue_graph_contract` and
  `issue_id_allocation_contract` (worktree lacks `.agents/`),
  `contained_temporary_usage_contract` (`tests/hash_evaluation`),
  `msvc_secure_crt_contract` (`conversation_cards_view_tests.cpp`) and
  `coding_format_check` (BPR-1485 files, fixed by its `d978cad54`). After
  merging that, the 26 diff, format, boundary and audit tests pass again.
