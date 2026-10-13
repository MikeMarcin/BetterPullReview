---
schema: "implement-rfc.card.v1"
id: "BPR-1475"
title: "Reread a pull request on every reopen and after publishing to it"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1472"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1475--BPR-1475--f4fade45d6e3.json","origin":"owner decision 2026-10-13"}
---

# Reread a pull request on every reopen and after publishing to it

BPR-1472 found that a pull request reopened from Home's Recently viewed list
could show a read taken before this window published a review to it. It
fixed that narrowly: the shown read was reused unless this window had
published over it. The owner then decided two questions BPR-1472 raised
(2026-10-13).

1. **Reread on every reopen.** Opening a pull request from Home's Recently
   viewed, or from a pasted link, always reads it fresh from GitHub, even when
   the window already shows that pull request. This replaces BPR-0987's "no
   second read" rule for that case. A paged read per reopen is accepted.
2. **Reread after publishing.** After a successful publish from the window (a
   review, a comment or a reply), BPR rereads the pull request automatically,
   so the window immediately shows what was posted.

## Acceptance

- AC1: Opening the pull request the window already shows, from its Recently
  viewed row or from a pasted link to it, reads it fresh from GitHub (one new
  snapshot read) and lands where an ordinary open lands; nothing reuses the
  shown read.
- AC2: A successful review submission from the window rereads the pull
  request by itself, so the submitted review shows without a manual refresh.
  A comment or reply published from the window does the same. A read already
  in flight is not doubled.
- AC3: The reread after publishing keeps the user's place: the window stays
  in the modality it was in, the selected file and its diff stay selected,
  and an open note is not closed.
- AC4: The tests that enforced BPR-0987's "no second read" for a reopen are
  updated to the owner's decision and pass:
  `application_profile_window_review_restoration_public_path` and
  `application_setup_destination_public_path`.

## Evidence

- AC1: `open_recent` and the pasted-link `open` in
  `src/app/src/github_application.cpp` no longer reuse the shown read.
  BPR-1472's `PublishedOverSnapshot`, `note_publication()` and
  `reuses_shown()` are removed. A Recently viewed row of the shown pull request
  goes through the Home bar's path, and a link dispatches
  `open_pull_request_intent`. The `finish` course reopens from Recently viewed:
  `reads=37 before=28`, with a new snapshot, landing on Overview.
  `application_setup_destination_public_path`'s link-location course needs a
  fresh read for each link to the shown pull request (file, review, discussion
  comment). `application_profile_window_review_restoration_public_path`
  needs a fresh read for the shown pull request's Recently viewed row, landing
  on Overview. Both pass. `setup_destination_opening::navigated` stays in the
  application port, but the composed window no longer returns it.
- AC2: `reread_after_publication()` runs after:
  - a review GitHub confirmed, on its answer or on Check status (the new
    `review_submission_workflow::set_publication_observer`, called with or
    without comments);
  - a comment or reply that succeeded (`finish_review_terminal`, which
    replaces the old Refresh-button trigger);
  - a published tour.

  It owes one read and pays it when no read is in flight, so a read already
  running is never doubled and never dropped. It dispatches the core's
  `refresh_pull_request_intent` straight to the window read. It does not use
  the Refresh gesture, because that path cancels setup and restoration and
  clears the status line ("Review submitted with 2 comments" disappeared
  before this change was made). The `finish` course submits a review and
  presses nothing else: `phase=17 reads=19 before=10`, with a new snapshot.
  With `reread_after_publication` disabled, the same course fails
  `no-reread-after-publish` (`reads=10 before=10`, evidence `11111000`).
  The fixture's pull-request read does not include posts sent to the
  review-submission fixture server, so the course proves the read and not
  the drawn review. Showing a submitted review is what that same read did for
  the owner's manual refresh in BPR-1472.
- An unknown outcome is not reread. A review's unknown outcome is settled by
  Check status. A read accepted while Check status runs can replace the
  authority it runs under: one `uncertain` run in about 17 failed
  `not-reconciled` while the reread was on. Check status rereads once it finds
  the review published. A comment or reply's unknown outcome holds its draft
  for a retry that a reread retires: `application_label_publication_recovery`
  failed and `application_inline_publish_cli_consent` crashed with the reread
  on, and both pass with it off.
- AC3: after the submission's reread the window stays in Review on
  `src/engine/renderer.cpp` with the same selection (`file=1 selection=1`),
  and the status line keeps "Review submitted with 2 comments". A note being
  written survives a reread (`phase=20 writing=1 field=1 written=1 open=1`).
  The new core decision `core::resume_comment_after_refresh`
  (`tests/core/reply_draft_standing_tests.cpp`) holds the comment again, in
  the way BPR-1374 holds a reply. The drawer state is recorded when the read
  begins and reopened with `begin_new_comment`. The `pending` course now
  waits for the single comment's reread before it chooses another file (it
  was 2 failures in 7, now 12 of 12 pass).
- AC4: both BPR-0987 courses are updated as described under AC1 and pass on
  the main-merged tree. `docs/ux/workbench-specification.md` now states both
  decisions.
- Also passing: `core_reply_draft_standing`, the start-review `pending`,
  `finish` (`11111111`), `uncertain` and `again` courses,
  `application_reply_draft_contract`, `application_threaded_reply_contract`,
  `application_line_draft_contract`,
  `application_published_file_switch_contract`,
  `application_review_progress_push_contract`,
  `application_inline_publish_cli_consent`,
  `application_label_publication_recovery`,
  `application_thread_words_public_path`,
  `application_attended_recent_replay`,
  `application_home_open_feedback_public_path`, all nine
  `application_review_tour_publication_*`, and
  `application_review_round_trip_composition_contract` after the reseal over
  main a05bf2623 (implementation 06399eb7f).
- Known failures that also fail on main: the start-review `refused` course
  (`refusal-lost-the-draft`) and `application_recent_account_isolation`.
