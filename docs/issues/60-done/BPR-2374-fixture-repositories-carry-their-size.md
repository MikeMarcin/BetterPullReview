---
schema: "implement-rfc.card.v1"
id: "BPR-2374"
title: "Recorded GitHub fixtures serve the repository's size, so BPR-2364's whole-history bound no longer refuses every fixture review"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"coordinator, 2026-11-24: every end-to-end course that opens bpr-fixtures/nested-folders#21 failed at 'the file opens' after 606cf91a1 and c88b2bfa6","reservation":"docs/issues/_reservations/BPR-2374--BPR-2374--c7b51b8109d3.json","related":["BPR-2364","BPR-2291","BPR-1879"],"acceptance":["BPR-2374:contract","BPR-2374:courses","BPR-2374:bpr-2364"]}
---

# BPR-2374: Recorded GitHub fixtures serve the repository's size

## Report

After [[BPR-2364]] (606cf91a1, c88b2bfa6), courses that open the fixture
pull request `bpr-fixtures/nested-folders#21` stopped at "the file
opens". The window said:

```text
No file is open. Choose one of the 17 actionable file(s). Review history
could not be updated. BetterPullReview couldn't limit the history download
for this pull request. Link a clone of the repository or sign in with
GitHub CLI to load selected files, then refresh.
```

Examples: `application_ask_setup_review_public_path`,
`application_tour_feedback_queue_public_path` and
`application_tour_chat_queue_{stop,whole_tour,local_draft}_public_path`.
Courses on other recorded fixtures failed the same way. Examples:
`application_header_source_navigation`, and on the large-review fixture
`application_header_source_navigation_large` and
`application_selected_file_safety_public_path`.

## Cause

The review's first read logged:

```text
loading_decision  loading=selected loaded_sparse=0 sparse_capable=0 cache_has_parent=0
smart_acquire     failed fetch_history_too_large
revision_prepare  failed fetch_history_too_large
```

[[BPR-2364]] Change 2 (`acquire_review_transport`,
`src/application/src/epoch_host_support.cpp`) refuses a complete-closure
fetch with no haves and no linked clone when the repository is past the
bound or its size is unknown, and the lease cannot fall back to selected
objects. That is AC3, and it is correct.

The fixture met every one of those conditions, although no real review
does:

- The fixture's Git lease serves only complete packs, so it does not
  support sparse commit trees. In the product, a lease without sparse
  support is the signed-out (public preview) read, or a `gh` lease whose
  account changed.
- The size was unknown because no recorded `repository.json` carries
  GitHub's `size`. GitHub's repository response always carries it, in
  kibibytes. The product reads it as the bound ([[BPR-1879]],
  [[BPR-2291]]). The strict recorded contract
  (`validate_repository_fixture`, `src/adapters/github/src/github_read_contract.cpp`)
  accepted only the seven keys without `size`, so a recording could not
  serve one. Adding `size` to a recording was refused as
  `unexpected_response_fields`, and the read failed as
  `malformed_response`. The large-review fixture's synthetic
  `GetRepository` had no size either.

Before [[BPR-2364]], an unknown size was not too large
(`complete_closure_too_large`), so the fixture fetched its small
complete pack. The fixture, not [[BPR-2364]], was wrong: it modelled a
repository whose size GitHub does not report.

## Fix

- Test contract (`github_read_contract.cpp`): a recorded repository may
  carry `size`, and only as a plain non-negative integer. Without it the
  key set is unchanged.
- Fixture (`src/app/src/github_public_path_fixture.cpp`): every served
  `GetRepository` carries `size`. That covers recorded contracts and the
  paged large-review recording. The size is the served packs' bytes in
  KiB, rounded up (1 KiB for the embedded pair). It stays true to what the
  fixture actually serves and is far below the 32 MiB bound. The recorded
  files are unchanged, so the fixture security tables are unchanged too.
- [[BPR-2364]]'s product behaviour is unchanged. A review of unknown or
  oversized history on a lease without sparse support is still refused.

## Acceptance

- **BPR-2374:contract** `test_recorded_repository_size`, run first in
  `better_pull_review_github_contract_tests`: a recording without a
  size stays valid. A size of 5 reaches the normalized read as 5 KiB. A
  quoted, negative, fractional or null size is refused as
  `unexpected_response_fields`. Passed at fa87cd4d5. The
  `github_read_contract` ctest as a whole is not claimed. It fails at
  fa87cd4d5 and at its parent on an older check, the EOF canonical-read
  comparison, which this change does not touch.
