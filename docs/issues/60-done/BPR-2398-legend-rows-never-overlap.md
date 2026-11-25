---
schema: "implement-rfc.card.v1"
id: "BPR-2398"
title: "The findings donut's legend rows no longer stand over each other under the macOS style: a row is one line of its font, laid out by the rect it paints, and the native look of the summary course is a registered row"
status: "Done"
rfcs: []
depends_on: ["BPR-2390", "BPR-2344"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2398--BPR-2398--0886c2deedb5.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"6436832c2","origin":"found in BPR-2390 (its Left as found): under QMacStyle the legend's rows are 22 points tall and stand 14 apart, and qt_agent_report_summary run with BPR_NATIVE_LOOK=1 fails at \"the row is under the one before\", on main too","related_cards":["BPR-2390","BPR-2344","BPR-2331","BPR-2392","BPR-2394"],"related_rfcs":["RFC-BPR-0128"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"],"owner_step":"attended look at the review turn's legend on nested-folders#21 in a frozen tests-on build (fixture agent), Light and Dark: at rest, a count under the pointer, Tab onto a count (see Owner look)"}
acceptance: []
---

# BPR-2398: The findings donut's legend rows no longer stand over each other

## The defect

[[BPR-2390]] left it as found: in the Agent tab, under the macOS style, each
row of the findings donut's legend was 22 points tall and the rows stood 14
points apart, so every row was drawn over the 8 points at the foot of the one
above it.

The cause, measured in a real window's Agent tab (Qt 6.12.1,
`.AppleSystemUIFont` 13, a line of 16): a legend row is a `QPushButton` that
paints its whole rect, and the macOS style gives a push button a layout rect
8 points shorter than the button (nothing off its sides or its top). The
legend laid its rows out by that rect. Fusion gives no such rect, so the
three registered rows of `qt_agent_report_summary`, which run offscreen,
passed, and no registered row drew the legend the way the owner's window
does.

What a person saw (the before renders):

- At rest, a tight legend exactly as tall as its donut (78 points for five
  labels). This is the look the owner accepted in [[BPR-2390]].
- With the pointer on a row, its wash reached into the words above and below.
- With the keyboard on a row, its focus ring ran through the words above it,
  and the row below was drawn over the ring's foot.

## Change

`report_tally_view.cpp` and `report_tally_view.hpp` only.

- **Laid out by the rect it paints.** `report_tally_row` sets
  `Qt::WA_LayoutUsesWidgetRect`, this adapter's rule for a button that paints
  its whole rect (the chips of [[BPR-2392]]). No style's push button rect
  moves a row again.
- **A row is one line of its font** (`report_tally_view::line_height`: the
  taller of the word's font and the count's, never less than 16). It was a
  line and 6 more, which only a style without the rect ever showed.
- **The wash and the focus ring are the row's line.** The wash fills the row;
  the ring's stroke is drawn inside it. Same colours and strengths (the
  window's ink at 6%, 10% pressed; a 1.5 point `Highlight` ring), same left
  and right edges, corners a quarter of the row up to the 5 they had (4 on a
  16 point row).
- Unchanged: the donut, the swatch, the word and the count, their colours,
  their places across the row, the 12 points between donut and legend, the
  one column of counts, the order, the keyboard, the tooltips and the
  accessible names.

### Why one line, and not the 22 the row had

The accepted look is the tight legend, and no row can stand 14 apart and
hold a 16 point line: it held only because each row ran over the next. One
line is the least a row can be and keep its words whole, so it is the nearest
that a legend without overlap comes to the accepted one.

Five labels, native macOS, in points:

| | row | apart | legend | summary |
|---|---|---|---|---|
| Before (accepted at rest) | 22 | 14 | 78 | 88 |
| After | 16 | 16 | 80 | 90 |
| Considered: the attribute alone | 22 | 22 | 110 | 120 |
| Considered: a line and 2 | 18 | 18 | 90 | 100 |

The donut is 78. After, the legend is 2 points taller than its donut, each
line stands 2 points further from the last, and the reply is 2 points taller.
With the attribute alone the legend would stand 32 points taller than its
donut and the reply 32 taller. A line and 2 gives the focus ring a point more
round the words and was the second choice; it moves the accepted legend twice
as far.

At 16 the focus ring meets the tail of a `g` or a `q` without crossing it
(`after/cocoa-macos/*-reply-keyboard-on-second-count.png`).

### What this changes off the Mac

Under a style with no such rect the rows already stood apart, 21 points each
in the offscreen course's font (a line of 15 and 6 more): legend 105, summary
115 for five labels. They are now 16 each: legend 80, summary 90, the Mac's
legend. Windows and Linux therefore show a legend 25 points shorter for five
labels than they did. That legend was never an owner look ([[BPR-2344]]
records offscreen renders and no owner judgement); the Mac's was the accepted
one. Not run on Windows or Linux in this round.

## Acceptance

- **AC1, no row over another, under any style.**
  `qt_agent_report_summary_scale_100/125/200` and
  `qt_agent_report_summary_native_look` › `expect_rows_tile`, in a real
  window's Agent tab at the narrowest and a wide drawer and a 1.6 times font,
  and in `test_the_rows_stand_apart_whatever_rect_the_style_gives_a_button`:
  each row is one whole line of its font, stands exactly under the one
  before, intersects no other row, and the legend is as tall as its rows.
  The second test runs the legend under a proxy style that takes 4 points
  off a push button's top, 8 off its foot and 3 off each side, and first
  shows that two plain push buttons laid out by that style do stand over
  each other. With `WA_LayoutUsesWidgetRect` removed the course fails in all
  four rows and natively at a 100% display, at "a style with a smaller
  button rect, question: the row stands exactly under the one before, at 8
  for 16". Pass.
- **AC2, the native look is a registered row.**
  `qt_agent_report_summary_native_look` (macOS only): the whole course under
  Cocoa, the macOS style, the system font and the display's own scale, every
  window kept off screen. On main 5bc62c938 the same command fails at
  "narrowest drawer, question: the row is under the one before, as wide".
  Pass through ctest; the same binary also passes with
  `BPR_NATIVE_LOOK_SCHEME` light and dark and at a 100% display
  (`QT_SCALE_FACTOR=0.5`).
- **AC3, the pointer and the keyboard stay in their row.**
  `expect_states_keep_to_their_row`, same test, at the default font and 1.6
  times: with each row in turn under the pointer, then holding the keyboard,
  that row's pixels change and no pixel wholly inside any other row does.
  Pass.
- **AC4, the accepted look.** Before and after renders of the composed
  window (below): the donut, swatches, words and counts are where they were
  across the row; the legend's lines stand 16 apart for 14, the legend is 80
  beside its 78 donut for 78, and the reply is 2 points taller at 280 (399
  to 401) and at 360 (363 to 365). The wash and the ring wear their colours
  on one line. Owner look pending.
- **AC5, nothing else moved.** `application_review_report_public_path` (the
  shipped composition on nested-folders#21 with the fixture agent),
  `qt_agent_tone_colours`, `qt_drawer_surfaces_fit_scale_100/125/200`,
  `application_composed_drawer_fit_scale_200` and
  `application_composed_drawer_fit_cocoa`: pass.
  `application_composed_drawer_fit_scale_100` passed once of three runs. The
  two failures are the same wait, "the question is sent: What changed in
  this push?", the 30 second send after the replay's pinned card (the step
  ec0c4e752 touched), long after the course swept the report turn at every
  width; the machine's load average was 30 to 40 with other builds running.
  Not shown to fail on main under the same load: main was not built for it.
- **AC6, gates.** `architecture_boundaries` pass;
  `python3 scripts/verify_documentation.py --root .` DOCUMENTATION_OK;
  `format_check.py` (clang-format 22.1.8) on the four changed C++ files
  pass; `tidy.py` on `report_tally_view.cpp`,
  `agent_report_summary_tests.cpp` and `review_report_public_path.cpp` pass.
  No digest covers a changed file; `src/app` did not change.

Evidence revision: 6436832c2.

## Renders (not committed)

From the application's own composed window: the public-path fixture on
nested-folders#21, Review this pull request answered by the fixture agent,
drawn by `review_report_public_path.cpp` under `BPR_REVIEW_REPORT_LOOK`
through Cocoa with the macOS style, windows kept off screen
(`BPR_NATIVE_LOOK=1`), `BPR_NATIVE_LOOK_SCHEME` light and dark, at the
display's own 200% (no `QT_SCALE_FACTOR`; the `100` in a file's name is that
factor, not the scale). Before is main 5bc62c938 drawn by the same course.
The look now also saves, at 280 and 360, the reply with its second count
under the pointer and with the keyboard on it: the states the defect showed
in.

