---
schema: "implement-rfc.card.v1"
id: "BPR-1482"
title: "Join comment-range hunks one unchanged line apart, as GitHub does"
status: "Done"
rfcs: ["RFC-BPR-0082"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r2","source":"RFC-BPR-0082 finding outside the decision rules","origin":"owner request 2026-10-13"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"cc6bad14d (reapplied e2aa991f2) on claude/diff-engine-evaluation; integrated at 2b306d6ef","evidence":{"kind":"command","detail":"github_review_hunks merges hunks at most github_inter_hunk_context_lines (1) unchanged line apart. core_review_comment_range adds a one-line gap (one hunk [7,22); a range 12..19 across it is accepted) and a two-line gap (two hunks; a range 12..20 still crosses a hunk). core-only focused ctest (core_review_comment_range, core_review_round_trip, core_diff_*, core_review_cycle_*) 34/34 passed; clang-tidy clean; on the integrated evaluation build core_review_comment_range, core_review_round_trip, application_comment_range_draft_store, application_line_draft_contract, application_review_round_trip_composition_contract, qt_workbench_line_draft and qt_workbench_window_scale_100 pass."}},{"criterion":"AC2","outcome":"pass","revision":"2b306d6ef","evidence":{"kind":"command","detail":"run_measurements.py --modes measure --rounds 1 --reps 1 over all 39 candidates and the 86-pair corpus (78 with the diff-oracle-v1 GitHub recording): on every recorded file where a candidate's hunks joined as GitHub joins them equal GitHub's, BPR's comment hunks now equal GitHub's (zero exceptions, all 39 candidates); no alignment digest changed. Baseline BPR hunks exact 32 -> 45 of 78, comment flips 289,126 -> 288,422; l-myers-indent 46 -> 78, flips 3,142 -> 0. The report's 'Comment hunks after BPR-1482' table refreshes the counts for every candidate (docs/verification/RFC-BPR-0082.md)."}}]
---

# BPR-1482: Join comment-range hunks one unchanged line apart, as GitHub does

A finding of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md). GitHub's REST file list joins two hunks that one unchanged
line separates (`git diff --inter-hunk-context=1`); its raw `.diff` does not.
On all 58 recorded files that had both, joining the raw diff's hunks that way
gave exactly the file list's hunks, and linked xdiff Myers with the indent
heuristic then matched GitHub on 78 of 78 files. BPR's
`github_review_hunks` (`src/core/src/review_comment_range.cpp`) joins only
touching hunks, so multi-line comment ranges across such a one-line gap are
refused here though GitHub accepts them, whatever the matcher: the engine's
comment hunks matched GitHub on 32 of 78 files, but on 45 with GitHub's join.

## Acceptance

- **AC1:** `github_review_hunks` joins hunks at most one unchanged line apart,
  matching GitHub's file list; `core_review_comment_range` covers a one-line
  gap and a two-line gap.
- **AC2:** Against `tests/fixtures/github/diff-oracle-v1`, the engine's comment
  hunks equal GitHub's on every recorded file where its alignment equals
  GitHub's, and the evaluation report's comment-flip counts are refreshed.
