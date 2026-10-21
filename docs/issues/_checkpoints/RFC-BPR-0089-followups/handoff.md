# RFC-BPR-0089 follow-ups checkpoint: BPR-1706 and BPR-1707

Assignment: [[BPR-1706]] then [[BPR-1707]], the two RFC-BPR-0089 follow-up
cards the owner approved, from the RFC implementation coordinator, each
verified and committed before the next. Branch
`worktree-agent-a7e91eec21716b2ec`, rebased onto main `7435c6901`. Code
commits: `e46cdedee` ([[BPR-1706]]) and `4c76e46e2` ([[BPR-1707]]); each card
closes in the commit after its code. One new card has a provisional ID
(`BPR-PENDING-U1`, no `provisional_id` key); the coordinator allocates it on
main.

`frontend-design:frontend-design` was loaded before the editor's file-comment
notice and the Start a review refusal were written. No criterion is a
look-and-judge one, so nothing is parked on the owner and no attended session
was opened.

## Result

| Card | Criterion | Status | Where |
|---|---|---|---|
| [[BPR-1706]] | Measurement | done: BPR-Test #119, recorded in the RFC and the card | RFC engineering questions |
| [[BPR-1706]] | AC1, AC2, AC3 | pass, fixture through the shipped composition and live (#121) | card acceptance |
| [[BPR-1707]] | AC1, AC2 | pass, fixture through the shipped composition and live (#122) | card acceptance |

## Decisions recorded

- **A file comment is written at the head.** Its permalink names the line at
  the filtered diff's own commit on its side. On #119 GitHub moved a file
  comment written on C1 to the head and refused one on a commit that did not
  change the path, or on the base.
- **A direct fallback is a third first-attempt authorization** in
  RFC-BPR-0022's policy (`unresolvable_line`), with no native child and no
  freshness window. New enumerators are last, so stored values keep their
  meaning; an earlier reader refuses the new ones.
- **A file comment from a filtered diff stays a file-level conversation.**
  The window projects permalinks only for an unfiltered diff.
- **The accepted comparison stays in force** while a refresh resolves the
  same request again (`core::filter_comparison_in_force`). A comparison the
  reviewer chose since waits for its resolution, as M1 decided.

## Live evidence

MikeMarcin/BPR-Test, signed in through gh as MikeMarcin:

- #119, the measurement: fourteen calls, `X-RateLimit-Remaining` above 4,980.
- #120 (`bpr1706-live`): every placement and GitHub answer matched; the
  checker counted file comments as line threads (fixed).
- #121 (`bpr1706-live2`): PASS; three file comments at the head, current;
  4,802 then 4,648.
- #122 (`bpr1707-live`): PASS with the post-refresh gesture made once; 4,975
  then 4,825.

All four pull requests are closed, every thread resolved and every branch
deleted.

## Verification on the candidate

- Composed: `application_commit_filter_public_path` (repeated three times),
  `application_source_pr_find`, `application_profile_window_review_restoration_public_path`,
  `application_document_tabs_restoration_public_path`,
  `application_epoch_update_line_public_path`,
  `application_refresh_feedback_public_path`.
- Focused: `core_filtered_comment`, `core_review_round_trip`,
  `core_commit_filter`, `application_review_mutation_application_host`,
  `application_commit_filter_model`, `sqlite_review_mutation_recovery_storage`,
  `github_mutation_contract`, `github_review_comment_permalink`,
  `commit_filter_fixture_contract`, `qt_workbench_window_scale_100`,
  `qt_commit_filter_view_scale_100`, `command_correctness_registry_contract`,
  `application_review_round_trip_composition_contract`.
- `core-only`: `core_filtered_comment`, `core_review_round_trip`,
  `core_commit_filter`, `core_window_review_restoration`, `core_document_tabs`.
- `coding-format-check` passes with clang-format 22; clang-tidy with warnings
  as errors passes on every changed translation unit.
- Negative controls: the core's file-comment placement turned back into a
  refusal and its admission into a native attempt (core tests fail); the
  in-force rule reduced to "none while resolving" (core and composed fail at
  the refresh gesture); the kept publication's line re-request removed
  (composed fails at the placement).
- Pins resealed with notes at the top of the round-trip contract.

## Open obligations and next action

- Allocate `BPR-PENDING-U1` on main: a line chosen while a kept tab
  revalidates after a reread is replaced by the tab's remembered line.
- Windows and Linux are unmeasured.

## Command state

No task-owned build, test, app or review process is running. The build tree,
the live clones and the scratch link to the main checkout's skills are removed
at the end of the task.
