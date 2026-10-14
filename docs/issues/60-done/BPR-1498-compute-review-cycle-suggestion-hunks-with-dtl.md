---
schema: "implement-rfc.card.v1"
id: "BPR-1498"
title: "Compute Review Cycle suggestion hunks with dtl"
status: "Done"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1481"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r5","source":"Owner Accept 2026-10-13: extend dtl's core approval from per-line intraline to the bounded Review Cycle suggestion-hunk matcher (512-edit cap kept)"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a41035268, 7a4468007","evidence":{"kind":"command","detail":"RFC-BPR-0082 owner answer section (mandate r5) with Q2 extension marked and before/after record asset; root and core AGENTS.md, VerifyCoreBoundary.cmake, BprDtlProvider.cmake, BprDependencies.cmake, system-architecture.md, build-and-test.md, release-packages.md and dependencies.json name exactly the two uses. architecture_boundaries allows dtl in exactly diff_intraline.cpp and review_cycle_suggestion.cpp; test_dtl_has_exactly_two_owners passes (and fails under the old single-owner rule); architecture_boundaries, architecture_boundary_contract and core_boundary_contract pass."}},{"criterion":"AC2","outcome":"pass","revision":"a41035268","evidence":{"kind":"observation","detail":"matched_lines runs dtl::Diff synchronously over the trimmed middle; |left-right| > 512 returns nothing before dtl; a band-stop comparator ends dtl at its first out-of-band comparison and finished edits over 512 return nothing; static_assert bounds path points at 264,196 < 2,000,000 (onHuge never enabled); common lines checked before use. Integrator argument from dtl 1.21's compose/snake recorded on the card: round band+1 either compares out of band (stop) or finishes."}},{"criterion":"AC3","outcome":"pass","revision":"a41035268","evidence":{"kind":"command","detail":"No suggestion expectation changed; core_review_cycle_suggestion passes with the new alignment_is_dtl_bounded_at_512_edits (512/513 edits both ways, old tie-break, 50,000-line file, 2,000-line rewrite). Worker: 60,000 random inputs identical to the old Myers. core-only after merging main at f5bea5271: 269 of 272 non-perf tests pass (3 failures are the worktree's missing .agents links), plus architecture_boundary_contract, dependency_modernization_contract, package and release audit contracts and coding_format_check pass; docs gate OK; changed-line tidy clean."}}]
---

# BPR-1498: Compute Review Cycle suggestion hunks with dtl

The owner Accepted on 2026-10-13 replacing the bounded first-party Myers in
`matched_lines` (`src/core/src/review_cycle_suggestion.cpp`, capped at 512
edits) with dtl, extending dtl's core approval beyond per-line intraline
comparison to this one bounded use. dtl stays out of whole-file line matching.

## Acceptance

- **AC1:** The owner's answer is recorded in RFC-BPR-0082 by the amendment
  convention, and both `AGENTS.md` files, `cmake/VerifyCoreBoundary.cmake`,
  the system architecture and the architecture-boundary rule name exactly the
  two approved dtl uses; the rule allows dtl in exactly those two files, with
  a contract test.
- **AC2:** `matched_lines` takes its matches from dtl's minimal edit script,
  synchronously in core, with inputs bounded as before, the 512-edit cap kept
  and dtl's safe size limits respected as `diff_intraline.cpp` does.
- **AC3:** Every existing Review Cycle suggestion test returns identical
  results; `core-only`'s Review Cycle and diff suites and both
  architecture-boundary checks pass.

## Built and verified (macOS arm64, 2026-10-13)

Implementation `a41035268` and `7a4468007` on `claude/bpr-1498`, from `main`
at `8ddd55bf0`, merged with `main` through `f5bea5271`.

