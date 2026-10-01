---
schema: "implement-rfc.card.v1"
id: "BPR-1255"
title: "Linux: a review-bar control folds into the overflow menu in the title-row and workbench-window scale rows"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux triage of the BPR-0905 Linux full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1255--BPR-1258--f86522aa8587.json","related_cards":["BPR-1251","BPR-1256","BPR-0905"]}
parked_on: "human-review"
acceptance: [{"criterion":"The six rows pass on Linux","outcome":"pending","revision":"2ac99c816","evidence":{"kind":"observation","detail":"Waits on the owner's choice among the card's three options. With only the overflow check bypassed (not committed), all six rows pass on Linux at this revision."}},{"criterion":"The card names the cause","outcome":"pass","revision":"2ac99c816","evidence":{"kind":"observation","detail":"Card 'Cause': at the course's doubled font, 18 pt Sans Serif (DejaVu Sans), the fully yielded review bar needs 983 px against the 900 px floor. Every yield (dates, words, numbers) has already fired, and the last control, Comment, folds at 982 px. At the default font the bar is 843 px and fits."}}]
---

# BPR-1255: Linux: a review-bar control folds into the overflow menu in the title-row and workbench-window scale rows

Found by bpr-main-linux on 2026-10-01: WSL2 Linux 6.18, GCC 15.2, Qt offscreen,
`build/dev` Debug, at `db021262a`. bpr-main-win's BPR-0905 full suite in this
checkout (556 rows, -j10) reported them first. A serial rerun of each row
failed the same way.

## Observation

All six rows fail at every scale, in about 2 s each, at the same check
(`tests/adapters_qt/workbench_window_tests.cpp`, about line 5514):

```
FAILED: no review-bar control folds into an overflow menu at any width
```

- `qt_workbench_title_row_scale_{100,125,200}`
- `qt_workbench_window_scale_{100,125,200}`

[[BPR-1251]] records Windows failures in `qt_workbench_window_scale_*`, but at a
different check ("the title yields once and does not return at a narrower
width"). This card stays separate from it, at the integrator's direction.

The same Linux run also has a document tab strip narrower than one tab
([[BPR-1251]]'s Linux half) and a comparison splitter at zero ([[BPR-1256]]). All
three are width rules at Linux metrics. Measure them together once before
splitting fixes.

## Scope

- Measure the review bar at the failing widths under Linux metrics. Name the
  control that folds, and what takes its room.
- Fix the layout that owns the rule, not the check.
- Keep macOS and Windows unchanged.

## Acceptance

- The six rows pass on Linux.
- The card names the cause.

## Cause (bpr-main-linux, 2026-10-01)

The check runs in the review-bar course (`workbench_window_tests.cpp`, about
line 5500). After a wide pass, the course doubles the review bar's font, "as a
reviewer's system font can be". It then narrows the window from 1,920 pixels to
`minimumWidth()`, 900 pixels off Windows, and requires that no control folds
into the overflow menu at any width.

Probes, not committed, in the course and in `apply_review_bar_width` printed:

- **Default font (9 pt).** The fully yielded bar needs 843 pixels, which fits
  the floor.
- **Doubled font (18 pt "Sans Serif", which fontconfig resolves to DejaVu Sans).**
  Every yield has fired: the dates, the connective words and the position
  numbers. The bar still needs 983 pixels. At 982 pixels the last control,
  Comment, folds.

```
PROBE width=982 bar=982 barHint=983 font=18 family=Sans Serif style=windows
  Updates 137 | Comparing exact anchors 360 | divider 13 | File 47 | 2×30
  | Changes 111 | 2×30 | Private progress 51 | spacer | Compose review 30
  | Comment 51 (folded)
```

So the layout does what RFC-BPR-0052 specifies. Nothing on the bar has
another yield, and DejaVu Sans at double size is wider than the 900-pixel
floor allows. On macOS and Windows the doubled default face fits, so their
rows pass. Windows' title-row rows pass, and [[BPR-1251]] records only its tab
and window rows. Nothing else in these six rows fails on Linux: with this one
check bypassed (not committed), all six pass.

[[BPR-1256]] turned out to be unrelated. [[BPR-1251]]'s Linux half was a width
problem of its own, and was fixed by giving the tab strip a minimum width.

## The decision, which is the owner's

The workbench specification says "No control folds into an overflow menu at
any width down to the platform's floor". It says nothing about a larger font
than the platform's default. Each way out changes something the owner chose:

1. **The floor follows the review bar.** When the fully yielded bar needs more
   than 900 pixels (902 on Windows), the window's minimum width rises to that
   width, and falls back when the font does. Nothing changes at the default
   font. The cost: the owner-chosen floor becomes a minimum, not a fixed value,
   and window-geometry persistence has to accept a floor that moves.
2. **One more yield.** For example, "Comparing exact anchors" yields to
   "Comparing", with the full name in its tooltip, or the Updates button
   becomes icon-only, after the position numbers. This changes the owner's
   yield order (hand-back 49).
3. **Bound the course's font.** The course enlarges the font by a factor that
   fits the floor in every platform's default face (on Linux it is below 2×; the exact factor was not measured),
   and the specification says the rule holds up to that factor. No product
   change. The cost is a weaker statement for larger fonts.

Until the owner chooses, the six Linux rows stay red at this check only.
