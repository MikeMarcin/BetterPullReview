---
schema: "implement-rfc.card.v1"
id: "BPR-1577"
title: "The Overview's Open and All switch is quiet like its row"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner feedback 2026-10-17","owner_note":"This particular Open button on the overview page is ugly. I guess it's mostly because it's highlighted like that and nothing else is.","governing_rfc":"RFC-BPR-0049","related_rfcs":["RFC-BPR-0050"],"related_cards":["BPR-1089","BPR-1337","BPR-1350","BPR-1157"],"reservation":"docs/issues/_reservations/BPR-1577--BPR-1577--1dde60b7fa88.json"}
---

# BPR-1577: The Overview's Open and All switch is quiet like its row

The owner said on 2026-10-17: "This particular Open button on the overview
page is ugly", and then: "I guess it's mostly because it's highlighted like
that and nothing else is." The switch is [[BPR-1089]]'s Open or All beside
the Overview's Files heading, where the counts start (Open by default), next
to the ? count, the Label key and collapse and expand all folders. Its two
halves are plain auto-raise `QToolButton`s. The style draws a checked one as
a light grey block, the only filled control on the row, and grey on Dark.
Open carries its mark beside the word and All has none, so the two halves
differ in height and in the size of their words.

Governing design: the Overview counts of
[RFC-BPR-0049](../../rfcs/RFC-BPR-0049-label-counters-overview-settings.md)
and the marks of
[RFC-BPR-0050](../../rfcs/RFC-BPR-0050-feedback-ux-label-icon-design.md).
The switch's behaviour, names and keyboard route stay as [[BPR-1089]]
accepted them; only how it is drawn changes.

## Acceptance

- AC1: Open and All are drawn by the page, the same height as the row's
  icon buttons and in the same type size, with no fill or border at rest.
  The chosen half is its word in the text colour and semibold over a
  two-pixel accent underline; the other is its word in the secondary text
  colour. Each half is as wide as its word in semibold, so choosing moves
  nothing. Open keeps the round open mark, at the height of its word and
  centred on it, because the mark is what Open counts.
- AC2: In the four built-in themes the words read at 4.5:1 or more against
  the page and the underline at 3:1 or more; the chosen half differs from the
  other by its underline. A render check covers both states in every theme,
  with a negative control that fails on the old grey block.
- AC3: Behaviour is unchanged: object names, accessible names and
  descriptions, tooltips, Tab order, Space and the dispatched mode are as
  before, and the keyboard focus ring shows on the focused half.
- AC4: Focused Qt tests, one full `ctest --preset dev` on the merged tree,
  format, tidy and the documentation gate pass.
- AC5 (human): The owner accepts the frozen candidate, Light and Dark: "Open
  and All read as a quiet part of the Files row, with the chosen one clear".

## Implementation

Qt presents: a page-local checkable tool button paints its word, mark and
underline from the page palette; the indicator colour is the window's focus
indicator floor over the page (`focus_ring_colour`). No QSS, no new state,
no core or host change. The UX redesign of the drawers, the Agent tab and
Compose review and the loading-states card are not touched. Never drive or
capture the owner's desktop; the owner checks the frozen build themselves.
