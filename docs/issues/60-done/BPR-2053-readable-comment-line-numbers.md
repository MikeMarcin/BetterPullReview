---
schema: "implement-rfc.card.v1"
id: "BPR-2053"
title: "Make comment line numbers readable on their tint"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2053--BPR-2053--4897cf8f1a44.json","source_commit":"9a5a692b4"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9a5a692b4","evidence":{"kind":"command","detail":"test_marked_line_numbers_read_on_their_tint: gutter numbers on thread/draft tints reach 4.5:1 (7:1 high contrast) in all four themes; dark open thread 2.56 to 4.60. Unmarked numbers keep the gutter colour."}},{"criterion":"AC2","outcome":"pass","revision":"9a5a692b4","evidence":{"kind":"command","detail":"test_line_numbers_read_on_the_open_card: draft line button and status words 2.06 to 4.51 in dark, all themes pass; qt_diff_workbench_scale_*, qt_conversation_cards_view and format pass."}}]
---

# Make comment line numbers readable on their tint

Owner report: comment line numbers were hard to read on their background in
dark theme. Gutter numbers kept the plain gutter grey on the thread tint with
the open-thread wash painted over the digits; the open draft card drew its line
and status words in the selection colour.

## Acceptance

- AC1. Diff gutter numbers on commented lines meet the theme text minimum on
  their actual background in every built-in theme.
- AC2. The open card's draft line number and status words meet the same
  minimum.
