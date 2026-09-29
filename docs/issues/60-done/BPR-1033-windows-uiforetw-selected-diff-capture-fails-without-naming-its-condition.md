---
schema: "implement-rfc.card.v1"
id: "BPR-1033"
title: "Windows: application_uiforetw_selected_diff_contract fails at UIFORETW_CAPTURE_FIRST_TARGET_FAILED without naming which of its nine conditions is false"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-0875 thirteen-row run, bpr-main-win","found_at":"c4130c3a","first_seen":"2b2e5549 (dsp-004 Windows sweep, 2026-09-17)","related_cards":["BPR-0875","BPR-0859","BPR-0890"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1033--BPR-1034--6684dcafdd51.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"9d3fb998b","evidence":{"kind":"observation","detail":"A failing Windows run at 585d8846 plus the change (9d3fb998) printed UIFORETW_CAPTURE_FIRST_TARGET_FACTS after the existing marker, one term per clause with its measured values (recorded on the card, bpr-main-win 2026-09-23)."}},{"criterion":"AC2","outcome":"pass","revision":"f9870baaf","evidence":{"kind":"command","detail":"application_uiforetw_selected_diff_contract passes on Windows (windows-dev, MSVC 19.51, Qt 6.11.2): at 585d8846 plus the change, at 22d18416 with 537f718d's build fixes (57.05 s) and at f9870baa (63.45 s)."}},{"criterion":"AC3","outcome":"pass","revision":"b18888aa","evidence":{"kind":"command","detail":"The row passes on Linux after BPR-1036 (bpr-main-linux, 2026-09-24) and on macOS at b18888aa (bpr-main-mac, 2026-09-24), recorded on the card."}}]
---

# BPR-1033: Windows: application_uiforetw_selected_diff_contract fails at UIFORETW_CAPTURE_FIRST_TARGET_FAILED without naming which of its nine conditions is false

## Defect

`application_uiforetw_selected_diff_contract` passes on macOS and Linux
([[BPR-0859]]'s acceptance) and fails on Windows. It has failed there since
the first full Windows sweep at `2b2e5549`
(`docs/verification/dsp4-windows-merged-head-sweep.md`). It still fails at
`c4130c3a`, measured on the bpr-main-win station (MSVC 19.51 Debug,
Qt 6.11.2, offscreen, `GITHUB_FIXTURE_QT_STYLE:QFusionStyle`), in 9.30 s:

```text
PUBLIC_PATH_FAILED
PUBLIC_PATH_STAGE:2
PUBLIC_PATH_VERDICT:presentation=0 shutdown_complete=1 cleanup_failed=0
...
UIFORETW_SELECTED_DIFF_TRACE_OK
UIFORETW_SELECTED_DIFF_EVIDENCE_FAILED
stderr: UIFORETW_CAPTURE_FIRST_TARGET_FAILED
```

Every earlier `UIFORETW_*` marker is OK: the exact commit pair, both
resource and RC gestures, blobs, intraline spans and roles, the focused
review layout, the unused fixture diff helper and the selected-diff trace.
[[BPR-0875]] removed this row's cleanup failure (`cleanup_failed=0` now), so
this is all that is left of it.

`UIFORETW_CAPTURE_FIRST_TARGET_FAILED` is printed by one condition in the
capture course (`src/app/src/github_application.cpp`, about line 18279 at
`2a5580d6`). The condition joins nine clauses with `||`:

1. the first target observed (`firstSelected`);
2. the first pair selected in the workbench and the link map (`selected(*firstPair)`);
3. left role pixels of the first target greater than zero;
4. right role pixels of the first target greater than zero;
5. the second target observed (`secondUnselected`);
6. the first target's connector observed;
7. the second target's connector observed;
8. the selected connector readable (endpoints, overlaps, valid colours, at least 24 sampled, contrast and dominant pixels);
9. the unselected connector readable (the same, plus at least half the pixels in contrast and a median contrast of 3.00 or more).

The output does not say which clause is false, so the next step is a guess.

## Scope

- **Name the failing clause first.** Print one term per clause, with the
  measured value where there is one: pixel counts, the median contrast, the
  endpoint and overlap flags. Print them on failure only and after the
  existing marker, so every existing marker and its order stay the same.
  Then run once on Windows.
- Then fix the cause in the layer that owns it. Plausible suspects, none
  measured: the target rectangles (`"WS_TABSTOP,464,28,67,10"` and friends)
  land on different glyphs under Windows font metrics; the connector sampling
  reads a Fusion-offscreen raster whose antialiasing differs on Windows; or
  the selection is not yet on the first pair when the capture runs.
- Keep the macOS and Linux behaviour: the row passes there today.

## Acceptance

- AC1 A failing run names the clause that failed, with its measured values,
  shown in a real Windows run.
- AC2 `application_uiforetw_selected_diff_contract` passes on Windows. If it
  cannot, the named cause gets its own recorded limitation.
- AC3 The row still passes on macOS and Linux.

**Note (bpr-main-linux, 2026-09-23).** The row no longer passes on macOS or
Linux, for two reasons that are not this card's. Both are in the capture after
the first-target clause: a strip row that predates [[BPR-0990]]'s
`refreshQueueAction`, and a comparison under the 850/1000 height floor. They
are carded as [[BPR-1036]]. AC3 can only be judged once BPR-1036 is done. Until
then, a Linux or macOS failure at `UIFORETW_NAVIGATION_ROW_UNEXPECTED` or
`UIFORETW_CAPTURE_LAYOUT_FAILED` is BPR-1036's, not a regression from this card.

## Named, fixed, and the row passes on Windows (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2,
offscreen, `QFusionStyle`), at `585d8846` plus this change. The logs are station
records under `build/logs/bpr-1033-main-win/` and are not committed.

