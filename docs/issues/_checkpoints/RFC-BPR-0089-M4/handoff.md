# RFC-BPR-0089 M4 checkpoint: comments from filtered diffs, BPR-1688 and closeout

Assignment: milestone M4 of RFC-BPR-0089 (revision r3), then [[BPR-1688]],
then the RFC's closeout, from the RFC implementation coordinator, over main
`d8a3f6b97`. Branch `worktree-agent-a4c2d01e7a59e471e`, rebased onto main
`ab1698599`. Code candidate `c44429b78` (one commit: M4, [[BPR-1688]], a
pre-existing course expectation and tidy finding, the pins). The commit after
it changes only documentation. New issue IDs are provisional
(`BPR-1703` to `L6`); the coordinator allocates them on main. No card's
metadata carries a `provisional_id` key.

`frontend-design:frontend-design` was loaded before the editor's target
notice, the filtered thread markers and the search panel's wording. No
criterion is a look-and-judge one, so nothing is parked on the owner and no
attended session was opened.

## Result

| RFC criterion | Status | Where |
|---|---|---|
| Measurement before M4 | done: BPR-Test #116, recorded in the RFC; #117 added the head's hunk rule | RFC engineering questions |
| AC07 | pass, fixture and live (#118) | [[BPR-1704]], [[BPR-1705]] |
| Thread markers under a filter | pass, fixture and live | [[BPR-1703]], [[BPR-1705]] |
| Split per-commit diffs | same placement (each is a filtered key) | [[BPR-1705]] |
| AC08 (M4 decisions) | pass under `core-only` | [[BPR-1703]] |
| [[BPR-1688]] AC1, AC2 | pass, fixture through the shipped (and desktop) composition | [[BPR-1688]] |
| RFC closeout | AC01 to AC08 pass; status Auto-Verified | [[BPR-1708]] |

## Decisions recorded

- **A line moved to the head** must lie within three lines of a change of the
  merge base to the head on its side; GitHub refused two such lines on #117
  with 422. Otherwise it is refused before Publish (`outside_github_diff`).
  It is not sent on the diff's commit instead: #116 did not measure how GitHub
  shows that.
- **The left side's file-comment fallback** is not delivered; such a line is
  refused with the reason ([[BPR-1706]]).
- **A placed line in a pending review** is refused ("published on its own"):
  the review's comments all name the head.
- **Find in files' core scope** carries the filtered count only; the list
  lives in the epoch host so the search state stays trivially copyable.
- **Kept filters** never store what a hidden commit changes; restoring needs
  the desktop composition, which owns the restoration store.

## Live evidence

MikeMarcin/BPR-Test, signed in through gh as MikeMarcin:

- #116, the measurement (11 calls; `X-RateLimit-Remaining` above 4,980).
- #117 (`--comment --prefix rfc0089-m4-comment1`): GitHub refused `RIGHT` 12
  and `LEFT` 10 at the head with 422, which drove the head's hunk rule; the
  course then timed out after Refresh (a filter gesture lost while the refresh
  landed, [[BPR-1707]]).
- #118 (`--comment --prefix rfc0089-m4-comment2`): every placement matched
  Git and GitHub (see [[BPR-1705]] AC2); markers after Refresh matched;
  Viewed unchanged for 326 files. 4,823 then 4,671.

All three pull requests are closed, every thread resolved and every branch
deleted.

## Verification on the candidate

- Composed: `application_commit_filter_public_path` (eight courses, repeated runs
  before the rebase, again after), the profile window restoration
  and document-tabs restoration courses, `application_source_pr_find`,
  `_search_scope`, `_search_retry`.
- Focused: `core_filtered_comment`, `core_commit_filter`,
  `core_generated_files`, `core_source_search_policy`,
  `core_window_review_restoration`, `application_review_mutation_application_host`,
  `application_review_mutation`, `application_epoch_host`,
  `application_commit_filter_model`, `application_window_review_restoration`,
  `application_source_search_host`, `application_source_pr_search`,
  `sqlite_review_mutation_recovery_storage`,
  `sqlite_window_review_restoration_storage`, `github_mutation_contract`,
  `github_read_contract`, `github_recorded_fixture_contract`,
  `github_contract_fixture_security`, `commit_filter_fixture_contract`,
  `profile_window_restoration_fixture_contract`, `qt_review_draft_editor`,
  `qt_source_search_panel` (1, 2), `qt_commit_filter_view` (100, 200),
  `qt_workbench_window` (100, 125, 200), `architecture_boundaries`,
  `architecture_boundary_contract`, `test_network_isolation_manifest`,
  `command_correctness_registry_contract`,
  `application_review_round_trip_composition_contract`.
- `core-only`: the five core rows above.
- `coding-format-check` passes with clang-format 22. clang-tidy with warnings
  as errors passes on every changed translation unit (one pre-existing
  finding in `source_search_panel.cpp` fixed).
- Negative controls: without keeping the filter authorization across a
  same-head refresh, the reread after a publication refuses every filtered
  read; the host refuses a commit outside the chain, unread lines and a reply
  off the head before any effect.
- The document-tabs restoration course failed on main since M2 (it counted
  the file menu's actions); it now finds Open in New Tab by name.
- Pins resealed with a note at the top of the round-trip contract.

## Open obligations and next action

- Allocate `BPR-1703` to `L6` on main.
- [[BPR-1706]]: the file-comment fallback for lines GitHub cannot
  resolve. [[BPR-1707]]: the reread and refresh gestures.
- Windows and Linux are unmeasured.

## Command state

No task-owned build, test, app or review process is running. Build trees, the
live clones and the course backup are removed at the end of the task.
