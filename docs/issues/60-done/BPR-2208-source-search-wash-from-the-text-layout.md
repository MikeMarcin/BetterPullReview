---
schema: "implement-rfc.card.v1"
id: "BPR-2208"
title: "A source search match's wash covers the match in any font"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win","mac","linux"]
metadata: {"found_by":"BPR-2200's Windows Qt run with Segoe UI 9 pt, checked against main (BPR-2205), 2026-11-15","reservation":"docs/issues/_reservations/BPR-2208--BPR-2208--a84f7e7d87cf.json","related_cards":["BPR-2200","BPR-2205","BPR-2199"],"kind":"font-dependent layout"}
---

# BPR-2208: A source search match's wash covers the match in any font

## Found

`qt_source_search_panel_scale_1` failed on the lane with Segoe UI 9 pt and on
main with no fonts, and passed on main with ctest's fonts and offscreen's
default family:

    FAILED: painted wash spans the match text

On macOS the same check failed with proportional snippet fonts at 100%:
Helvetica 9 (no wash found on the scan line), Arial 9, Times 9, Verdana 9,
Georgia 10, Arial 12 and Helvetica Neue 11.

## Cause

Two faults, one each side.

- **Product.** `source_search_delegate` placed the wash by measuring the text
  before the match as a string of its own
  (`horizontalAdvance(text.left(start))`), then drew the whole snippet with
  `drawText`. The two measures need not agree: the drawn line carries the
  font's kerning and fractional advances across the match's edge, while a
  separately measured prefix does not. That guessed where the match would be.
- **Test.** `painted_wash` read one pixel row, `rect.top() + 3`. The wash is
  centred on the row and as tall as the font plus 2. When the font was
  shorter than the row, that line crossed the wash's rounded corner (a
  narrower run) or missed the wash altogether (Helvetica 9: none found). At
  mid-height instead, the glyphs at the match's ends cover the wash's
  one-pixel border.

## Fix (`1e5ef1e30`)

- The snippet is laid out once with `QTextLayout`, drawn from that layout,
  and the wash is placed from the same layout's `cursorToX` at the match's
  start and end. The match is wherever the drawn text puts it.
- The test measures the wash's widest run over every pixel row of the match
  row, which no corner or glyph can shorten. The tolerance and the expected
  width (the match's advance plus the wash's 2-pixel border) are unchanged.

## Verification

- macOS, 30 combinations with the snippet font forced by a local hook (not
  committed): the system fixed font, Helvetica 9, Arial 9, Times 9, Verdana
  9, Arial 12, Georgia 10, Helvetica Neue 11, Menlo 9 and Courier 11, each at
  100%, 125% and 200%, all pass. A render of the row shows the wash on the
  drawn "needle". `qt_source_search_panel_scale_1/2` pass in `dev`.
- Windows station, Release, Segoe UI 9 pt, run directly with ctest's
  environment: `qt_source_search_panel_scale_1/2` pass (`PAINTED_WASH
  left=133 right=172 advance=38`).
- Linux not run: the change is platform-neutral Qt.
