---
schema: "implement-rfc.card.v1"
id: "BPR-0749"
title: "The seam rule sits in the boundary column of the splitter hot zone"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P3"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"f2c49e12","found_by":"attended-prep-2026-09-10 d23","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"9298ad8d-c3a2-4b4d-b58e-ba18a3daa3f5"}
---

# The seam rule sits in the boundary column of the splitter hot zone

The owner, 2026-09-12, deciding `BPR-0739-seam-hover` in the v009 attended
session (decision `9298ad8d`, 16:05:46Z), in the note beside his selection:

> We don't need it anymore now that the cursor is fixed, it wasn't drawn in
> the right location anyways.

The first clause closed [[BPR-0739]]'s hover question and, later the same
day, [[BPR-0739]] itself. **The second clause was never answered.** It is not
about cursor delivery; it is about where the line is drawn, and it survives
both the highlight being dropped and the cursor being reported fixed. This
card carries it so it is not lost behind a parked diagnosis.

## What the drawing does

`workbench_splitter_handle::paintEvent` draws one rule in `QPalette::Mid`
at the column `rule_edge()` returns
(`src/adapters/qt/src/workbench_window.cpp:6292-6301`, the paint at
`:6303-6312`): column `0` for the files and repositories handles, column
`width() - 1` for the conversations handle. The rule is therefore at the
**extreme edge** of the hot zone, not down its middle. Aiming at the visible
line puts the pointer on the boundary, and one pixel past it is the
comparison's scroll-bar trough on one side or the drawer on the other. That
is a fair reading of "it wasn't drawn in the right location", and it is a
geometry question rather than a colour one.

## The width, now measured

The two numbers the predecessor cards quote are both right, for different
styles, and the discrepancy [[BPR-0746]] recorded as open is closed here.
Measured directly against the shipped Qt 6.10.3 under
`QT_QPA_PLATFORM=offscreen`, reading `PM_SplitterWidth` off each style
`QStyleFactory` offers:

| Style | `PM_SplitterWidth` |
|---|---|
| macOS | **7** |
| Windows | 4 |
| Fusion | 4 |

`QApplication::style()->objectName()` under the offscreen platform is
**`fusion`**, and a default-constructed `QSplitter` there reports
`handleWidth() == 4`.

Two things follow.

- The **"seven pixels"** of [[BPR-0739]]'s d17 diagnosis and of [[BPR-0746]]
  is the correct figure for the shipped macOS bundle. The `handleWidth=4`
  that d22 read out of `qt_workbench_window` is Fusion's value, not a
  contradiction of it.
- More usefully: **the Qt suite does not exercise the style the product
  ships.** Every geometric assertion `qt_workbench_window` makes about this
  handle is made against a four-pixel Fusion handle while the signed bundle
  presents a seven-pixel macOS one. Nothing currently pins that, so a change
  that is correct at four pixels and wrong at seven passes.

## Scope

- Decide where the seam rule belongs relative to the hot zone, and say why:
  down the middle of the handle, or kept on the boundary with the hot zone
  widened around it, or `setHandleWidth` taken under explicit control so the
  visible line and the aimable zone are one thing the product owns rather
  than two the style decides.
- Whatever is chosen must stay inside the accepted asset: the design draws
  the drawer's edge as **one pixel** of `--rule` (`.files` `border-right`,
  `.cmts` `border-left`). A wider *hot zone* is not a wider *rule*.
- Pin the style the assertion is made under. A geometric check that runs only
  under Fusion is not evidence about the shipped bundle; either drive the
  handle width explicitly so both styles agree, or assert against
  `PM_SplitterWidth` read from the style rather than against a literal.
- Do **not** reopen cursor delivery here. [[BPR-0739]] is Done on the owner's
  confirmation and [[BPR-0746]] holds the undischarged hypotheses; if the
  arrow ever returns under a resting pointer that is BPR-0746's business, not
  this card's.
- Owned paths: `src/adapters/qt/src/workbench_window.cpp` (the handle at
  `:6285-6312` and the splitter at `:6258-6290`),
  `src/adapters/qt/include/bpr/qt/workbench_window.hpp:199-208`,
  `tests/adapters_qt/workbench_window_tests.cpp`.

## Acceptance

- The rule's column relative to the hot zone is decided and recorded here
  with its reason, and the drawing matches the decision.
- A Qt check pins the relationship between the rule's column and the handle's
  width in a way that does not silently assume Fusion's four pixels; it must
  be able to fail if the shipped macOS width of seven changes the answer.
- `qt_workbench_window_scale_{100,125,200}` and `qt_composed_github_scale_100`
  pass.
- The rule stays one pixel wide and inside the accepted asset.