- **BPR-2374:courses** On macOS arm64 Debug, 67 courses failed with
  BPR-2364 in place and without this fix. All of them pass at fa87cd4d5:
  `application_accepted_file_in_overview`,
  `application_agent_chat_anchored_continuation_contract`,
  `application_agent_chat_composition_contract`,
  `application_agent_chat_construction_cut_contract`,
  `application_agent_chat_continued_contract`,
  `application_agent_chat_conversation_contract`,
  `application_agent_chat_custom_agent_contract`,
  `application_agent_chat_custom_agent_refused_contract`,
  `application_agent_chat_held_turn_public_path`,
  `application_agent_chat_helpers_public_path`,
  `application_agent_chat_helpers_unfinished_public_path`,
  `application_agent_chat_setup_list_contract`,
  `application_agent_chat_stale_contract`,
  `application_agent_chat_subject_contract`,
  `application_agent_keep_action_public_path`,
  `application_agent_queue_close_runs_on_public_path`,
  `application_agent_queue_finish_pauses_public_path`,
  `application_agent_queue_restart_public_path`,
  `application_agent_queue_second_window_loading_public_path`,
  `application_agent_queue_tour_feedback_crash_public_path`,
  `application_agent_queue_windows_public_path`,
  `application_agent_steer_sent_public_path`,
  `application_agent_steer_unavailable_public_path`,
  `application_agent_suggestion_range_public_path`,
  `application_anonymous_public_preview_public_path`,
  `application_ask_addressed_public_path`,
  `application_ask_menu_public_path`, `application_ask_queue_public_path`,
  `application_ask_report_public_path`,
  `application_ask_setup_review_public_path`,
  `application_change_arrows_across_files`,
  `application_diff_workbench_public_path`,
  `application_epoch_fixture_noncanonical_root_cause`,
  `application_epoch_maintenance_public_path`,
  `application_epoch_persistence_ghes-3_18`,
  `application_epoch_persistence_github-com`,
  `application_explain_file_public_path`,
  `application_fixture_agent_guard_public_path`,
  `application_follow_up_public_path`,
  `application_github_public_path_ghes_3_18`,
  `application_github_public_path_github_com`,
  `application_github_public_path_nested_folders_v1`,
  `application_github_public_path_uiforetw_74`,
  `application_header_source_navigation`,
  `application_header_source_navigation_large`,
  `application_initial_epoch_file_path`,
  `application_overview_double_click`, `application_overview_summary_1`,
  `application_overview_summary_1_25`, `application_overview_summary_2`,
  `application_pending_states_public_path`,
  `application_prompt_facts_public_path`,
  `application_prompt_macros_public_path`,
  `application_returning_review_addressed_public_path`,
  `application_returning_review_gone_public_path`,
  `application_returning_review_latest_public_path`,
  `application_returning_review_mark_public_path`,
  `application_review_report_public_path`,
  `application_selected_file_safety_public_path`,
  `application_setup_destination_public_path`,
  `application_suggest_change_public_path`,
  `application_thread_words_public_path`,
  `application_tour_chat_queue_local_draft_public_path`,
  `application_tour_chat_queue_stop_public_path`,
  `application_tour_chat_queue_whole_tour_public_path`,
  `application_tour_feedback_queue_public_path`,
  `application_uiforetw_selected_diff_contract`.
  These 9 passed both before and after:
  `application_discuss_top_public_path`,
  `application_markdown_images_public_path`,
  `application_nested_overview_folders_public_path`,
  `application_overview_feedback_public_path`,
  `application_overview_labels_public_path`, `application_public_path`,
  `application_refused_first_read_public_path`,
  `application_status_attention_public_path`,
  `large_review_replay_contract`.
- **BPR-2374:bpr-2364** [[BPR-2364]]'s own tests pass at fa87cd4d5:
  `application_epoch_host`, `git_process_contract`,
  `gh_cli_git_smart_transport`, `core_operation_diagnosis`,
  `qt_agent_launch_panel`, `quill_logging`,
  `quill_logging_content_policy` and `architecture_boundaries`. So do
  `github_contract_fixture_security`, `application_github_contract` and
  `github_anonymous_read_contract`.
