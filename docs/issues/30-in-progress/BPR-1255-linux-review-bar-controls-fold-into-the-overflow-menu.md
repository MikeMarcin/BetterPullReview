---
schema: "implement-rfc.card.v1"
id: "BPR-1255"
title: "Linux: a review-bar control folds into the overflow menu in the title-row and workbench-window scale rows"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux", "win"]
metadata: {"found_by":"bpr-main-linux triage of the BPR-0905 Linux full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1255--BPR-1258--f86522aa8587.json","related_cards":["BPR-1251","BPR-1256","BPR-0905"]}
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

**Note: Windows fails the same check (bpr-main-win, 2026-10-04).** Windows 11
Home 10.0.26200, MSVC 14.51, Qt 6.11.2, `build/windows-dev` Debug, at
`36b71c56b` (`main` with BPR-1277's change; the rows run no composition code).
All six rows fail at the same check in a `-j4` run:
`qt_workbench_title_row_scale_{100,125,200}` and
`qt_workbench_window_scale_{100,125,200}`, each at
`FAILED: no review-bar control folds into an overflow menu at any width`
(`tests/adapters_qt/workbench_window_tests.cpp`, about line 5516). Recorded
only; the card stays parked for the owner.

## Owner decision — 2026-10-06

The owner chose option 1, **the floor follows the bar**, in chat, through the
integrator's question on 2026-10-06. When the fully yielded review bar needs
more than the platform floor, the window's minimum width rises to fit it. It
returns to the floor at normal fonts, and it is recomputed on font, scale and
theme changes. Window-geometry restore accepts the moving floor. The rule "no
control folds into an overflow menu" stays, amended to say that the floor
follows the bar. The minimum is a Qt layout fact; the core is unchanged.
Claimed for Linux; the Windows run follows.

## Linux implementation — 2026-10-06 (`bpr-main-linux`)

**Change** (`src/adapters/qt/src/workbench_window.cpp`, Qt only; core
unchanged):

- `apply_review_bar_width` first measures the review bar with everything
  yielded (no dates, compact positions, no position numbers).
- The window's minimum width becomes the platform floor
  (`platform_floor_width`, 900, or 902 on Windows) or that width, whichever is
  larger. Qt widens a window that is below a raised floor. The existing yield
  sequence then runs unchanged.
- The floor is measured on every strip relayout: each resize, the window's
  font, palette, style, theme and device-pixel-ratio changes, and the review
  bar's own font and style changes, through an event filter.
- Restore already clamps a saved width to `minimumWidth()`, so a geometry
  saved at 900 opens at the raised floor. `valid()` needs no change.
- The workbench specification states the amended rule.

**Measured on Linux** (DejaVu Sans): at the default font the floor stays 900.
At the doubled font it rises to 983.

**Tests** (`tests/adapters_qt/workbench_window_tests.cpp`, in the title-row
and full window rows):

- `test_the_floor_follows_the_review_bar`. At 3× the bar's font, wider than
  every platform's floor, the minimum rises. The window cannot be made
  narrower than it and nothing folds. The minimum returns to the platform
  floor at the default font, and a 3× window font raises it the same way.
- `test_restored_geometry_accepts_the_raised_floor`. A geometry saved at the
  platform floor (900) and restored under the doubled font opens at the floor
  the bar needs (983 here), with nothing folded, at 100, 125 and 200 %. The
  height is left to the screen's clamp, since the scaled offscreen screen is
  shorter.

**Evidence** (`build/dev`, GCC 15.2, offscreen, `-j1`):

- `qt_workbench_title_row_scale_{100,125,200}` and
  `qt_workbench_window_scale_{100,125,200}` pass 6/6. The review-bar course
  covers the default and the doubled font.
- Failing-before control: with the original `workbench_window.cpp` and the new
  tests, all six fail at "no review-bar control folds into an overflow menu at
  any width". They pass again with the change.
- Wider set: every `qt_` row, the scaling, window-restoration, multi-window,
  geometry and composed-window courses, `review_cycle_desktop` and
  `architecture_boundaries` pass 132/132.
- `coding-format-check`, `git diff --check` and `DOCUMENTATION_OK` pass.

**Owed:** a Windows run (the six rows failed there at the same check) and a
macOS run, to confirm the 3× rise and that the default and doubled fonts keep
the floor. The card stays In Progress until they are in.

**Note: Windows run at `9bed0a21a` (holds `012eb89c0`; station `bpr-main-win`,
MSVC 14.51, Qt 6.11.2, `windows-dev` Debug, offscreen; no remote
mutation).** The whole tree built with no `warning C` or `warning LNK` line.
The six rows run with `-j1`:

- `qt_workbench_title_row_scale_{100,125,200}` passed (23.6, 26.4, 29.6 s).
- `qt_workbench_window_scale_{100,125,200}` first stopped at an MSVC
  checked-iterator assertion, "can't dereference invalidated vector
  iterator" (`0xC0000409`), in [[BPR-1133]]'s
  `test_comment_names_its_target_and_every_card_replies`. The test found
  `review-thread:t22`'s card in the cards list, then opened and closed the
  Comment menu with events processed. That rebuilds the cards, and the test
  then read `card->Actions` through the stale iterator. GCC does not check
  iterators, so on Linux the read went unseen. The test now looks the card up
  again after the menu closes. Only the test changed, no product code.
- With that, all six pass: the window rows in 134.7, 141.5 and 151.5 s.

**The floor on Windows**, read once with a temporary print (not committed)
in `test_restored_geometry_accepts_the_raised_floor`. It is the same at
100, 125 and 200 %:

| Review bar font | Window minimum width |
|---|---|
| Default | 902, the platform floor |
| Doubled | 917 |

So the default font keeps the platform's floor of 902. The doubled font
raises it to 917; Linux measured 983, with a wider default font. The 3×
rise passes in `test_the_floor_follows_the_review_bar`. The macOS run is
still owed.
