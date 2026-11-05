---
schema: "implement-rfc.card.v1"
id: "BPR-1866"
title: "My own pull requests show Changed and Commented · new commits on Home"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"bug","found_by":"owner report, 2026-11-04: \"My pull requests are showing as Changed and Commented new commits even if they haven't moved in a month. I opened them and closed them and it's the same. Pretty sure these belong in waiting for others and those statuses are wrong\"","reservation":"docs/issues/_reservations/BPR-1866--BPR-1866--8b95fb7d0d58.json","related_cards":["BPR-0989","BPR-1683","BPR-1718","BPR-1780","BPR-1843","BPR-1848","BPR-1850"],"owner_judgement":"none needed: the owner named the section (Waiting for others) and the fix puts the rows there; no layout or look changed","nearby_work":"BPR-1865 (Home list fuzzy filter) and BPR-1864 (window title) untouched; no Home list layout change"}
acceptance: [{"criterion":"AC1 My own commits and comments never count as new activity for me","outcome":"pass","revision":"d9cd0f893","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_review_queue_attention|application_home_own_activity_public_path)$'","detail":"Core: reviewed_by_viewer(true, authored) is false; bucket_for of an authored item whose own reply is on the head, or behind it after my push, is authored_awaiting_reviewer_action (quiet with nobody asked), never changed_since_my_review; my_review_shown clears a commented-with-new-commits verdict on an authored item and keeps a reviewer's; queue_membership_with_activity drops Reviewed, retained or observed, on an authored item. Composed: after Home Refresh reads my comment-only reviews (served as GitHub records an author's reply, a month old, on the head and on an earlier commit), #4001, #4005 and #4006 have no Changed, no verdict chip and no Reviewed reason; my own new comment on the open #4001 leaves it unread-free and Waiting."}},{"criterion":"AC2 Opening a pull request and closing it leaves its row saying what is true","outcome":"pass","revision":"d9cd0f893","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_home_own_activity_public_path$' (repeated 4x until-fail)","detail":"#4001 opened from Home (Return on its row), then Home and the Active review card's Close review: all three rows are still Waiting, with no verdict. Opened again, someone else's reply arrives by a pull request refresh after I read everything: Home hears of it while it is open (UnseenComments 1), and after closing and opening again it is still the one unread comment."}},{"criterion":"AC3 An authored pull request with requested reviewers and no new activity from others is in Waiting for others; someone else's new comment brings it to My pull requests","outcome":"pass","revision":"d9cd0f893","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_review_queue_attention|application_home_own_activity_public_path|application_home_waiting_public_path|application_personal_discovery_public_path)$'","detail":"home_section_for is waiting_on_others for each own-reply case and my_pull_requests for someone else's unread comment (Replied). Composed: the rows are listed in the Waiting for others panel and in neither My pull requests nor Needs me; with someone else's unread reply #4001 moves to My pull requests as Replied, with no verdict. The existing discovery and Home waiting courses still pass."}},{"criterion":"AC4 The course fails on the defect","outcome":"pass","revision":"d9cd0f893","evidence":{"kind":"command","command":"the core and application hunks reverted locally, test_personal_discovery_public_path rebuilt, the course run, the fix reapplied","detail":"Without the fix the course failed at its first post-Refresh check with exactly the owner's symptom: '#4001 bucket=changed since my review verdict=commented new-commits=1 reviewed=1 chip=Commented · new commits | You commented on this on 4 Oct 2026. New commits were pushed after it.'"}},{"criterion":"AC5 Live shape of the data","outcome":"pass","revision":"d9cd0f893","evidence":{"kind":"observation","detail":"MikeMarcin/BPR-Test, 2026-11-05: PR #147 opened as MikeMarcin; MikeMarcinTest left a line comment on 779b3047; MikeMarcin replied in that thread and then pushed 1d045113. GET /pulls/147/reviews: MikeMarcinTest COMMENTED 779b3047, MikeMarcin COMMENTED 779b3047, head 1d045113 - the author's reply is a comment-only review on the earlier head, which the app read as 'changed since my review'. Cleanup: both comments deleted, #147 closed, branch bpr-1866-own-reply deleted. X-RateLimit-Remaining 4958 before, 4927 after."}},{"criterion":"AC6 Gates","outcome":"pass","revision":"d9cd0f893","evidence":{"kind":"command","command":"_coding-standard/scripts/format.py <changed C++ files>; ctest --preset dev -R '^(coding_format_check|architecture_boundary_contract|test_network_isolation_structure_contract|application_review_queue_enrollment|application_review_queue_notification|application_queue_composition_contract|core_home_ignore|qt_review_landing_model|qt_home_list_scale_.*)$'; python3 scripts/verify_documentation.py --root .","detail":"All pass. architecture_boundaries and test_network_isolation_manifest fail identically on the base b399051a2 for reasons outside this card (QToolTip::showText in workbench_window.cpp; application_draft_delete_contract unregistered and the gh_cli_git_smart_transport argument rule). The new row is registered 'unmeasured (registered on macOS)' in WINDOWS_UNCONTAINED_TESTS."}}]
---

