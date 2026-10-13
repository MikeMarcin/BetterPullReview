---
schema: "implement-rfc.card.v1"
id: "BPR-1438"
title: "Step, measure and paint the suggestion editor by grapheme cluster and display width"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1434"]
priority: "P2"
platforms: []
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit","related_cards":["BPR-1156","BPR-1432","BPR-1434"]}
---

# Step, measure and paint the suggestion editor by grapheme cluster and display width

The multi-caret suggestion editor, which is also the Review Cycles file pane,
treats one Unicode scalar as one column and one caret step
([suggestion_text_editing.cpp](../../../src/core/src/suggestion_text_editing.cpp)),
and its painter puts each scalar in a fixed cell the width of `M`
([suggestion_editor.cpp](../../../src/adapters/qt/src/suggestion_editor.cpp)).
So CJK text overlaps its neighbours and a combining accent gets a cell of its
own. Left, Right, Backspace and Delete also split flags, ZWJ emoji,
skin-tone modifiers and combining sequences, and every non-ASCII byte counts
as a word character. The owner approved using utf8proc, which core already
links ([[BPR-1434]]), instead of hand-written tables.

## Acceptance

- AC1: Caret movement, selection and deletion step by extended grapheme
  cluster (`é` as `e`+U+0301, 👍🏽, 👨‍👩‍👧, 🇯🇵): a cluster never splits, and a
  malformed byte is a cluster of its own.
- AC2: Columns are display columns. A wide cluster (漢, fullwidth Ａ, emoji)
  takes two columns and a combining mark adds none. Tabs keep their stop rule
  after wide text. A column inside a wide cluster resolves to its start, as a
  column inside a tab does.
- AC3: Word movement and double-click selection treat Unicode letters, marks,
  numbers and connector punctuation as word characters, and treat other
  punctuation and symbols as non-word.
- AC4: The Qt painter places each cluster at its column, gives a wide cluster
  two cells and draws a cluster's scalars together, so combining marks sit on
  their base and wide clusters do not overlap. A lone CR is still shown as
  U+240D.
- AC5: The editor's other behaviour is unchanged: box selection, virtual
  space, undo and line endings. The utf8proc calls stay private to
  `src/core/src/utf8.cpp`.

## Evidence

Recorded at completion.
