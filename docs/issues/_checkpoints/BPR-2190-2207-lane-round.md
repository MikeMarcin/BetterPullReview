# Lane round checkpoint: BPR-2207, BPR-2206, BPR-2190, BPR-2194, BPR-2195

Lane `claude/v0-5-0-prerelease-targets-52265d`. The assignment: work these
five cards in order, fixing the real cause of each and repairing the course
that proves it. Other implementers share this tree (BPR-2152/2154 queue and
steer, BPR-2189 local-draft check), so each commit names its paths.

## State

| Card | State | Commits |
|---|---|---|
| BPR-2207 | Done, mirrored to main | `b59d7c61d`, card `81d6a6108` (main `ad2ec51f0`) |
| BPR-2206 | Done, mirrored to main | `2185e9ab3`, card `d95a96006` (main `cbab7bcd3`) |
| BPR-2190 | AC01 and AC02 met; AC03 (owner's look at the shorter footer) pending | `0e5f2e1e1`, pin `984dd4696` |
| BPR-2194 | Code done, uncommitted until its 10x repeats finish | see below |
| BPR-2195 | Log keeping written, uncommitted; reproduction under load running | see below |

## Uncommitted, mine

- BPR-2194: `src/adapters/qt/include/bpr/qt/pull_request_overview_page.hpp`,
  `src/adapters/qt/include/bpr/qt/workbench_window.hpp`,
  `src/adapters/qt/src/pull_request_overview_files_interaction.cpp`,
  `src/adapters/qt/src/pull_request_overview_presentation.cpp`,
  `src/adapters/qt/src/workbench_window_drafts_and_behaviour.cpp`,
  `src/adapters/qt/src/workbench_window_github_presentation.cpp`,
  `src/adapters/qt/src/workbench_window_repositories_and_files.cpp`,
  `tests/end_to_end/progressive_open_public_path_tests.cpp`. Qt's
  `PendingReviewPath`, `open_pending_review_path` and the Overview page's own
  `PendingOpen` fallback are removed. In the window the Overview's
  activation while loading always went to `wait_for_review_at`, which hands
  the path to the host (BPR-2191's `open_when_listed`), so the Qt path was
  dead. Checks so far: `application_workbench_host` and `qt_pending_veil`
  pass.
- BPR-2195: `tests/end_to_end/private_data_root.hpp`
  (`e2e::course_logs_on_failure`), `tests/end_to_end/pending_states_public_path_tests.cpp`,
  `tests/end_to_end/CMakeLists.txt` (`BPR_COURSE_LOG_ROOT` for the course).

## Next action

1. Read `ctest -R 'progressive_open|pending_states' --repeat until-fail:10`
   (log in the session scratch, `t-2194.log`). Commit BPR-2194 and close its
   card on the lane and on main.
2. Read the concurrent copies (scratch `b2195/s1`). For each failure, read
   `COURSE_LOGS_KEPT` and find the last epoch stage and reason in the kept
   diagnostic log.
3. BPR-2190 AC03: the owner judges the footer, now one status line tall, in
   a frozen build. Park the card on human review.
