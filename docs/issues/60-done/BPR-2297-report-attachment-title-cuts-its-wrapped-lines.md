---
schema: "implement-rfc.card.v1"
id: "BPR-2297"
title: "A report attachment's title cuts its wrapped lines through"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-2297--BPR-2297--716ce704166a.json","origin":"owner report 2026-11-19 (\"Another instance of an attachment that doesn't fit the title\"), with a photo of the Agent tab; the photo is not kept in the repository","acceptance":["AC-a","AC-b","AC-c"]}
---

# A report attachment's title cuts its wrapped lines through

## What happened

In the Agent tab, an answer shown as a Report attachment card had a title
that wrapped to three lines inside a box about two lines tall: the first
line was cut through its tops and the last through its bottoms, above the
card's "Report" kind line. The owner: "Another instance of an attachment
that doesn't fit the title".

## Earlier instances and why this one escaped

- Look ask-v007 ("Why is your mockup report attachment cutting of text"):
  the row took one height from its first width, so a narrower drawer cut
  the wrapped title. Fixed by giving `report_attachment_row` a height for
  its width.
- A Windows follow-up (the row's comment cites [[BPR-2215]]): a separate
  height sum asked one pixel more than the row was given; the row now
  answers what its own layout does.

Both fixed the *row's* height. Neither touched the title label, which was a
word-wrapping `QLabel` with `setMaximumHeight(2 * lineSpacing + 2)`, taken
before the label was made semibold. A capped wrapping `QLabel` does not
elide: its height for a width is all its wrapped lines, the cap clamps the
box, and the label draws every line centred in the shorter box, so a title
of three or more lines loses the top of its first line and the bottom of
its last. Short titles (a heading, "Explain …") never reached three lines,
so the earlier looks passed. A report without a heading takes its answer's
**whole first line** as its title (`core::report_card_of`), and an answer's
first line is often a paragraph: that is this instance.

## Fix

A shared label, `bpr::qt::line_bounded_label`
(`src/adapters/qt/include/bpr/qt/line_bounded_label.hpp`): at most N whole
lines, wrapped at word boundaries (anywhere in a word wider than the line),
the last ending in "…" when the text goes on. Its height for a width is its
shown lines at its *current* font's line spacing, so it follows the font,
style and scale it is drawn with; its painting draws only whole lines even
in a box a parent made too short. `text()` stays whole (the accessible name
and the report tab's title read it). The report attachment row's title is
one, two lines at most as `docs/design/report-card-v4` specifies; when the
title is shortened, the row's tooltip says it whole. The row was the only
card with a capped wrapping title in `src/adapters/qt/src` (searched for
`setMaximumHeight`/`setFixedHeight` on labels).

## Acceptance

- **AC-a.** A long multi-line title in a report attachment card is either
  fully visible or shortened on whole lines ending in "…", never cut through
  its glyphs, at a narrow and a wide drawer and with a larger application
  font. `test_report_attachment_title`
  (`tests/adapters_qt/report_attachment_title_tests.cpp`), which fails on the
  old row.
- **AC-b.** The same at 1.0, 1.25 and 2.0 scale:
  `qt_report_attachment_title_scale_100/125/200`.
- **AC-c.** The same in macOS's own style and fonts: the test run with
  `QT_QPA_PLATFORM=cocoa BPR_NATIVE_LOOK=1`.

## Evidence

Candidate `6d808722c` (the fix and the test), macOS arm64, dev (Debug);
rebased onto main after BPR-2296 as `5935a2027`, where the three scale rows,
the cocoa run and the documentation gate were repeated and passed.

| Criterion | Check | Result |
| --- | --- | --- |
| AC-a | `test_report_attachment_title` against the old row (only `agent_tab_view.cpp` reverted to `056125b0f`) | Failed, as it should: "long title, narrow drawer: every line of the title is whole, 32 px for 105 px of wrapped lines (7 lines of 15 px)" at every scale |
| AC-a, AC-b | `ctest --preset dev -R qt_report_attachment_title` | 3 of 3 passed (`_scale_100`, `_125`, `_200`, offscreen) |
| AC-c | the test with `QT_QPA_PLATFORM=cocoa BPR_NATIVE_LOOK=1`, dark and light, `QT_SCALE_FACTOR` 1, 1.25 and 2.0 | 6 of 6 passed |
| Look | cocoa renders of the card (dark, 1.0 and 2.0, narrow and wide drawer, larger font), viewed and discarded | Two whole semibold lines, the second ending "word…", the kind line whole below; a short title on one line, unshortened |

Format check passed on the four touched C++ files; clang-tidy passed on
`line_bounded_label.cpp` and the test. `agent_tab_view.cpp` reports two
findings that are on main unchanged (`press_offer`'s parameter names, the
copy of `aChosen`), outside this change. `verify_documentation.py` passed.
The `frontend-design` skill was loaded for this Qt surface.
