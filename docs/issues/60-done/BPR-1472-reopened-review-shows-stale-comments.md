---
schema: "implement-rfc.card.v1"
id: "BPR-1472"
title: "Reopening a pull request from Recently viewed shows comments published since"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1472--BPR-1472--a0d64e912270.json","origin":"owner report 2026-10-13"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3db9927d4","evidence":{"kind":"observation","detail":"Reopening the shown pull request from Recently viewed after a submitted review rereads it: finish course phase=18 reads=19 before=10, landing on Overview; reuses_shown is false once note_publication recorded the shown snapshot.","path":"docs/issues/60-done/BPR-1472-reopened-review-shows-stale-comments.md"}},{"criterion":"AC2","outcome":"pass","revision":"3db9927d4","evidence":{"kind":"observation","detail":"application_start_review_contract finish course submits two comments, goes Home and opens the Recently viewed row: START_REVIEW_FINISH_EVIDENCE 111111; with the two call sites reverted it fails reopen-reused-the-stale-read (reads=10 before=10, 111110).","path":"docs/issues/60-done/BPR-1472-reopened-review-shows-stale-comments.md"}},{"criterion":"AC3","outcome":"pass","revision":"3db9927d4","evidence":{"kind":"observation","detail":"With nothing published the shown read is reused: application_profile_window_review_restoration_public_path and application_setup_destination_public_path pass, as do reply_draft, attended_recent_replay, home_open_feedback and the composition contract after reseal.","path":"docs/issues/60-done/BPR-1472-reopened-review-shows-stale-comments.md"}}]
---

# Reopening a pull request from Recently viewed shows comments published since

The owner published a review from BetterPullReview one evening: Approve, with
two comments. The comments appeared on github.com straight away, so the
publication worked. The next morning the owner opened the same pull request
from Home's Recently viewed list, and the two comments were missing.
Refreshing Home first did not help. The comments appeared only after a
manual refresh of the open pull request.

## Acceptance

- AC1: After this window publishes to a pull request (a submitted review, or
  a single comment or reply), reopening that pull request from Recently
  viewed reads it again and shows the published comments without a manual
  refresh. A pasted link to that pull request does the same.
- AC2: A regression test drives the composed application: it submits a review
  with two comments, goes Home, and opens the pull request from Recently
  viewed. It requires one new snapshot read. The test fails on the code
  before the fix.
- AC3: If nothing was published since the shown read, reopening it from
  Recently viewed still reads nothing (BPR-0987 unchanged). The existing
  restoration and link-location courses still pass.

## Evidence

- Cause: `open_recent` in `src/app/src/github_application.cpp` lands on
  Overview with no read when the window already shows the row's pull request
  (BPR-0987, `shows_destination`). The pasted-link `open` does the same.
  Nothing rereads after a publication: the only `refresh_pull_request_intent`
  producer is the refresh button. Published drafts are not drawn
  (`conversation_cards_view.cpp` skips `shared()` drafts), so the shown read
  lacked the review's comments. Home's refresh glyph is `Discovery->refresh()`
  (queue discovery only), so it could not help. Other hypotheses were
  refuted. There are no conditional requests: no ETag or If-None-Match, and
  the native transport sets `AlwaysNetwork` and saves nothing to a cache. A
  pull request not already shown is read live; the stored overview loads only
  after a network failure.
- Owner logs: the release data root
  (`~/Library/Application Support/BetterPullReview/BetterPullReview`) has no
  session log after 2026-10-11 16:45. Its profile registry and epoch catalog
  stop at the same instant, and the review ledger stops on 2026-10-10. So
  the 2026-10-12 session did not use this data root on this Mac.
- AC1: `reuses_shown` replaces `shows_destination` at both shortcuts. It is
  false once `note_publication` recorded the shown snapshot. The record is
  made when `settle_review_comments` gets a review that is published or
  indeterminate, or when `complete_shared_draft` succeeds. A new accepted read
  has a new snapshot identity, which clears the record. With the fix, the
  `finish` course logs `phase=18 reads=19 before=10`: one fresh paged
  snapshot read, landing on Overview. That is the same read the owner's
  manual refresh made.
- AC2: the `finish` course of `application_start_review_contract` submits two
  comments, goes Home and presses Return on the pull request's Recently
  viewed row. `START_REVIEW_FINISH_EVIDENCE` is `111111` with the fix. With
  only the two call sites reverted it fails with
  `failed=reopen-reused-the-stale-read` (`reads=10 before=10`), evidence
  `111110`.
- AC3: with nothing published, the read is reused as before.
  `application_profile_window_review_restoration_public_path` (Recently
  viewed row of the shown pull request lands with no read) and
  `application_setup_destination_public_path` (a pasted link to the shown
  pull request reads nothing) pass. Also passing:
  `application_reply_draft_contract`, `application_attended_recent_replay`,
  `application_home_open_feedback_public_path`, and the `pending`, `finish`,
  `uncertain` and `again` start-review courses.
- Known failures that are not this change: the start-review `refused` course
  (`refusal-lost-the-draft`, a status-text mismatch) and
  `application_recent_account_isolation` (`identity-seed`). Both fail the
  same way with main's `github_application.cpp`.
- Upkeep: `application_review_round_trip_composition_contract` passes after
  the reseal over main 58ca8e97a. The format check, changed-line tidy
  (`NO_CHANGED_LINE_FINDINGS`), `DOCUMENTATION_OK` and `git diff --check`
  are clean.
