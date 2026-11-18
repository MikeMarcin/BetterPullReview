---
schema: "implement-rfc.card.v1"
id: "BPR-2260"
title: "A draft clicked away from keeps its words"
status: "Done"
rfcs: []
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2260--BPR-2260--1c6312374c89.json","origin":"owner, 2026-11-18"}
acceptance: [{"criterion":"AC-a","outcome":"pass","revision":"fef6ae373","evidence":{"kind":"command","detail":"qt_conversation_cards_view test_a_draft_clicked_away_from_paints_its_words: draft written in the drawer, the thread below pressed open; the draft note is not Writing, carries its post (author mmarcin, its words) and is as tall as a fresh view measures it"}},{"criterion":"AC-b","outcome":"pass","revision":"fef6ae373","evidence":{"kind":"command","detail":"same test, offscreen viewport grab: ink inside the draft note's dashed frame"}},{"criterion":"AC-c","outcome":"pass","revision":"fef6ae373","evidence":{"kind":"command","detail":"same test: close_thread keeps the post; begin_draft makes the draft the composer again"}},{"criterion":"AC-d","outcome":"pass","revision":"fef6ae373","evidence":{"kind":"command","detail":"the test fails on the unfixed source (\"my draft's note carries its author and words, not an empty composer\") and passes with the fix"}}]
---

# A draft clicked away from keeps its words

## Origin

The owner, 2026-11-18, with photos of the drawer: "I have a draft comment
sometimes not rendering when I click away from it." Drawn correctly, my
draft's note names me, its line and Draft and shows two lines of its words;
after another thread is clicked open it is an empty dashed frame, shorter,
with no avatar, author or words. No look is needed.

## Scope and acceptance

- **AC-a.** My draft written in the drawer, then another thread opened by a
  click (or the thread closed), my draft's note carries its post: author,
  line, Draft and its words, measured as a fresh view measures it.
- **AC-b.** Rendered offscreen, the note paints ink inside its dashed frame.
- **AC-c.** Opening the draft again makes it the composer again.
- **AC-d.** A Qt test fails without the fix and passes with it.

## Root cause

`conversation_cards_view` draws the draft being written in the drawer as its
composer, and builds that draft's closed note without a post (the composer
shows the words). `choose` (a click on another thread) and `begin_reply`
moved `Open` off the draft but left `Writing` set. `writing_draft()` then
answered false, so the window never ended the draft, while every relayout
still built the draft's note as a composer's: no post, a header's height, and
`paint_closed` drew the dashed frame and returned. The empty frame stayed
until the window next ended or began the draft, hence "sometimes".

## Built

- `leave_draft()` ends writing the draft when its card is the open one; it
  runs before `Open` changes in `choose` (both paths), `begin_reply` and
  `close_thread`.
- The relayout derives the composer card from `writing_draft()` (the draft
  written and open), not the bare `Writing` flag.

## Verification

On the lane at `fef6ae373`, dev preset, macOS, offscreen: the new test
fails on the unfixed source and passes with the fix. Passed:
`qt_conversation_cards_view`, `qt_composed_github_scale_100`,
`qt_composed_inline_drawer_scale_{100,125,200}`,
`qt_composed_resident_drawer_filters`, `qt_compose_review_entry_scale_100`,
`qt_workbench_line_draft`, `qt_review_draft_editor`,
`qt_github_conversation_panel`, `qt_thread_moves_scale_100`,
`qt_local_draft_window`, the `application_local_draft_*` and
`application_local_comments_found` public paths,
`application_label_authoring_contract`, `application_writing_delivery`,
`application_review_tour_writing_wrapper`. `application_review_tour_writing`
fails on its signed-out wording check, which BPR-2259 changed and which this
card does not touch. Format check passes on the changed files; clang-tidy
reports nothing on the changed lines. Not run: the whole suite.
