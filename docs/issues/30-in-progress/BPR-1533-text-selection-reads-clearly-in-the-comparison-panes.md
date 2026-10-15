---
schema: "implement-rfc.card.v1"
id: "BPR-1533"
title: "A text selection in the comparison panes reads as clearly as in Line details, on every line kind"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-15","owner_note":"At least on dark theme I'm finding it quite hard to see a selected block (eg a word double clicked) in the middle of a change diff on the main pane. Partially because of the line highlight color, but I don't think that's the only cause. Actually it's not so easy to see even on unchanged lines. The single line details pane shows it much more clearly for some reason.","governing_rfc":"RFC-BPR-0006","related_rfcs":["RFC-BPR-0028","RFC-BPR-0034"],"related_cards":["BPR-1212","BPR-1156","BPR-1334","BPR-1503","BPR-1534"],"reservation":"docs/issues/_reservations/BPR-1533--BPR-1533--5c45415baeca.json","requested_skill":"frontend-design"}
---

# BPR-1533: A text selection in the comparison panes reads as clearly as in Line details, on every line kind

The owner reported on 2026-10-15: "At least on dark theme I'm finding it
quite hard to see a selected block (eg a word double clicked) in the middle of
a change diff on the main pane. Partially because of the line highlight color,
but I don't think that's the only cause. Actually it's not so easy to see even
on unchanged lines. The single line details pane shows it much more clearly
for some reason."

Governing design: [RFC-BPR-0006](../../rfcs/RFC-BPR-0006-syntax-highlighting-themes-appearance.md)
(the theme's `code_selection_background` role and the code canvas's text
floor), under the review workbench of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md). The
panes' selection treatment is recorded in the
[workbench specification](../../ux/workbench-specification.md). The companion
report, that Line details loses the syntax colours under a selection, is
[[BPR-1534]]; the two must agree on one treatment.

## Acceptance

- AC1: In both comparison panes (two-pane and one-pane), a text selection
  (a double-clicked word, a drag, Shift+Up/Down) is drawn over everything the
  line already wears: the unchanged canvas, the added and removed fills, the
  intraline emphasis and the selected line's band. Measured offscreen in
  Light, Dark, High Contrast Light and High Contrast Dark, the selection's
  fill differs from each surrounding line background it sits in at least as
  much as Line details' selection does today, focused and unfocused.
- AC2: Selected glyphs keep their syntax role colours, moved only as far as
  they must to read at the variant's text floor (4.5:1, 7:1 for high
  contrast) on the selection; no hard-coded colour, no new setting; the
  colour comes from the theme's `code_selection_background`.
- AC3: Copy, Find's current match (no selection stacked on it), the line
  band, gutter and keyboard and mouse selection behaviour are unchanged.
- AC4 (human): The owner accepts the exact frozen candidate, shown with
  offscreen Dark and Light before and after renders of a double-clicked word
  on unchanged, added, removed and current lines, and Line details beside it.

## Implementation

Load frontend-design:frontend-design before choosing the treatment. Qt
presents: the change is the Qt adapter's paint of the panes' text selection,
shared with Line details; no core, host, provider or storage change unless
the contrast contract needs a new pair. Never drive or capture the owner's
desktop; renders are offscreen. Visual judgement goes through a fresh short
attended-review session and a frozen build.
