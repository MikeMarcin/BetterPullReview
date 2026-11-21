---
schema: "implement-rfc.card.v1"
id: "BPR-2326"
title: "Another author's draft waits in Following, not Needs me, until it is ready"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","found_by":"owner request, 2026-11-20: \"We need to move drafts out of needs me by default. Maybe we can stick them into following while they are in draft.\"","reservation":"docs/issues/_reservations/BPR-2326--BPR-2326--666f7b29f2e7.json","related_cards":["BPR-1683","BPR-1780","BPR-2301"],"related_rfcs":["RFC-BPR-0066","RFC-BPR-0098"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"],"frontend_design":"loaded: frontend-design:frontend-design; one Settings › Review card (Home) with a two-choice row in the existing style; Home rows reuse the Draft chip","disk_note":"free disk fell to 2.5 GB during the focused build (a freeze was also building); the task's build tree was removed"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "aa360f80d", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(core_review_queue_attention|core_home_ignore|core_home_next_move)$'", "detail": "test_another_authors_draft_waits_in_following: a review requested on a draft keeps bucket initial_review_requested and is in Following by default; with needs_me it is in Needs me; across every bucket only a Needs me section moves and only to Following; an ignored draft is in Ignored; an unread draft status (nullopt) stays in Needs me. Notification eligibility is bucket-based (permits_notification) and unchanged."}}, {"criterion": "AC2", "outcome": "pass", "revision": "aa360f80d", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(core_review_queue_attention|qt_home_list_scale_100|qt_home_list_scale_125|qt_home_list_scale_200)$'", "detail": "Core: Draft=false is back in Needs me. Qt test_another_authors_draft_waits_in_following: apply_item with Draft=false moves the row from Following to Needs me without a model reset (counts 1/2 -> 2/1). Not run against live GitHub: the refresh path that reads isDraft is unchanged."}}, {"criterion": "AC3", "outcome": "pass", "revision": "aa360f80d", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(core_review_queue_attention|core_home_next_move)$'", "detail": "My own draft (author role, ReviewersAwaited 1) has next move Draft and stays in My pull requests with either placement, also with unread comments; core_home_next_move unchanged and passing."}}, {"criterion": "AC4", "outcome": "pass", "revision": "aa360f80d", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(sqlite_workbench_settings_storage|qt_center_ribbon_width_scale_1.0|qt_settings_window.*|application_workbench_preferences_consumers)$'; BPR_WORKBENCH_ONLY_CASE=test_review_behaviour_rows_dispatch_follow_the_projection_and_reset_by_group better_pull_review_qt_workbench_window_tests", "detail": "test_others_drafts_from_thirty_six: a version-36 store migrates to 37 with Following and every other choice kept (reading widths included); Needs me stores and loads in column others_drafts; the CHECK refuses 2. Every older legacy fixture also drops the new column. Window case test_others_drafts_preference_follows_the_projection (in --center-ribbon-width and the default list; traced WORKBENCH_CASE_END): the Settings › Review › Home row starts on Following, Needs me dispatches the whole record, Home's landing model takes the placement only once published, Reset returns Following. The existing review-behaviour rows case passes with the new card."}}, {"criterion": "AC5", "outcome": "pass", "revision": "aa360f80d", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(core_agent_configuration|sqlite_agent_configuration_storage|qt_agent_configuration_dialogs|application_profile_settings_export_public_path)$'", "detail": "others_drafts is a Review choice (following, needs_me) inside format version 4; the export writes \"others_drafts\": \"needs_me\"; a file with only others_drafts reads it with nothing ignored; a file without the member carries no value and with_review leaves the placement unchanged; the profile-to-profile round trip names 12 changed settings. An older v4 reader treats the member as unknown, which BPR-2301's reader leaves out and names."}}, {"criterion": "AC6", "outcome": "pass", "revision": "aa360f80d", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(architecture_boundaries|qt_home_filter|qt_review_landing_model|qt_review_queue|application_review_queue_notification|settings_inventory_comparison_contract)$'; _coding-standard/scripts/format.py --dry-run (clang-format 22.1.8) on the 22 touched C++ files; python3 scripts/verify_documentation.py --root .", "detail": "The Following row carries the existing Draft chip (home_row_role::draft). The draft-chip look test now sets Needs me explicitly to keep measuring in that panel. All listed pass; format clean; documentation gate run on main after landing. qualification_case_inventory_contract fails before and after this change: tests/adapters_qt/composed_github_tests.cpp, which this card does not touch, no longer matches its pinned hash on main. Not run: the full dev build and full ctest (disk fell under 5 GB during the focused build, so the tree was removed after the focused suites)."}}]
---

# BPR-2326: Another author's draft waits in Following, not Needs me, until it is ready

## Asked

The owner, 2026-11-20: "We need to move drafts out of needs me by default.
Maybe we can stick them into following while they are in draft."

A pull request that GitHub reports as a draft is not ready for review, yet a
review request on it (or new comments, or commits since my review) puts it
in Needs me beside work that is.

## Acceptance

- AC1 Core: a pull request I did not author, whose GitHub draft status is
  read as a draft, and whose attention would place it in Needs me, is in
  Following instead. Its attention bucket, its reason sentence and its
  notification eligibility are unchanged; only its Home section moves. An
  unknown draft status changes nothing. Ignored still wins.
- AC2 Once GitHub reports it ready for review (the next refresh reads
  `isDraft` false), the same row is back in Needs me with no action by me.
- AC3 My own drafts are unchanged: they stay in My pull requests with the
  Draft next move ([[BPR-1780]], RFC-BPR-0098).
- AC4 Settings › Review gains a Home row, "Others' drafts", with "Following"
  (the default) and "Needs me". "Needs me" restores the previous placement.
  The choice applies to Home at once, in every window of the profile, and
  survives a restart (workbench settings schema step; an older store reads
  as Following).
- AC5 The choice travels in the optional Review section of the exported
  `.bprsettings` file ([[BPR-2301]]) as `others_drafts`
  (`following`, `needs_me`). A file without the member still loads and
  leaves the choice unchanged; an older reader leaves the member out and
  names it.
- AC6 Rows of such drafts carry the existing Draft chip. Tests: core policy
  (draft with a review request in Following; ready in Needs me; the setting
  on in Needs me; authored drafts and ignored rows unchanged), the Qt Home
  composition (the row's section moves live with the setting), the settings
  store and `.bprsettings` round trips; architecture boundaries, format and
  the documentation gate.

## Outcome

Home's core decides with one more input, `core::home_draft_placement`, the
profile's Settings › Review › Home "Others' drafts" (Following by default).
A pull request another author still holds as a draft is in Following while
its section would otherwise be Needs me; marked ready, the next refresh puts
it back. My own drafts are unchanged. The choice is stored in workbench
settings schema 37 and travels as `others_drafts` in `.bprsettings`.

Not verified: a live BPR-Test course (the draft read path is unchanged), and
the full dev build and test matrix (disk ran low; focused suites only).
