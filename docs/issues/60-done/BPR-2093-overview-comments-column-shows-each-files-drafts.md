---
schema: "implement-rfc.card.v1"
id: "BPR-2093"
title: "Overview's Comments column shows each file's drafts"
status: "Done"
rfcs: ["RFC-BPR-0085", "RFC-BPR-0049"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2092--BPR-2094--a28677e4b291.json","origin":"owner request, 2026-11-11 (drafts round)","related_cards":["BPR-1089","BPR-2092","BPR-2094"]}
depends_on: []
acceptance: [{"criterion":"BPR-2093 scope 1","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Drafts chip in Comments cell, fixed width, folder sums","command":"ctest --preset dev -R 'qt_pull_request_overview_page|qt_pull_request_overview_chips_scale_125'"}},{"criterion":"BPR-2093 scope 2","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Tooltip and accessible text in/not in review","command":"ctest --preset dev -R qt_pull_request_overview_page"}},{"criterion":"BPR-2093 scope 3","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Click/Enter opens Threads with My drafts on at the first draft; composed OVERVIEW 3,2","command":"ctest --preset dev -R qt_pull_request_overview_page"}},{"criterion":"BPR-2093 scope 4","outcome":"pass","revision":"7cd7fab2b5d682a546644499bc94b2c831f26c6c","evidence":{"kind":"command","detail":"Counts are core draft_counts_by_file","command":"ctest --preset dev -R core_draft_roster"}},{"criterion":"BPR-2093 scope 5","outcome":"pass","revision":"ee4724b2a98446ddeb2d33a46a99dc12520c2b19","evidence":{"kind":"observation","detail":"Owner Accept `2dc03edc` on candidate `BPR-2093-overview-drafts`, session 585d18246da0bd78411dee4b26777a2d (handed back 2026-11-11T14:24Z), frozen root from ee4724b2a"}}]
---

# Overview's Comments column shows each file's drafts

## Origin

The owner, 2026-11-11: "Actually it's fine is do stuff for drafts on overview
as well", "I would put in next to the file on overview", "Yeah I mean we
already have a comments column so probably something there".

## Scope and acceptance

1. A file row's Comments cell shows the file's unsent drafts as one chip
   beside its comment chips: a draft count, with a distinct mark when any is
   not in the review. It matches the column's chips in style and keeps the
   column's fixed width; a full cell folds comment chips into "+n" before the
   drafts chip. A folder row adds up its files.
2. The chip's tooltip and accessible text say how many drafts are in the
   review and how many are not.
3. A click (or the keyboard's activation) on the chip goes to that file's
   drafts: Threads with My drafts on and the file's first draft under the
   cursor.
4. The counts are the core's (BPR-2092's standing and place).
5. Owner look: Accept or Reject the column.

Load frontend-design before the Qt work.

## Implementation (2026-11-11)

Frontend-design was loaded before the Qt work.

- The Comments cell's chip layout takes the row's drafts
  (`overview_comment_chips(counts*, cell, font, drafts)`): one drafts chip, a
  pencil in a dashed outline (the draft's own mark in Threads) and the count,
  with an amber dot (the theme's attention colour) while any is not in the
  review. It keeps the cell's right end, just left of the qualifier's place, so
  drafts line up down the column; the comment chips fold into "+n" before
  they reach it; the column keeps its 216-pixel width. A folder row adds up
  its files; the "Not on a file" row has none.
- Its tooltip and keyboard announcement: "3 drafts of yours here. 2 in your
  review, 1 not. Click or press Enter to list them in Threads." A click or
  Enter opens Threads with My drafts on and the cursor on the row's (or the
  folder's) first draft.
- Counts are the core's `draft_counts_by_file`.
- `qt_pull_request_overview_page`
  (`test_drafts_chip_counts_and_goes_to_the_drafts`).

## Owner look (filed 2026-11-11, not waited on)

Session `585d18246da0bd78411dee4b26777a2d`, state
`/Users/jaeger/.attended-review-state/drafts-round-v001-2026-11-11/state`,
candidate `BPR-2093-overview-drafts` (one Accept or Reject), cursor 9. Frozen root
`/Users/jaeger/drafts-round-v001.yva0tan3` from `ee4724b2a984` (executable
SHA-256 `56ba5f7f9482…`, launcher check reached
`ATTENDED_FINISH_REVIEW_SHOWN`); look `--attended-drafts-look` on
conventional-comments-v1 (renderer.cpp: threads, three comments in the review,
two drafts left out). The URL stays in the session state.

## Owner hand-back, session `585d1824…` (2026-11-11)

Read from `session.json` (handed back at sequence 20): `BPR-2093-overview-drafts` **Accept**
(`2dc03edc`), no note. Host 24810/24818 stopped with `attended_review.py stop`;
root `drafts-round-v001.yva0tan3` removed with `freeze_attended_build.py
remove`. Every scope item passes; the card is Done.
