---
id: BPR-0455
title: Show line numbers in the source panes, on by default
status: Ready
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
