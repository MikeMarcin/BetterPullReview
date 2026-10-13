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

- AC1: `bpr/core/utf8.hpp` gains `grapheme_end` (UAX #29 extended clusters
  through `utf8proc_grapheme_break_stateful`; an ill-formed byte is its own
  cluster). The editor's scalar stepping is replaced by cluster stepping in
  Left, Right, Backspace, Delete, selection, word movement and double-click.
  `core_suggestion_text_editing` `steps_by_grapheme_clusters` pins Right and
  Left over `a`, e+U+0301, 👍🏽, 👨‍👩‍👧, 🇯🇵 and `z` (columns 1, 2, 4, 6, 8, 9).
  It also pins Shift+Right selecting e+U+0301, Delete taking the thumb with
  its skin tone, Backspace taking a whole flag, family and accented letter,
  undo, a malformed byte stepping alone, and a typed cluster coalescing.
- AC2: `grapheme_width` (the widest `utf8proc_charwidth`, 2 with U+FE0F or a
  regional-indicator pair, at least 1) drives every column. `text_clusters`
  and `suggestion_text_editor::cluster_span` expose that column model.
  `measures_display_columns` pins 漢字, fullwidth Ａ and `a漢` followed by a
  tab (widths 9, 5 and 5), byte offsets mapped to columns, the spans of a
  wide cluster and of a tab, and a combining mark adding no column. It also
  pins Up into a wide cluster stopping at its start while Down keeps its
  goal, and a box copy taking whole clusters.
- AC3: `classify_word` maps L*, M*, N* and Pc to word, Zs and space and tab
  to space, and everything else to non-word. `moves_by_unicode_words` pins
  Ctrl+Right stops across `foo_bar «café» 漢字。x—y 12`, Ctrl+Left, a
  double-click taking `café` with its mark, `。` alone and 漢字 whole, and an
  emoji not being a word character.
- AC4: `suggestion_editor::paintEvent` draws by `core::text_clusters`: one
  shaped cluster per cell, a wide cluster centred in two cells (narrowed if
  a fallback face is wider than both), and U+240D for a lone CR. `point_at`
  takes the nearer edge of a wide cluster, and the accessible character
  rectangle covers both cells. `qt_suggestion_editor_1_0` and `_2_0`
  `paints_clusters_in_their_cells` compare offscreen renders. 漢 and Ａ keep
  their ink inside columns 0 to 2 and reach into the second cell. The `x`
  after 漢 lies in column 2. U+0301 adds ink only inside its base's cell.
  The offscreen sample is `build/dev/tests/adapters_qt/suggestion-{1_0,2_0}/suggestion-editor-graphemes.png`
  (a build output, not committed). It shows CJK, fullwidth, emoji, a flag, a
  combining mark, a tab after wide text and ␍ without overlap. Offscreen
  renders are layout evidence only. No owner look is parked, because the
  geometry is pinned by the pixel tests. An owner glance at real emoji and
  CJK fallback faces on the desktop remains advisable at the next attended
  session that touches this pane.
- AC5: utf8proc is included only by `src/core/src/utf8.cpp`. The earlier
  box, virtual-space, undo, line-ending and indentation cases pass unchanged
  in `core_suggestion_text_editing`. `core_suggestion_draft`,
  `core_suggestion_applicability`, `qt_review_cycle_view_1_0` and `_2_0`, and
  `application_review_cycle_desktop_public_path` pass.
- Gates: `core_boundary_contract`, `core_dependency_boundary` and
  `architecture_boundaries` pass in dev and core-only. clang-format 22 is
  applied, clang-tidy 22 reports no finding on changed lines, and the
  documentation gate passes.
