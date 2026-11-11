---
schema: "implement-rfc.card.v1"
id: "BPR-2092"
title: "My drafts are found in Threads, labelled Draft, with a filter for only my drafts"
status: "Done"
rfcs: ["RFC-BPR-0085"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2092--BPR-2094--a28677e4b291.json","origin":"owner request, 2026-11-11 (drafts round)","related_cards":["BPR-1293","BPR-1345","BPR-2093","BPR-2094"]}
depends_on: []
acceptance: [{"criterion":"BPR-2092 scope 1","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Place recorded with an edit; v4 record; older records read unchanged","command":"ctest --preset dev -R 'core_draft_roster|application_comment_range_draft_store'"}},{"criterion":"BPR-2092 scope 2","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Standing and place decided in core, no Qt","command":"ctest --preset dev -R core_draft_roster"}},{"criterion":"BPR-2092 scope 3","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Threads lists drafts labelled Draft / Not in review","command":"ctest --preset dev -R qt_conversation_finder_scale_100"}},{"criterion":"BPR-2092 scope 4","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"My drafts filter with count, not remembered (amended)","command":"ctest --preset dev -R qt_conversation_finder_scale_100"}},{"criterion":"BPR-2092 scope 5","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Row click/Return goes to the draft's lines with it open; composed --attended-drafts-look THREADS 5,2","command":"ctest --preset dev -R qt_conversation_finder_scale_100"}},{"criterion":"BPR-2092 scope 6","outcome":"pass","revision":"ee4724b2a98446ddeb2d33a46a99dc12520c2b19","evidence":{"kind":"observation","detail":"Owner Accept `828baa6d` on candidate `BPR-2092-threads-drafts`, session 585d18246da0bd78411dee4b26777a2d (handed back 2026-11-11T14:24Z), frozen root from ee4724b2a"}}]
---

# My drafts are found in Threads, labelled Draft, with a filter for only my drafts

## Origin

The owner, 2026-11-11: "I don't seem to have an easy way to find all my draft
comments." On Home: "I dunno if I'd put it there. I feel like discuss or
threads is probably better." No Home change.

## How drafts work today

A comment written at a line is a private local draft (`review_comment_draft`,
state `drafting`) the moment it is saved. **Add review comment** / **Start a
review** gives it a pending mark (`InReview`: the file's path at the head):
it is then *in the review* and rides the review's Submit. A draft without the
mark is *not in the review*: it is published only by its own Publish. A reply
can never be in a review. The window shows a saved draft only at its line in
the file on screen, and Threads shows only the one draft open in the drawer.
A saved draft does not record its file, so a draft in a file not opened this
session cannot be listed by file.

## Scope and acceptance

1. Every saved draft records where it was written: the file and whether a
   review could take it at its own line (the pending mark Start a review would
   give it). Older drafts without one are placed when their comparison is
   shown. Stored drafts read unchanged.
2. The core decides each draft's standing (in the review, not in the review,
   held by a submission, a reply) and its place, with no Qt.
3. Threads lists every unsent draft of mine in its file's group, labelled
   **Draft**, and for one not in the review a distinct "not in review" mark.
4. Threads' filter bar has **My drafts**: only my drafts, with their count.
   Amended 2026-11-11 (implementer): it is a way to find drafts, not a view
   to keep, so it is not remembered with Only new and Hide resolved (a
   remembered My drafts would open every later pull request on an empty
   Threads); under it no other filter hides a draft.
5. A draft row's click (Return) goes to the draft's lines in Review with the
   draft open in the drawer.
6. Owner look on a frozen build whose fixture pull request has several drafts,
   some in and some not in the review, and existing threads: Accept or Reject.

Load frontend-design before the Qt work.

## Placement: Threads, not Discuss

Threads already groups every thread by file, has the filter bar, and goes to a
line in Review; Discuss is the reading page for whole conversations, where a
line draft is written in Review and "Your draft is written beside its line"
(BPR-1581). Not a close call, so the look asks one Accept or Reject, with
Discuss as the alternative in a Reject note.

## Implementation (2026-11-11)

Frontend-design was loaded before the Qt work.

- Core: `review_draft_place` (`review_round_trip.hpp`): the file a draft is
  written in and, when Start a review could take it at its own line, the head
  it would be marked at. `revisioned_review_draft_command::ChangePlace` records
  it with an edit only (`valid(review_draft_place)`: bounded, non-empty, valid
  head). `draft_roster.hpp`: `project_draft_roster` (standing: in review,
  not in review, held, reply; place from the pending mark, the recorded place,
  a reply's thread, or a comparison this window showed; blank and finished
  drafts left out; file, side, line order), `draft_counts_by_file`,
  `drafts_outside_review`, `plan_add_to_review`. Pure; `core_draft_roster`.
- Storage: a placed draft is private-store record version 4 (version 3's
  optional first line and mark, then the place); every earlier record keeps its
  bytes and reads unchanged; the store refuses an unbounded place.
  `application_comment_range_draft_store` (`a_draft_place_survives_a_restart`).
- Host: every edit of the shown draft records its place, decided as Start a
  review decides it (`shown_draft_place`, `comment_destination`); the window's
  roster is projected after every saved-draft change and every newly shown
  comparison (`present_draft_roster`).
- Qt: Threads lists each saved draft in its file's group (opening the group
  when no thread does): "Draft" in a dashed pill, "Draft reply" for a reply,
  and an amber "Not in review" pill in the theme's attention colour; the
  **My drafts** chip (pencil and count) shows only them. A draft row's click
  or Return goes to its lines in Review with it open (`open_roster_draft`,
  the Finish-review route); a reply goes to its thread.

Not recorded: a draft saved before this change has no place until it is edited
or its file is shown; Finish review says "Open it to add it" for it.

## Owner look (filed 2026-11-11, not waited on)

Session `585d18246da0bd78411dee4b26777a2d`, state
`/Users/jaeger/.attended-review-state/drafts-round-v001-2026-11-11/state`,
candidate `BPR-2092-threads-drafts` (one Accept or Reject), cursor 9. Frozen root
`/Users/jaeger/drafts-round-v001.yva0tan3` from `ee4724b2a984` (executable
SHA-256 `56ba5f7f9482…`, launcher check reached
`ATTENDED_FINISH_REVIEW_SHOWN`); look `--attended-drafts-look` on
conventional-comments-v1 (renderer.cpp: threads, three comments in the review,
two drafts left out). The URL stays in the session state.

## Owner hand-back, session `585d1824…` (2026-11-11)

Read from `session.json` (handed back at sequence 20): `BPR-2092-threads-drafts` **Accept**
(`828baa6d`), no note. Host 24810/24818 stopped with `attended_review.py stop`;
root `drafts-round-v001.yva0tan3` removed with `freeze_attended_build.py
remove`. Every scope item passes; the card is Done.
