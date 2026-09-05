---
id: BPR-0520
title: "Qt: the defect action row clips in the conversations drawer at 100 %"
status: Done
rfc: RFC-BPR-0010
depends_on: [BPR-0513]
---

# Qt: the defect action row clips in the conversations drawer at 100 %

## Why

[[BPR-0513]] placed two more panels in the conversations drawer and its
capture showed action rows laid out horizontally clipping at 100 % scale:
the drawer never scrolls sideways, so a too-wide `QHBoxLayout` makes its
last actions absent rather than offscreen. [[BPR-0506]] stacked the finding
queue's six actions and the side chat's row was stacked with it; the defect
row (`defectActions` in `src/adapters/qt/src/workbench_window.cpp`, the
verify/reopen/withdraw/accept actions of RFC-BPR-0009/0010) still lays out
horizontally and still clips.

## Required final state

- Every action in the defect row is reachable at 100 %, 125 %, and 200 %
  with the drawer at its default width; the actions, their labels, object
  names, and intents are unchanged.
- The composed-window scale captures show the whole row.

## Verification

```text
ctest --preset dev -R '^(qt_composed_github_scale_(100\|125\|200)\|qt_review_queue\|qt_workbench_window_scale_100)$' --output-on-failure
```

Crop the drawer region of each capture and look at it; a whole-image diff
is not evidence.

## What this slice delivered — 2026-09-02

Both defect rows in the conversations drawer are now `QVBoxLayout` columns, on
the terms [[BPR-0506]] set for the finding queue. The measurement first: at the
drawer's default width the column's visible region was 522 px wide while these
two rows asked for 621. *Next defect* began at 539 and so was wholly outside
the drawer — absent, not offscreen, because the column never scrolls sideways;
*Previous defect* lost 11 px; *Apply local classification* lost 99 of its 159.
The defect navigation row clipped as badly as the action row, so both were
stacked. All eight actions, their order, their labels, their object names, and
the single intent each emits are untouched.

The count label keeps its place between the classify action and the pair that
walks the queue, and loses only the horizontal stretch that used to push that
pair to the far edge; a column has nothing to push against.

### The drawer's default width changed — 540 px to 366 px

This is a presentation change beyond the one row, and it is for the integrator
to accept or reject. The defect action row was the widest thing in the drawer's
column, so it, not any decision, was setting the column's minimum width and
through it the splitter's default for the drawer. With the row stacked, the
widest remaining row is the agent launch panel's, and the drawer opens at
366 px instead of 540 px. The user still drags it and the host still persists
what they choose; only the initial width moved. Everything else in the column
clips strictly less than before: the column's overflow past its viewport fell
from 99 px to 18 px, `showReviewThreadsAction` from 98 px cut to 17 px, and
`cancelAgentAction` from 89 px to 8 px. Those two rows are inside
`github_conversation_panel` and `agent_launch_panel`, not this window, and are
left for a card of their own.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Every defect action is laid into the drawer's column and inside its visible width, at all three scales | `ctest --preset dev -R '^(qt_composed_github_scale_(100\|125\|200)\|qt_review_queue\|qt_workbench_window_scale_100)$' --output-on-failure` | Pass, 5 of 5: 0.90 s / 0.77 s / 1.04 s / 1.99 s / 0.17 s | this commit |
| Putting both rows back horizontal fails the new case by name | restore `defectNavigation` and `defectActions` to `QHBoxLayout`, rebuild, run the three composed scales | Fails at each scale: *the defect action is inside the drawer's visible width: previousDefect*. Restored byte-exact, md5 `d83370fa506372daee438391df61e257` before and after | this commit |
| Putting only the action row back horizontal fails it by name too | restore `defectActions` alone to `QHBoxLayout`, rebuild, `ctest --preset dev -R '^qt_composed_github_scale_100$'` | Fails: *the defect action is inside the drawer's visible width: defectReclassify*. Restored byte-exact, same md5 | this commit |
| The new default drawer width breaks nothing else that reads it | `ctest --preset dev -R '^qt_' --output-on-failure` | Pass, 43 of 43, 124.97 s | this commit |
| Qt still performs no host effect | `cmake --build build/dev --target check_boundaries` | Pass | this commit |
| Documentation is consistent | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| Both touched files are formatted | `clang-format --dry-run --Werror` on the two files | Exit 0 | this commit |

### Capture evidence

`qt_composed_github_scale_100`, `_125`, and `_200` each save
`composed-github-<scale>-defect-drawer.png` under the build tree: the window's
own grab cropped to the drawer's exact device rectangle, after the column is
scrolled so the defect stack is wholly inside the drawer. All three were
cropped again to the defect rows and looked at. Each shows nine complete
controls — *Classify selected conversation as defect*, *Defects 0/0*,
*Previous defect*, *Next defect*, *Verify this fix*, *Reopen this defect*,
*Withdraw this defect*, *Accept as is*, *Apply local classification* — none
touching the right edge. The same capture taken against the pre-change layout
shows the defect it replaces: *Previous defec…* cut mid-word, *Next defect*
missing entirely, and *Apply loc…* cut mid-word.

### Folded in

[[BPR-0513]]'s drawer crop built its device rectangle by rounding the origin
and the size separately. Those two roundings disagree by a pixel at 1.25x for
half the drawer widths there are, and `QPixmap::copy` answers a rectangle that
leaves the raster with a smaller pixmap rather than refusing — so the new
drawer width turned a latent fault into a failing assertion. The rectangle is
now the pair of rounded edges, and the case additionally asserts the crop lies
inside the raster. The assertion's wording is unchanged.

### Not delivered

The `defectCount` label's rectangle still spans the column's full content
width, 17 px past the viewport, because a `QLabel` expands where a
`QToolButton` does not. Its text is left-aligned and short, so nothing of
*Defects 0/0* is lost; it is a rectangle overhanging, not a control clipped,
and giving the label a size policy is a change this card did not ask for.

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0), `check_boundaries` exit 0, and ran the three
composed-window scales, the three workbench scales, the review queue,
defect card, side chat, finding queue, and agent launch panel suites, and
`application_agent_finding_composition_contract`: 12/12. The 100 % drawer
crop was inspected: nine defect controls whole, none at the right edge.
The default drawer width falling from 540 px to 366 px is accepted as the
default — the splitter still resizes it, and the crop reads well — and the
two rows that still overflow at that width are carded as [[BPR-0527]].
The one-pixel crop correction to [[BPR-0513]]'s test is kept.