**The clause is named (AC1).** After `UIFORETW_CAPTURE_FIRST_TARGET_FAILED`
the capture now prints one line, `UIFORETW_CAPTURE_FIRST_TARGET_FACTS:`, with
one term per clause in the condition's order and the values each clause reads:
both targets' rectangles and role pixels, the selection split into the
workbench's and the link map's, the link map's own state, and each connector's
endpoints, overlaps, colours, pixel counts and median contrast. It prints on
failure only and after the marker, so every earlier line keeps its place. The
first Windows run printed:

```text
UIFORETW_CAPTURE_FIRST_TARGET_FACTS: first_observed=1
first_left_rect=429,350,8,14 first_right_rect=429,350,8,14
first_left_role_pixels=41 first_right_role_pixels=59 first_selected=0
workbench_selection=1 link_map_selection=0 second_observed=1
second_left_rect=134,378,15,14 second_right_rect=134,378,15,14
second_left_role_pixels=115 second_right_role_pixels=99 geometry_pending=1
link_map=visible:1,size:56x857,links:0,first_segment_links:0,second_segment_links:0,selected:0
first_connector_observed=0 second_connector_observed=0 selected_readable=0
unselected_readable=0
```

So clause 2 is false only on its link-map half, and clauses 6 to 9 are false
because neither target has a connector. The glyph targets, their role pixels
and the workbench's selection are all right. None of the three suspects above
is the cause.

**The cause.** The link map was visible and had no links at all, and the
workbench still had its geometry pending. The panes measure their lines in
batches of about 4 ms. The link map gets its geometry, and with it its links
and its selection, only when that measurement completes. The capture waited a
fixed four event passes after moving to the first target, and on this station
the measurement of `UIforETW.rc` needs more batches than that. Four passes
were enough on macOS and Linux, which is why the row passed there.

**The fix, in the fixture probe.** The capture's `drain` now waits for the
measurement itself, bounded at 5 s, and then drains as before. It is the same
wait `diff_workbench_tests`' `settle_geometry` uses. It waits only while the
workbench reports its geometry pending, so a platform that finishes within
four passes does exactly what it did before. No product path changes.

The row then got further and failed later, in the contract, at
`UIforETW light compact navigation order/edges/spacer drifted`. The capture
facts showed why. On Windows the strip is the title row, so the caption
buttons end the row after settings ([[BPR-0653]]). The right-edge gap was 227
against a bound of 110, and the bound was measured for refresh, the divider,
account and settings alone. The capture facts now carry
`window_controls_rect_in_row`, the caption buttons' rectangle on Windows and an
empty one elsewhere. The contract takes its width off the right-edge gap
before applying the unchanged bound. On macOS and Linux it subtracts 0.