# BPR-1866: My own pull requests show Changed and Commented · new commits on Home

## Report

The owner, 2026-11-04: "My pull requests are showing as Changed and Commented
new commits even if they haven't moved in a month. I opened them and closed
them and it's the same. Pretty sure these belong in waiting for others and
those statuses are wrong."

## Root cause

GitHub records an author's reply in a review thread, and their comment on a
line of their own diff, as a pull request review by them in the `COMMENTED`
state, on the head of the moment. An author cannot approve or request changes
on their own pull request, so these are the only reviews they ever have there.

Home read the viewer's submitted reviews without asking whose pull request it
was:

- Personal discovery's metadata read (`personal_discovery_host`) set
  `ReviewedByMe` from any review whose author is the viewer and
  `HeadMovedSinceMyReview` from that review's commit against the head. Once
  the author pushed after replying, `bucket_for` returned
  `changed_since_my_review` ("Changed"), because that test runs before the
  authored-waiting one. The same reviews made `my_review_for` return
  `commented` with `NewCommits` ("Commented · new commits").
- `home_section_for` sends an authored row whose bucket is not
  `authored_awaiting_reviewer_action` to My pull requests, so the rows never
  reached Waiting for others (c). The section rule was right; its input was
  wrong.
- Opening the pull request (b) observes `CurrentViewerReview` from the
  snapshot, which is the same reply, so the open reported the same "reviewed,
  head moved". "Changed" is not a seen marker: it compares against the
  review's commit, deliberately ("Opening a PR does not review it"), so no
  open or close could clear it. The verdict chip is not decided by an open at
  all; it is carried from discovery's last read and, since [[BPR-1848]],
  restored from the stored facts at launch. Nothing is keyed to another
  identity: the reviews were matched to the right viewer, they were just the
  author's own.
- The unseen-comment count was already right: the ledger skips the viewer's
  own posts (`unread_posts`, `Mine`). [[BPR-1850]]'s activity store feeds
  desktop notifications only; Home does not read it.

## Fix

Core decides (`review_queue_attention`):

- `reviewed_by_viewer(reviewedByMe, authored)`: the viewer's own submitted
  reviews make them a reviewer only of a pull request they did not author.
- `bucket_for` uses it, so an author's reply and their push after it are never
  `changed_since_my_review`; with reviewers asked the row is
  `authored_awaiting_reviewer_action` and in Waiting for others.
- `my_review_shown(membership, review)`: no verdict on an authored row. The
  enrollment host applies it where it builds each item, so a row read now and
  a row restored from stored facts both lose the stale chip.
- `queue_membership_with_activity` adds no `Reviewed` reason to an authored
  row and drops a retained one, and the enrollment host no longer enrolls a
  `Reviewed` reason from the author's own reviews.

Someone else's new comment still counts: it is unread in the ledger, the row
is `new_comments_or_author_replies` ("Replied") and in My pull requests.
A reviewer whose reviewed head moved is still told it changed.

## Evidence

- `tests/core/review_queue_attention_tests.cpp`
  `test_my_own_activity_is_never_news_to_me`.
- `application_home_own_activity_public_path`
  (`test_personal_discovery_public_path --home-own-activity`): the real
  composition and fixture server. #4005 replied to a month ago on the current
  head (untouched), #4006 and #4001 replied to and then pushed; discovery,
  Home Refresh, open from Home and Close review, my own new comment, someone
  else's reply, open and close again, Refresh again. The fixture's
  `ViewerReply` serves the viewer's comment-only review in both the REST and
  batched reads; `ForeignReplies` makes a refresh's owed reply another
  reviewer's.

## Owner judgement

None needed. The owner named the section and the rows now land there; the
words, chips and Home list layout are unchanged.
