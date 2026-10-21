# RFC-BPR-0089 M1 checkpoint: Commits tab and contiguous filters

Assignment: milestone M1 of RFC-BPR-0089 (revision r3, main at `0a10d0b9f`),
from the RFC implementation coordinator. Branch
`worktree-agent-a66bfc62123613c66`, rebased by the coordinator onto main
`02ffdd9e5` (after BPR-1682, BPR-1683 and BPR-1684). Tested code candidate:
`07920aeb4473341ec22c435eb76896ce08f3e7b5`, which reseals the
command-correctness pins that the composition changes move. The commit
that adds this checkpoint and the cards changes only documentation. Issue IDs
are provisional (`BPR-PENDING-F1` to `F4`); the coordinator allocates them on
main when the work lands.

## Result

| RFC criterion | M1 status | Where |
|---|---|---|
| AC01 | pass, live and fixture | [[BPR-PENDING-F2]] AC1 |
| AC02 | pass for contiguous filters; a split file reads nothing and says why (M3 splits it) | [[BPR-PENDING-F2]] AC2 |
| AC03 | M3 | not started |
| AC04 | M2 | not started |
| AC05 | pass, live | [[BPR-PENDING-F2]] AC3 |
| AC06 | M1 part passes (the chip names a gone commit); rematch is M3 | [[BPR-PENDING-F2]] AC4 |
| AC07 | M4; comments stay head-only | not started |
| AC08 | M1 part passes under `core-only`; boundaries and documentation gates pass | [[BPR-PENDING-F1]] AC2 |

`frontend-design:frontend-design` was loaded before the Commits tab, the chip
and the filtered heading were drawn. No criterion of M1 is a look-and-judge
one, so nothing is parked on the owner and no attended session was opened.

## Live evidence

`tests/end_to_end/rfc0089_commit_filter_live_course.py` built the owner's
shape on MikeMarcin/BPR-Test: C1 source (4 files), C2 regenerating 321 files,
C3 mixed (8 files), 325 files in all. On `30ba92de0` (the same filter-path code
before the coordinator's rebase, whose new main commits do not touch it), pull
request #111
passed: every row, file set and the 34 diffs checked matched Git path by
path; Viewed state was unchanged for all 325 files. #109 (the first run)
failed on the split file, which kept the previous file's diff on screen; the
fix is the surface clear for `selected_file_split_by_commit_filter` in
`workbench_window.cpp`. #110 passed before the rebase. All three are closed and
their branches deleted. Each run spent under 100 REST calls; the account's
`X-RateLimit-Remaining` stayed above 4,700.

## Verification on the candidate

- Focused dev tests, 37 rows, all pass on the candidate, including
  `qt_current_file_unread_badge`, `qt_home_list_scale_100`,
  `sqlite_queue_enrollment_storage` and `command_correctness_registry_contract`
  for the merged work: the commit filter core, model, view,
  fixture contract and public path; the epoch, workbench, review mutation,
  snapshot observation and window read hosts; GitHub paged read and contract;
  gh CLI transport and smart transport; the Overview page, drawer parts and
  workbench window at three scales; the merge-base public path; network
  isolation manifest and structure; both architecture boundary rows.
- `live_mutation_isolation` and its contract pass (pre-rebase build).
- `core-only`: `core_commit_filter` and `core_diff_model` pass.
- `coding-format-check` passes with clang-format 22.1.8. clang-tidy with
  warnings as errors passes on every changed translation unit.
- The documentation gate passes apart from the provisional IDs.

## Shared files touched

`workbench_window`, `pull_request_overview_page`, `overview_status_drawer`,
`github_application.cpp`, `epoch_application_host`, the gh transport,
`main.cpp`, the tests' CMake lists and `verify_test_network_isolation.py`.
None of the concurrent agents' areas (keyboard shortcuts, Home, comparison
panes) is edited.

## Open obligations

- [[BPR-PENDING-F4]]: Find in files on the filtered set, and restoring the
  filter with the window. The RFC asks for both; no milestone names them.
- M2: generated files and file filters. M3: split files and rematch. M4:
  comments from filtered diffs.
- Windows and Linux are unmeasured; both new ctest rows are registered as
  unmeasured on macOS in the network isolation manifest.
- The chain store is transient and re-acquired per window; the RFC's cost
  question for long chains stays open.
- Additions and deletions are not counted for an exact filtered row.

## Command state

No task-owned process is running. The build directories were removed before
the report.
