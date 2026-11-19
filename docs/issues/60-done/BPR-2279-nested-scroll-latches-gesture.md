---
schema: "implement-rfc.card.v1"
id: "BPR-2279"
title: "A scroll gesture stays with the nested scroller it began in"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2277--BPR-2279--424d9c5fc0e9.json","found_by":"owner attended review, 2026-11-19, accepting BPR-2273","related_cards":["BPR-2271","BPR-2273"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"80b7bb62d","evidence":{"kind":"command","detail":"qt_nested_scroll_latch and qt_conversation_cards_view: a gesture begun in a nested field keeps it to its end through updates and momentum; the outer area and the drawer do not move"}},{"criterion":"AC2","outcome":"pass","revision":"80b7bb62d","evidence":{"kind":"command","detail":"qt_nested_scroll_latch: the next gesture at the end moves the outer area (and the drawer in qt_conversation_cards_view); a vertical gesture over a sideways-only code line moves the outer area"}},{"criterion":"AC3","outcome":"pass","revision":"80b7bb62d","evidence":{"kind":"command","detail":"nested_scroll_latch is the one application filter; the drawer BPR-2273 forwarding asks nested_scroll_latch::delivering()"}},{"criterion":"AC4","outcome":"pass","revision":"80b7bb62d","evidence":{"kind":"command","detail":"phased Begin/Update/Momentum/End events through QWindowSystemInterface; without the latch the course fails (outer 210)"}},{"criterion":"AC5","outcome":"pass","revision":"80b7bb62d","evidence":{"kind":"command","detail":"qt_nested_scroll_latch, qt_conversation_cards_view, application_comment_edit_contract, application_conversation_read_public_path, qt_suggestion_editor_*, qt_workbench_window_scale_100, architecture_boundaries, coding-format-check, tidy on the new file, verify_documentation"}}]
---

# BPR-2279: A scroll gesture stays with the nested scroller it began in

## Observed

Owner, 2026-11-19, accepting [[BPR-2273]]: "momentum scrolling that reaches
the end of a nested scroll window won't scroll the outer window without an
additional gesture. In fact that appears to be exactly how it works in the
chrome browser."

## Acceptance

- AC1. A wheel or trackpad gesture that begins in a nested scroller (a long
  draft editor, a code block or suggested-change diff in the drawer, a
  report) stays latched to it through its momentum phase; reaching its end
  does not pass the rest of the gesture or its momentum to the outer area.
- AC2. A new gesture begun at the inner scroller's end scrolls the outer
  area. A horizontal-only inner scroller passes vertical deltas through.
- AC3. One shared helper latches every nested scroller; the per-widget wheel
  forwarding of BPR-2271 and BPR-2273 routes through it.
- AC4. Qt tests synthesize phased wheel events (begin, update, end, momentum).
- AC5. Focused tests, format and the documentation gate pass.

## Cause

Qt latches a phased gesture to the widget that took its first movement, but
the drawer filtered every wheel over the open card and, once a long field
reached its end, called its own `wheelEvent` (BPR-2273), so the rest of the
gesture and its momentum scrolled the drawer. Without that filter Qt still
chained: a plain `QScrollArea` with a `QPlainTextEdit` in it scrolled the
outer area 210 px from the field's momentum in the new course.

## Fix

`nested_scroll_latch` (`bpr/qt/nested_scroll_latch.hpp`), one application
event filter installed by the application's `main` and by the comments
drawer. For a spontaneous phased wheel event that begins inside a scroll area
nested in another, the gesture's first movement picks its scroller: the
innermost one that can still move that way, else the outermost, which
decides for itself (the drawer still moves the code with it). A scroller
with no range on the gesture's axis, such as a code block or suggested
change that only scrolls sideways, is passed over. Every later event of the
gesture, momentum after its end included, is sent to that scroller's
viewport alone; what it cannot take is dropped. The next gesture chooses
again. A mouse wheel's unphased notches, events widgets send themselves and
a scroller not nested in another are left to Qt. The drawer's BPR-2273
forwarding asks `nested_scroll_latch::delivering()` and leaves a latched
event with the field.

## Verification

- `qt_nested_scroll_latch` (new): phased events through
  `QWindowSystemInterface` over a field in a `QScrollArea` and a sideways-only
  code line. A gesture near the field's end scrolls it to the end and its
  updates and momentum, and momentum after its ScrollEnd, leave the outer
  area at 0; the next gesture moves the outer area 90 px and keeps it when
  it turns back; a gesture the field can take moves only the field; a
  vertical gesture over the code line moves the outer area; a sideways one
  stays with the code line to its end; a mouse wheel at the field's end
  still scrolls the outer area. Without the latch it fails at "the rest of
  the gesture and its momentum stay with the field" (outer 210).
- `qt_conversation_cards_view` `test_a_long_draft_grows_then_scrolls`: in
  the drawer, a phased gesture from two lines above the long draft's end
  scrolls it to the end and its momentum never reaches the drawer; the next
  gesture is the drawer's. The unphased BPR-2273 checks pass as before.