**The row passes (AC2).** `application_uiforetw_selected_diff_contract` passes
on Windows in 60.25 s: light, dark and both high-contrast profiles. It passes
again in 58.40 s on the committed candidate, formatted and resealed, and
`application_review_round_trip_composition_contract` passes beside it
(7.09 s).

| Run | Change | Result |
| --- | --- | --- |
| Before | Facts only | `FIRST_TARGET_FAILED`, facts as above |
| Negative control | Facts, with the wait disabled | `FIRST_TARGET_FAILED`, `geometry_pending=1`, `links:0` |
| Wait only | Facts and the bounded wait | Capture passes; contract fails at `compact navigation order/edges/spacer drifted` (right-edge gap 227) |
| Fix | All of it | Passes, 60.25 s; again 58.40 s as committed |

**The pins.** `github_application.cpp` is pinned by
`application_review_round_trip_composition_contract`. The change is inside the
`BPR_BUILD_TESTING` fixture-access region: the UIforETW capture's wait, its
failure line and one capture fact. The full-file and manifest digests are
resealed in the same commit, covering `3d941d47..` this card's commit. The
mutation-branch pin is unchanged. This is the review that reseal records.

**Related, not fixed here.** `qt_workbench_window_scale_{100,125,200}` fail on
this station at "the composed link map paints ribbons to begin with". It is the
same class of failure, measured with a reverted probe: after the window test's
fixed four passes the link map had `entries=0 rebuilds=0`, and a few passes
later it had its geometry and 5 links. Waiting for the measurement there got
past that line but not past the next one, a shared scrollbar that was not yet
visible, so that test needs more than this wait. It is left for its own card,
and an ID has been requested.

### Still open

- **AC3, macOS and Linux.** No run has been made from this station. On those
  platforms the wait returns at once if the geometry is ready, as it evidently
  was, and the new fact subtracts 0. So the row there should still stop only
  where [[BPR-1036]] records, but only a run can show that. Wanted on
  bpr-main-mac and bpr-main-linux, after BPR-1036's answer:
  `ctest --preset dev -R '^(application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract)$' --output-on-failure`.

### The owner's answer that AC3 waited for (main, 2026-09-24)

The owner answered [[BPR-0653]]'s ask 3 in attended session
`cbd48b91000b00f1e72e1ad62c7c384c` (decision `007a6eb5`, 2026-09-24T18:54:09Z):
**amend RFC-BPR-0028 AC-01 to admit the entry rows**, with a design follow-up
in [[BPR-1106]]. So [[BPR-1036]] is Ready and takes its height floor from the
measured stacks rather than moving the entries. AC3 here is judged by the
macOS and Linux run named under "Still open" once BPR-1036's floor change is
on main. Recorded by the main integrator consuming the hand-back; this card's
claim and its Windows work are unchanged.

## Linux: AC3's rows pass after BPR-1036's floor (bpr-main-linux, 2026-09-24)

