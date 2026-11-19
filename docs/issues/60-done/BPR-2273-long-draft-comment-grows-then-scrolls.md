---
schema: "implement-rfc.card.v1"
id: "BPR-2273"
title: "A long draft comment grows to fit, then scrolls with the wheel and trackpad"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2271--BPR-2273--d45a84d10eb3.json","found_by":"owner, 2026-11-18","related_cards":["BPR-1212","BPR-1290","BPR-1775"]}
---

# BPR-2273: A long draft comment grows to fit, then scrolls with the wheel and trackpad

## Observed

Owner, 2026-11-18: "A long draft comment neither scrolls with 2 finger
MacBook gestures (scroll wheel) nor expands to fit the contents. It has to
do one or the other".

## Acceptance

- AC1. The drawer's comment editors grow with their text up to about 60%
  of the drawer's height, then scroll inside.
- AC2. A wheel or two-finger scroll over such an editor moves it, by angle
  or by pixel delta; at its end the drawer takes the rest. No parent
  swallows it.
- AC3. A closed long draft is never clipped silently: it clamps with an
  ellipsis and opens in full.
- AC4. Qt tests: a long editor grows to its maximum and then has a vertical
  range; synthetic wheel events with angle and with pixel delta move it; a
  closed long draft is not clipped.
- AC5. Focused tests, format and tidy on the changed files, and the
  documentation gate pass.

## Cause

The composer (`threadDraftText`) stopped growing at a fixed 220 px. The
drawer filters every wheel event over the open card and moved itself for
any vertical one, so a field taller than that could never be scrolled by
wheel or trackpad. Only the caret reached its hidden lines. The comment
editor (`threadCommentEditText`, BPR-1775) had a fixed eight-line height.

## Fix

The drawer gives the open card a field share of 60% of its viewport on open
and on resize (`open_thread_card::set_field_maximum`). The composer and the
reply field grow to that share, and the comment editor grows from its eight
lines to it; past it they show a scroll bar and scroll. Before moving
itself, the drawer's wheel filter asks `open_thread_card::scroll_field`. A
field with room that way moves by the wheel's notches, or by a trackpad's
pixels added up to lines (the plain-text bar counts lines). At the field's
end the drawer takes the wheel. The card filters its own fields' viewports
too, which covers the comment editor created after the drawer's filters.
Closed, a long draft clamps to its note's lines and ends in an ellipsis,
and opening it shows all of it. `closed_preview` exposes the words a closed
note paints.

## Verification

`qt_conversation_cards_view`'s `test_a_long_draft_grows_then_scrolls`
covers this. In a 700 px drawer the composer grows from 54 px to 420 px,
its 60% share, and then has a vertical range. An angle-delta wheel and a
pixel-delta-only wheel each scroll it without reaching the drawer. At its
end the wheel goes to the drawer. Closed, the draft's preview ends in
"…" while the note keeps every word. Run with `--render-bpr-2273 DIR` to
write the renders. Also passing: `application_comment_edit_contract`,
`application_conversation_read_public_path`, `qt_workbench_window_scale_100`
and the panel and Discuss tests named in BPR-2271. Format, tidy on the
changed lines and the documentation gate pass.