- `legend-renders/before/cocoa-macos/` and `legend-renders/after/cocoa-macos/`:
  the reply and the drawer at 280, 320, 360, 420 and 640, the window at 360,
  the two states at 280 and 360.
- `legend-renders/side-by-side/`: before on the left, after on the right, at
  280 and 360, at rest and in the two states, Light and Dark.
- `legend-renders/after-at-100-percent-display/cocoa-macos/`: after, natively
  with `QT_SCALE_FACTOR=0.5` (the `50` in a file's name).
- `legend-renders/after/offscreen-fusion/`: after, the same look drawn
  offscreen. Layout evidence for a style without the rect; not what any
  platform draws.
- `legend-renders/considered/`: rows of 16, 18 and 22 from the summary
  course's own window under the native look, Dark, drawn with throwaway
  switches that are not in the change. Not the composed window.

## Left as found

- The attachment row under the summary is a `QPushButton` that paints its
  whole rect too (`report_attachment_row`, `agent_tab_view.cpp`). Under the
  macOS style it is 46 points tall with a layout rect of 34 (4 off its top, 8
  off its foot). With a summary over it, it stands exactly its 10 under the
  summary. Alone (a report that labels no finding) it is drawn 11 under its
  answer where its card puts it 15. The course now judges the place the
  layout gave and prints the difference where there is one
  (`report-attachment-alone: drawn 11 under its answer, laid out 15`); under
  a style without the rect the two are equal and the check is what it was.
  Not changed: the row is another surface's, and moving it changes a look
  the owner accepted.

## Owner look

Pending, not prepared in this round: whether the legend with its lines 2
points further apart reads as the accepted one, and whether the wash and the
ring on one line read right, in Light and Dark.
