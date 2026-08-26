---
id: BPR-0455
title: Show line numbers in the source panes, on by default
status: Done
rfc: RFC-BPR-0005
depends_on: []
---

# Show line numbers in the source panes, on by default

Owner direction from the [[BPR-0414]] verdict on 2026-08-25, verbatim:

> There are no line numbers in the source panes so I cannot tell which is line
> 42, there should be line numbers on by default (toggle off with view menu
> option).

This is the first half of why that batch was rejected, and it is the sharper
half: the batch asked the owner to confirm that a drawer thread is
"unmistakably associated with right-side line 42", against a screenshot in
which **no line number is visible anywhere**. The criterion could not be
judged, so the honest answer was reject.

## Known before starting

- `src/adapters/qt/src/diff_workbench.cpp` has no line-number rendering at all —
  no gutter, no margin numbers. The `source_pane` derives from `QPlainTextEdit`
  and paints change boundaries and the conversation anchor marker in its
  `paintEvent`; a number column is new work, not a toggle over something
  present.
- The panes are natural-layout with no synthetic alignment rows, so a pane's
  displayed line numbers are its own document's numbering. They must not be
  renumbered to line up with the other side, which would invent a coordinate
  the diff model does not have.
- The lower detail pane and the `DiffLinkMap` measure against pane geometry. A
  number column changes the text viewport's left edge, so
  `measured_alignment_axis` and the ribbon endpoints have to be checked, not
  assumed — [[BPR-0208]] records a ribbon meeting the pane in the wrong place
  for exactly this class of reason.
- **There is no menu bar.** `workbench_window` creates one `QMenu`
  (`ReviewControlsMenu`) reached from a tool button; `menuBar()` is never
  called. So "view menu option" is a second decision, not a given: either add a
  real menu bar with a View menu, or put the toggle in the existing controls
  surface. Resolve that before building, and record which was chosen and why.

## Required final state

- Both source panes show their own document's line numbers, on by default.
- The setting is toggleable, discoverable where the owner expects it, and its
  state is semantic core state rather than a widget-local flag.
- Ribbons, wedges, the shared scrollbar, the conversation anchor marker, and
  the lower detail pane all still land where they did, with numbers on and off.
- The rendered evidence shows a legible number beside the anchored line, so the
  [[BPR-0414]] criterion becomes judgeable.