This is AC3's Linux half, the run named under "Still open". The source is
`d3b583d0` on `main`, which contains this card's `9d3fb998` and
[[BPR-1036]]'s floor change. The station is bpr-main-linux: WSL2 Ubuntu,
GCC 15.2, Qt 6.11.2, `build/dev` Debug, offscreen with the basic Windows
style (`QT_STYLE_OVERRIDE=Windows`, the row's own environment). The run used a
fresh `HOME` and fresh XDG roots under `/tmp`, with `DISPLAY` and
`WAYLAND_DISPLAY` unset. The application was rebuilt first
(`cmake --build --preset dev --target app_better_pull_review`, exit 0, no
`warning:` or `error:` line). The command was the one named above:
`ctest --preset dev -R '^(application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract)$' --output-on-failure`.

| Row | Result |
|---|---|
| `application_uiforetw_selected_diff_contract` | **Passed**, 15.16 s |
| `application_review_round_trip_composition_contract` | **Passed**, 1.59 s |

Against what "Still open" expected on Linux:

- **The row passes.** Met, in all four profiles (light, dark and both high
  contrasts). In each profile's `capture-facts.json` the comparison takes
  987/1000 of the window's width and 842/1000 of its height, and
  `navigation_row_exact_order` is true. The height meets
  BPR-1036's Linux floor of 842, the measured stack under RFC-BPR-0028 AC-01
  as the owner amended it.
- **The first target holds.** Met. The capture wrote its evidence, so the
  first-target condition held and neither
  `UIFORETW_CAPTURE_FIRST_TARGET_FAILED` nor its facts line printed.
- **The new fact subtracts 0.** Met. Every profile's
  `window_controls_rect_in_row` is `{x:0, y:0, width:0, height:0}`, so the
  right-edge gap the contract bounds is the one it bounded before.

The capture facts are in
`build/dev/tests/end_to_end/uiforetw-selected-diff-contract/data/evidence/<profile>/capture-facts.json`.
The log is `build/r2-logs/bpr1033-ac3-linux-d3b583d0.log`, a station record
that is not committed.

**AC3 on Linux is met.** macOS is still owed: the same command on
bpr-main-mac at `d3b583d0` or later, which is also BPR-1036's owed item 1.
This note does not claim, move or finish this card; bpr-main-win owns it.

## macOS: AC3's rows pass at `b18888aa` (bpr-main-mac, 2026-09-24)

The macOS half of AC3, run on macOS 26.6.2 arm64, `build/dev`, at
`b18888aa` (which contains `d3b583d0`): first
`cmake --build --preset dev --target app_better_pull_review`, then
`ctest --preset dev -V -R '^(application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract)$' --output-on-failure`.

| Row | Result |
|---|---|
| `application_uiforetw_selected_diff_contract` | **Passed**, 69.88 s, `UIFORETW_SELECTED_DIFF_CONTRACT_OK` |
| `application_review_round_trip_composition_contract` | **Passed**, 11.51 s, `REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK` |

In all four profiles `capture-facts.json` has the comparison at 985/1000 of
the width and 835/1000 of the height, meeting [[BPR-1036]]'s macOS floor;
`navigation_row_exact_order` is true; and `window_controls_rect_in_row` is
`{x:0, y:0, width:0, height:0}`, so the new fact subtracts nothing. Neither
`UIFORETW_CAPTURE_FIRST_TARGET_FAILED`, `UIFORETW_NAVIGATION_ROW_UNEXPECTED`
nor `UIFORETW_CAPTURE_LAYOUT_FAILED` printed. **AC3 on macOS is met.** This
note does not claim, move or finish this card; bpr-main-win owns it.

## Released to Ready, 2026-09-25 (bpr-main-win, wind-down)

The owner asked the stations to wind down, so this card's claim is released.
Its evidence is all recorded above:

- AC1 and AC2 on Windows, at `585d8846` plus the change;
- AC3 on Linux, after [[BPR-1036]];
- AC3 on macOS, at `b18888aa`.

What remains: `application_uiforetw_selected_diff_contract`, once more on
Windows at the current main revision, because the workbench has changed
since `585d8846`. Then the finish decision. Both need no owner, and neither
needs the foreground.

## Rerun at current `main`, and finished (bpr-main-win, 2026-09-29)

Claimed again at `27cc07f8` for the one run the wind-down note left:
`application_uiforetw_selected_diff_contract` on Windows at the current
`main`, since the workbench has changed since `585d8846`.

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, `build/windows-dev`
Debug with warnings as errors. `main` at `22d18416` did not build on
Windows, for reasons outside this card; `537f718d` fixes them without
touching the workbench, the row or its course.

| Revision | `application_uiforetw_selected_diff_contract` |
|---|---|
| `22d18416` with `537f718d`'s build fixes | Pass, 57.05 s |
| `f9870baa` | Pass, 63.45 s |

Both runs were serial, through `ctest`, and needed no foreground. The row
is uncontained on Windows, as pinned.

**Finish.**
- AC1: met on Windows at `585d8846` plus the change, recorded above. The
  failing run named the clause and its measured values.
- AC2: met. The row passes on Windows at `585d8846` plus the change, and
  again at the current `main`, in the table above.
- AC3: met on Linux after [[BPR-1036]], and on macOS at `b18888aa`,
  recorded above.

The logs are station records in
`C:\Users\mikem\bpr-main-helpers\run\station-0929\` (`new.log`,
`pass2.log`), and are not committed.
