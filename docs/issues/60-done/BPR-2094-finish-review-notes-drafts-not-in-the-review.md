---
schema: "implement-rfc.card.v1"
id: "BPR-2094"
title: "Finish your review notes drafts that are not in the review"
status: "Done"
rfcs: ["RFC-BPR-0085"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2092--BPR-2094--a28677e4b291.json","origin":"owner request, 2026-11-11 (drafts round)","related_cards":["BPR-1345","BPR-1590","BPR-2092"]}
depends_on: []
acceptance: [{"criterion":"BPR-2094 scope 1","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Finish review line and list of drafts not in the review; composed OUTSIDE 2","command":"ctest --preset dev -R 'qt_review_submission_editor_scale_100|qt_review_submission_editor_scale_125|qt_review_submission_editor_scale_200'"}},{"criterion":"BPR-2094 scope 2","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Add all / Add / reason per row; nothing added unless asked; composed ADDED 5","command":"ctest --preset dev -R 'qt_review_submission_editor_scale_100|application_review_submission_application_host'"}},{"criterion":"BPR-2094 scope 3","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"plan_add_to_review in core, one durable write in host","command":"ctest --preset dev -R 'core_draft_roster|application_review_submission_application_host'"}},{"criterion":"BPR-2094 scope 4","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Verdict buttons unchanged; submit without adding","command":"ctest --preset dev -R 'qt_review_submission_editor_scale_100|application_review_submission_contract'"}},{"criterion":"BPR-2094 scope 5","outcome":"pass","revision":"ee4724b2a98446ddeb2d33a46a99dc12520c2b19","evidence":{"kind":"observation","detail":"Owner Accept `cca3371f` on candidate `BPR-2094-finish-drafts-line`, session 585d18246da0bd78411dee4b26777a2d (handed back 2026-11-11T14:24Z), frozen root from ee4724b2a"}}]
---

# Finish your review notes drafts that are not in the review

## Origin

The owner, 2026-11-11: "I also think that finish review should note draft
comments that haven't been added to the review."

## Scope and acceptance

1. Finish your review shows one line when any of my drafts is not in the
   review: "N drafts aren't in this review", and under it each draft with its
   file and lines. The line is absent when every draft is in.
2. **Add all** adds every draft a review can take; each row has **Add** (or
   says why a review cannot take it, e.g. a reply) and goes to its draft on a
   click. Nothing is added unless asked; a draft left out stays saved and is
   not published by Submit.
3. Which drafts a review can take, and what Add marks, is the core's
   decision (BPR-2092's place); the host performs it in one durable write.
4. The verdict buttons stay as they are: the reviewer can submit without
   adding them.
5. Owner look: Accept or Reject the line.

Load frontend-design before the Qt work.

## Implementation (2026-11-11)

Frontend-design was loaded before the Qt work.

- Finish your review (sheet and page) has a second section in its box under
  the pending comments: "N drafts aren't in this review", **Add all**, and each
  draft as a pending row is drawn (file, lines, label mark, first line), with
  **Add** where a review can take it, or why not under it ("A reply is sent to
  its thread on its own, never in a review."; "Open it to add it: it was saved
  before drafts recorded where they go."; "A review can't take it at its line:
  it was written on another diff or head."). A row's click goes to the draft.
  The section is absent when every draft is in, and while a submitted review
  waits. The verdict buttons are unchanged: Submit sends only what the review
  carries; a draft left out stays saved.
- A pending row's menu adds **Leave Out of Review**: the draft leaves the
  review and stays saved (a visible button would have narrowed every row of
  the 540-pixel sheet).
- Add and Add all: the core's `plan_add_to_review` (an edit at the listed
  revision that sets the place's mark), one durable write
  (`change_review_drafts`); Leave out: an edit that clears the mark.
- `qt_review_submission_editor_scale_{100,125,200}`; composed path
  `--attended-drafts-look` with `BPR_ATTENDED_DRAFTS_CHECK=1` on
  conventional-comments-v1: ATTENDED_DRAFTS_THREADS:5,2,
  ATTENDED_DRAFTS_OVERVIEW:3,2, ATTENDED_DRAFTS_OUTSIDE:2,
  ATTENDED_DRAFTS_ADDED:5.

## Owner look (filed 2026-11-11, not waited on)

Session `585d18246da0bd78411dee4b26777a2d`, state
`/Users/jaeger/.attended-review-state/drafts-round-v001-2026-11-11/state`,
candidate `BPR-2094-finish-drafts-line` (one Accept or Reject), cursor 9. Frozen root
`/Users/jaeger/drafts-round-v001.yva0tan3` from `ee4724b2a984` (executable
SHA-256 `56ba5f7f9482…`, launcher check reached
`ATTENDED_FINISH_REVIEW_SHOWN`); look `--attended-drafts-look` on
conventional-comments-v1 (renderer.cpp: threads, three comments in the review,
two drafts left out). The URL stays in the session state.

## Owner hand-back, session `585d1824…` (2026-11-11)

Read from `session.json` (handed back at sequence 20): `BPR-2094-finish-drafts-line` **Accept**
(`cca3371f`), no note. Host 24810/24818 stopped with `attended_review.py stop`;
root `drafts-round-v001.yva0tan3` removed with `freeze_attended_build.py
remove`. Every scope item passes; the card is Done.
