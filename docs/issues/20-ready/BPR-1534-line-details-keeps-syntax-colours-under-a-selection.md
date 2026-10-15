---
schema: "implement-rfc.card.v1"
id: "BPR-1534"
title: "Line details keeps the syntax colours under a text selection"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-15","owner_note":"Although I see the line details lists syntax highlights on selecting which is a separate bug","governing_rfc":"RFC-BPR-0006","related_rfcs":["RFC-BPR-0028"],"related_cards":["BPR-1334","BPR-1212","BPR-1156","BPR-1533"],"reservation":"docs/issues/_reservations/BPR-1534--BPR-1534--78ec543a573a.json","requested_skill":"frontend-design"}
---

# BPR-1534: Line details keeps the syntax colours under a text selection

The owner reported on 2026-10-15, beside [[BPR-1533]]: "Although I see the
line details lists syntax highlights on selecting which is a separate bug".
The reading is "loses": selecting text in Line details ([[BPR-1334]]) is
expected to repaint the selected glyphs in one colour. What actually happens
is measured offscreen and recorded below before any change.

Governing design: [RFC-BPR-0006](../../rfcs/RFC-BPR-0006-syntax-highlighting-themes-appearance.md),
as for [[BPR-1533]]. The comparison panes keep the syntax colours under a
selection since [[BPR-1212]] v002 (the owner, on v001: "are we meant to lose
all syntax highlighting when we select something?"), and Review Cycles' code
view since [[BPR-1156]] v006.

## Acceptance

- AC1: In Line details a text selection keeps each selected glyph's syntax
  role colour, moved only as far as it must to read at the variant's text
  floor on the selection, in all four built-in variants; plain text and the
  intraline emphasis read the same way.
- AC2: The selection is the same treatment the comparison panes use
  ([[BPR-1533]]): one shared helper, the theme's `code_selection_background`.
  Copy, the gutter, the bands, the placeholder and the horizontal scroll are
  unchanged.
- AC3 (human): The owner accepts the exact frozen candidate's Line details
  with a selection, shown before and after in offscreen Dark and Light renders.

## Implementation

Separate commit from [[BPR-1533]], so either can merge alone. Qt presents;
no core, host or storage change. Renders offscreen only.