**AC1, approval and boundary.** RFC-BPR-0082 records the owner's Accept under
mandate `diff-engine-evaluation-r5` ("Owner answer on dtl for Review Cycle
suggestion hunks"), with Q2's extension marked and the
[record](../../rfcs/assets/diff-engine-evaluation-v1/owner-answer-dtl-suggestion-hunks-2026-10-13.md)
keeping its text before and after. Root and core `AGENTS.md`,
`cmake/VerifyCoreBoundary.cmake`, `cmake/BprDtlProvider.cmake`,
`cmake/BprDependencies.cmake`, the system architecture, `build-and-test.md`,
`release-packages.md` and `packaging/dependencies.json` name exactly the two
uses. `architecture_boundaries` allows dtl in exactly
`src/core/src/diff_intraline.cpp` and `src/core/src/review_cycle_suggestion.cpp`;
`test_dtl_has_exactly_two_owners` passes both files together and alone and
refuses a third core file and four other files, and fails under the old
single-owner rule.

**AC2, engine.** `matched_lines` runs `dtl::Diff` (O(NP)) over the trimmed
middle's line indexes, synchronously in core, with inputs bounded as before.
A difference in line counts over 512 returns nothing before dtl runs; an empty
side returns no pairs. The comparator stops dtl at its first comparison outside
the cap's band of diagonals and then compares everything equal, so dtl ends in
that round and the result is dropped; a finished edit longer than 512 also
returns nothing, so pairs exist exactly when the edit distance is at most 512.
A `static_assert` bounds dtl's path points at 264,196, under its 2,000,000-point
restart, and `onHuge` is never enabled. As in `diff_intraline.cpp`, a common
line out of order, out of range or unequal makes the middle one hunk. A shorter
original is padded at its end with lines that equal nothing, which makes dtl
choose the alignment the first-party Myers chose.

Why the stop cannot be evaded (integrator's check against dtl 1.21's
`compose` and `snake`): the padded original is never shorter, so dtl always
swaps and walks diagonals `k` = original index minus edited index. Round `p`
starts at its lowest diagonal `-p`, from `max(0, fp[-p+1])`. That snake makes
no comparison only when its start is already at the edited side's end; then the
same round's ascending sweep (`fp[k] >= fp[k-1] + 1`, no comparisons past that
end) carries the point to diagonal `delta` at the original's end, and the round
finishes. So round `band + 1` either compares on diagonal `-(band + 1)`, which
is outside the band and stops dtl, or finishes; at most `band + 1` rounds run,
which is the bound the `static_assert` checks.

**AC3, identical results.** No existing suggestion expectation changed and all
pass. The worker compared the new and old `matched_lines` on 60,000 random
inputs, including inputs past the cap, with no difference, and the new test
passes against the old source too. New `alignment_is_dtl_bounded_at_512_edits`
covers 512 and 513 edits with either side longer, the old tie-break, two edits
50,000 lines apart and a 2,000-line rewrite.

- `core-only` (Debug, `-g0`): `core_review_cycle*`, `core_diff_*` (no
  benchmark), `architecture_boundaries`, `architecture_boundary_contract`,
  `core_boundary_contract`, the dependency and package audit contracts and
  `coding_format_check`: 59 of 59 and 11 of 11 pass after merging main.
  The wider `core-only` run (no benchmark, large or perf tests) at
  `f5bea5271`: 269 of 272, the three failures being the worktree's missing
  `.agents` links. Changed-line tidy reports no findings; the documentation
  gate passes.
- Independent critical review of `a41035268` (read-only, scratch harness
  copying both implementations against dtl 1.21): no blocking finding. The
  band stop cannot be evaded (same argument as above; about 6.0 million inputs
  never exceeded `band + 1` rounds or the asserted path points), results equal
  the old Myers on about 5.8 million exhaustive, random and boundary inputs
  (binary alphabet lengths 0 to 10 exhaustive, empty sides, 512 and 513
  edits both ways, the tie-break), and no undefined behaviour was found. Known
  characteristic, not a defect: dtl completes its last round where the old
  Myers stopped at the first finishing diagonal, so a 200,000-line adversarial
  input (512 lines inserted at the front of an alternating file) took 0.30 s
  against 0.001 s at `-O2`; other shapes go the other way (512 spread
  deletions 0.003 s against 0.118 s). Work stays bounded by the band. The SES
  is copied twice while it is read, a few megabytes on the largest inputs.
