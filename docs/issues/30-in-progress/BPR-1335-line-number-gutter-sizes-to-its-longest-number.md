---
schema: "implement-rfc.card.v1"
id: "BPR-1335"
title: "The line-number gutter sizes to the longest number it shows"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1335--BPR-1335--645a64497526.json","related_cards":["BPR-0455","BPR-1290","BPR-1212","BPR-1264"]}
---

# BPR-1335: The line-number gutter sizes to the longest number it shows

## Owner note

Owner, chat, 2026-10-08, verbatim: "Our line numbers pane looks to be winder
then necessary. It should size to the longest line number".

## Scope

The code panes' line-number gutter (`source_pane` in
`src/adapters/qt/src/diff_workbench.cpp`): the two split panes and the one-pane
(unified) view.

- The number column is as wide as the longest line number the pane shows:
  its digit count times the widest digit advance of the pane's font, plus a
  small pad on each side.
- The thread-marker and "+" column (BPR-1290, BPR-1212) keeps its own width
  to the right of the numbers. It is not narrowed.
- The width is recomputed when the document, the font or the scale changes.
  It does not change while scrolling.

## Acceptance

- `BPR-1335:cause`: the card records why the gutter was wider than its numbers.
- `BPR-1335:width`: at 1.0x, 1.25x and 2.0x, 9-, 99-, 1557- and 100000-line
  files give a gutter of exactly digits × widest digit advance + padding, with
  the marker column unchanged when it is on.
- `BPR-1335:control`: the same test fails against the code before the change.
- `BPR-1335:stable`: the width holds while scrolling the longest file to its
  end, the widest number is painted whole, and a font change resizes the
  gutter.
- `BPR-1335:rows`: the diff workbench rows pass at all three scales.

## Cause

`source_pane::gutter_width()` sized the number column as
`2 × 6 px + advance('9') × max(digits, 2)` in a split pane, and
`2 × (2 × 6 px + advance('9') × max(digits, 4))` in one pane. Four things
made it wider than its numbers:

1. **Padding.** Six pixels either side of the numbers, twelve in all. With
   the marker column on (it always is in the application), the number's
   right edge sat another two pixels from the marker box.
2. **Digit floors.** A split pane reserved at least two digits, so a file of
   nine lines or fewer got a column twice the width of its numbers. One pane
   reserved at least four digits per column.
3. **One digit width, not the widest.** It measured `'9'` only. With a
   monospace code font that is every digit, so this did not widen anything;
   with a proportional font it could clip.
4. **A staged file kept the empty pane's gutter.** Files over 4096 lines on
   both sides together are filled into detached documents and adopted with
   `setDocument` (BPR-1264). That emits no `blockCountChanged`, so nothing
   recomputed the width: a 100000-line file kept the two-digit width of the
   empty pane (28 px) until something else, such as the marker column
   toggling, recomputed it. This one made the gutter too narrow, not too
   wide, and clipped six-digit numbers.

The marker and "+" column (22 px, BPR-1290 and BPR-1212) was not the cause.
It stays the same.

## Change

- The number column is the longest number's digits times the widest digit
  advance of the pane's font, plus 4 px either side. No digit floor.
- One pane: each of the two columns is the larger of that and the width of
  its `base` / `head` heading, plus the same padding. Its longest number is
  the larger of the last base and head numbers, not the row count.
- The width is cached and recomputed when the document is replaced (including
  staged adoption), the block count changes, or the font, device pixel ratio,
  unified rows, marker column or number visibility changes. The layout is
  touched only when the width or heading row actually changed. Scrolling
  recomputes nothing.
- A file that ends in a newline still numbers the empty line after it, as
  VS Code does and as BPR-1291's scroll-past-end course expects. That number
  counts toward the width. A 99-line file ending in a newline therefore shows
  "100" and gets three digits.

## Open question for the owner

GitHub does not number the empty line after a final newline. This app does.
Dropping that number would save one digit for files of 9, 99, 999 … lines
that end in a newline. It would also change the scroll-past-end behaviour,
so it was not done here.
