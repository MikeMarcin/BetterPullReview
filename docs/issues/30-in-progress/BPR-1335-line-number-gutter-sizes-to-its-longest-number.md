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