## Verification

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'diff_workbench|workbench_window|thread_map|composed_github'
```

A rendered check is required: the claim is that a reviewer can read the line
number, and a passing signal test does not show that a number was painted.

## Done — 2026-08-26

Both panes number their own lines, on by default.

### Four semantics were already fixed; the rest was designed here

Stated carefully, because the first version of this record said "never a
missing feature, an undrawn one" and that reads as though there was nothing to
decide. There was. What existed, and had for two RFCs, is four constraints:

| already there | where |
|---|---|
| `code_gutter_background`, `code_line_number_text` roles | `src/core/include/bpr/core/appearance_model.hpp:48-49` |
| concrete values in Light, Dark, and high-contrast | `src/adapters/qt/src/appearance_profile.cpp:79,128,177` |
| a contrast rule validating that exact pair in every profile | `src/core/src/appearance_contrast.cpp:319-320` |
| "User-visible line numbers may be one-based" | `RFC-BPR-0004:216` |
| "Do not mutate source line numbers to match the other pane" | `RFC-BPR-0004:389` |
| the whole palette already reaching the workbench | `diff_workbench::adopt_appearance` |

So the appearance system has been checking the readability of line-number text
against a gutter, in every theme, for text nothing ever drew. That is the
[[BPR-0180]] pattern in a new place — a surface specified, coloured, and
validated, but never constructed.

Four constraints are not a design, and everything about the gutter itself was
decided here, unspecified anywhere:

- a child widget with `setViewportMargins` reserving its width, rather than
  painting inside the existing viewport — which is what keeps BPR-0414's
  conversation marker beside the code instead of behind the numbers;
- width from the document's digit count rather than from the visible rows, so
  the code does not shift sideways while scrolling;
- a two-digit floor, six pixels of padding either side, right-aligned and
  vertically centred;
- falling back to `QPalette::Window`/`WindowText` when no appearance profile has
  been adopted;
- recomputing on `FontChange`, because a different font is a different digit
  width;
- scrolling the gutter with the viewport rather than repainting it.

Those are the decisions a reviewer should be looking at, and the earlier
phrasing pointed attention away from them — toward "it was already specified",
when the specified part was the easy part.

### What was built

A `gutter_area` child widget per pane, with `setViewportMargins` reserving its
width. The width follows the document's digit count, not the visible rows: a
gutter that grew while scrolling would shift the code sideways under the
reader. Numbers are one-based on screen and zero-based everywhere inside.

The geometry risk was real and turned out to be already handled. The scout
flagged that a narrower text viewport could bring a horizontal scrollbar in
sooner, and a scrollbar takes height from the viewport without taking any from
the link map — which is [[BPR-0208]]. But `declare_link_map_band` already
*measures* the band from the live viewport instead of inferring it, precisely
for that reason, and `resizeEvent` already re-declares it. Nothing needed
changing, and the 23-test geometry matrix proves it rather than the argument
doing so.

BPR-0414's four-pixel conversation marker needed no change either: its rect is
viewport-relative (`QRect{1, …, 4, …}`), so it followed the new left edge
instead of ending up behind the numbers.

### The test found a bug in the test

The first version of the rendered check passed against panes that held **one
block**. The workbench admits a result only for its exact current loading
context, and the test published without calling `set_loading`, so the result
was correctly refused and every assertion ran against an empty document. The
suite now asserts the panes actually hold the fixture before measuring
anything.

A negative run then found a second weak check: `distinct_colors > 1` sampled a
strip of the *pane*, and the pane's frame border alone supplied a second
colour, so an empty gutter looked painted. It now samples the gutter's own
raster.

### Verification

| claim | command | proves it | result |
|---|---|---|---|
| the panes hold real source before anything is measured | `ctest --preset dev -R 'qt_diff_workbench'` | block count > 1 after a context-matched publish | pass, 3/3 scales |
| numbers are painted, not just reserved | same | the gutter's own raster has more than one colour | pass |
| the code moved aside rather than being drawn over | same | the gutter's right edge is at or left of the text viewport's x | pass |
| the gutter is sized to the document | same | 2, 3, and 4 digit documents step by exactly one digit of width each | pass |
| numbers follow the document | same | scrolling changes the rendered gutter | pass |
| neither pane is renumbered to match the other | same | identical at the top, different past a one-sided block | pass |
| no geometry moved | `ctest --preset dev -R 'diff_workbench\|workbench_window\|composed_github\|thread_map\|github_conversation\|measured_alignment\|link_map\|comparison_scroll'` | ribbons, band, marker, scroll controller, widget counts, three window scales, the public-path courses | 23/23 pass |
| formatting | `cmake --build --preset dev --target coding-format-check` | 425 files | pass |
| documentation | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | pass |

### The tests fail when the claim is false

- reserving the gutter and painting nothing fails *"the gutter has numbers
  painted in it rather than a reserved but empty strip"* — and it was this run
  that exposed the frame-border weakness above, before the strengthened check;
- a fixed-width gutter that ignores the digit count fails *"a longer document
  reserves a wider gutter"*.

### Not done here

The toggle. The owner asked for line numbers **and** a view-menu option to turn
them off, and only the first half is here. A Qt-local boolean would satisfy the
sentence while violating both the adapter boundary and RFC-BPR-0005's
projection rule, so the toggle is a core-intent, durable-field, SQLite-migration
slice — and this application has no menu bar at all, which makes "view menu"
its own decision. [[BPR-0457]] owns both, and the split is recorded rather than
quiet.
