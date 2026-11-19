---
schema: "implement-rfc.card.v1"
id: "BPR-2271"
title: "Suggested-change diffs in the Comments drawer scroll sideways as code blocks do"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2271--BPR-2273--d45a84d10eb3.json","found_by":"owner, 2026-11-18","related_cards":["BPR-1212","BPR-1564","BPR-1944"]}
---

# BPR-2271: Suggested-change diffs in the Comments drawer scroll sideways as code blocks do

## Observed

Owner, 2026-11-18: "Suggested changes diffs in the comments drawer should
scroll like code blocks there do".

## Acceptance

- AC1. A suggested-change diff in the Comments drawer does not wrap or
  elide a long line: its rows scroll sideways inside their own box, one
  horizontal scroll per diff, in the drawer's code scroll bar style.
- AC2. A sideways wheel or Shift-wheel moves the diff, a vertical wheel
  moves the drawer, and Left and Right scroll it when it has focus, as a
  wide code block in a post does.
- AC3. A Qt test: a suggestion whose line is longer than the drawer gets a
  horizontal scroll range, and the card stays the drawer's width.
- AC4. Focused tests, format and tidy on the changed files, and the
  documentation gate pass.

## Cause

A posted suggestion in a post's reader already scrolls sideways with the
post, as a fenced block does. The diff a suggestion draft shows under its
field (`threadDraftChange`, BPR-1212) was painted as one widget: every row
was elided to the box's width, so a long line ended in "…" and could not be
read.

## Fix

`suggestion_change_rows`, a `QAbstractScrollArea` inside the painted box,
draws the rows unwrapped and unelided, their fills running the whole row.
Its horizontal scroll bar is the platform's, as on the fenced field above it
and in a post's reader. A sideways wheel, Shift-wheel or trackpad moves it,
by pixel delta where there is one. A vertical wheel is ignored, so the
drawer keeps it, as a post's reader in place does. With focus, Left, Right,
Home and End scroll. The rows take focus only when they are wider than the
box, and the box's hairline turns the signal colour while they have it. The
box counts the bar's height when the bar takes room, as the reader does.

## Verification

`qt_conversation_cards_view`'s
`test_a_suggested_change_scrolls_sideways_in_its_box` covers this. A
suggestion whose line is longer than a 320 px drawer gets a horizontal
range. The card and the diff stay the drawer's width. A sideways wheel moves
the diff and a vertical one does not. Right, End and Home scroll it. A short
change has no range and no tab stop. Run with `--render-bpr-2271 DIR` to
write the offscreen render. Also passing: `qt_github_conversation_panel`,
`qt_workbench_window_scale_100`, `qt_workbench_conversation_reveal`,
`qt_discuss_comment_bodies` and `qt_public_preview_conversation`. Format and
tidy pass on the changed lines; tidy's remaining findings in these files are
on older lines. The documentation gate passes.
