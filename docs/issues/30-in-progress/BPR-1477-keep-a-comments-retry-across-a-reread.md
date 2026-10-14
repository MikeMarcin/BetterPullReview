---
schema: "implement-rfc.card.v1"
id: "BPR-1477"
title: "Keep a comment's retry when its pull request is read again"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1475"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1477--BPR-1477--58607c90b7ff.json","origin":"BPR-1475 implementation finding"}
---

# Keep a comment's retry when its pull request is read again

BPR-1475 rereads a pull request after the window publishes to it. While
building that, its worker found that rereading while a comment's outcome is
unknown breaks the retry. In that state the comment's draft is held
`publishing`, locked to the body it was sent with, and offers Retry, which
reconciles instead of posting again. With an automatic reread at that moment,
`application_inline_publish_cli_consent` crashed and
`application_label_publication_recovery` failed. BPR-1475 therefore does not
reread after an unknown outcome. Refresh can still be pressed in that state.

## Acceptance

- AC1: Pressing Refresh while a new comment's publication outcome is unknown
  reads the pull request again and keeps the draft held for the same retry.
  After the read, the draft is open in the drawer, locked to its prepared
  body, says why the result is unknown, and offers Retry. Retry reconciles
  the one stored comment with no second write. An end-to-end course proves
  this and fails on the old code.
- AC2: The fix is made where the decision is taken. The core decides which
  draft a reread holds again, and the composition restores the retry under
  the new composition. A comment held by a submitted review stays that
  review's to settle.
- AC3: Once the retry survives a reread, the automatic reread after a comment
  or reply's unknown outcome is tried. It is turned on only if no course shows
  it losing a retry.
- AC4: BPR-1475's courses and the neighbouring draft and publication
  contracts still pass, and the review round-trip composition contract is
  resealed.

## Root cause

A reread that is accepted recomposes the review mutation host
(`compose_review_mutation` in `src/app/src/github_application.cpp`).
Retiring the old host reset `RetryableReviewDraft`, and the editor's draft was
cleared with the composition. BPR-1475's `ResumeComment` held again only a
comment still `drafting`. A comment held `publishing` after a lost answer was
therefore saved and locked but shown nowhere: the editor held no draft, the
drawer card was closed, and Retry was gone. That is the
`application_label_publication_recovery` failure (the editor's body was empty
when the course tried to relabel). The `application_inline_publish_cli_consent`
crash was a use-after-free in that course's `expect_status_visible`: it kept
the drawer card's status label across event processing, and the reread
replaced the card. lldb stopped in `QFontMetrics(label->font())` at
`inline_publish_cli_consent_tests.cpp:100`.

## Evidence

- AC1: the new `lost-refreshed-retried` course in
  `tests/end_to_end/label_publication_recovery_tests.cpp` loses the answer,
  presses Refresh, and waits for a new accepted snapshot with no read in
  flight. It then requires the draft to be open in the drawer, `publishing`,
  with its body, locked, saying "retry is safe" and offering Retry. Retry then
  gives `native_posts=1 submissions=2 stored=1 duplicate=1 posted=1`. On the
  old code (main 44c91ecdf, and again with only the core decision reverted)
  it fails after the Refresh with `writing=0 state=-1 body="<none>"
  publish=none retry=0`. It passes 3 of 3 runs on the fix.
- AC2: `core::resume_comment_after_refresh`
  (`src/core/src/reply_draft_standing.cpp`) now also holds a new comment that
  is `publishing` and not `held_by_review`. Scenario `reread holds the retry`
  in `core_reply_draft_standing` covers this, including a submitted review's
  comment and a reply. `compose_review_mutation` asks the core and keeps the
  retry record's reason (`retryable_review_draft_record::Failure`, recorded
  where `present_inline_publication_failure` says it). `resume_comment_at`
  re-establishes `RetryableReviewDraft` under the new authority, says the
  reason again, and reopens the locked draft at its line with the new
  `workbench_window::reopen_draft_in_drawer`. `begin_new_comment` sends a
  read-only draft to Discuss, so it is not used for a locked draft. The same
  path also keeps a comment whose publication was in flight when a read
  landed: it is held as unknown instead of disappearing. The CLI course's
  `expect_status_visible` now finds the label again on every look.
- AC3: tried and not turned on. Rereading after an unknown outcome whose
  request went out (`MutationAttempted`) made the label and CLI suites pass,
  after the CLI course was taught to wait for that reread. But
  `application_review_round_trip_composition_contract` then failed:
  `SEMANTIC_COMMENT_SERVER_REFRESH_STALLED` at phase 8 (`post=1
  pending_selection=0 draft=-1`, with `submissions=2 duplicate=1` at the
  fixture). That course presses Retry while the window's reread after the
  lost answer is still in flight. Its retry reached the server, but its
  posted thread was never selected. The likely cause is that the read
  replaced the mutation host the retry was running on, so its answer was
  dropped. A reviewer who presses Retry at once would hit the same race. With
  that reread off, the contract passes. `finish_review_terminal` still
  rereads only on success, and `docs/ux/workbench-specification.md` now
  states that a Refresh keeps the retry.
- AC4: passing on the main-merged tree (main e145ccd35, implementation
  c016ecba1, reseal 8664a7226): `core_reply_draft_standing`,
  `application_label_publication_recovery` (three courses, 3 of 3 runs),
  `application_inline_publish_cli_consent` (six courses, 3 of 3 runs),
  `application_review_round_trip_composition_contract` (resealed over the
  merge, which carries BPR-1479's unsealed `github_application.cpp` change),
  `application_reply_draft_contract`, `application_threaded_reply_contract`,
  `application_line_draft_contract`,
  `application_published_file_switch_contract`,
  `application_review_progress_push_contract` and `architecture_boundaries`.
  Before the merge, on the same fix, `application_setup_destination_public_path`,
  `application_thread_words_public_path`, `qt_composed_github_scale_100`, and
  the start-review `pending`, `finish`, `uncertain` (`1111`) and `again`
  (`1111111111`) courses also passed. Earlier
  in the work, with the automatic reread on (a superset of this behaviour),
  `application_profile_window_review_restoration_public_path`,
  `application_attended_recent_replay`,
  `application_home_open_feedback_public_path`, all nine
  `application_review_tour_publication_*` and the `qt_composed_github` scales
  also passed.
- Known failure, unchanged: the start-review `refused` course fails
  `refusal-lost-the-draft` (`START_REVIEW_REFUSED_EVIDENCE:1100`), as
  BPR-1475 recorded on main. It is a definite review refusal, which this
  change neither rereads nor holds again.
- Gates: format clean (`_coding-standard/scripts/format.py`, clang-format
  22.1.8). clang-tidy over the six changed translation units reports no
  diagnostic on a changed line (9 on unchanged lines).
  `scripts/verify_documentation.py` (`DOCUMENTATION_OK`) and
  `tests/scripts/verify_documentation_tests.py` pass on the main-merged tree.
