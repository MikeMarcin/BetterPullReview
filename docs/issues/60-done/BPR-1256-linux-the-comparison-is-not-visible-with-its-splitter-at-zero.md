---
schema: "implement-rfc.card.v1"
id: "BPR-1256"
title: "Linux: application_diff_workbench_public_path and application_appearance_store_fallback_contract never see the comparison; its splitter share is zero"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux triage of the BPR-0905 Linux full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1255--BPR-1258--f86522aa8587.json","related_cards":["BPR-1255","BPR-1251","BPR-0823","BPR-1102","BPR-0905"]}
acceptance: [{"criterion":"Both rows pass on Linux","outcome":"pass","revision":"2191da24f","evidence":{"kind":"command","detail":"bpr-main-linux, WSL2, GCC 15.2, build/dev: application_diff_workbench_public_path passed in 3.10 s and application_appearance_store_fallback_contract in 4.03 s; application_appearance_theme_customization_contract passed in 6.30 s. In the qt_/application_ sweep at -j10, 306 of 314 pass, and both rows are among them."}},{"criterion":"The card names the cause","outcome":"pass","revision":"2191da24f","evidence":{"kind":"observation","detail":"Card 'Cause and fix': visibleRegion() leaves out the comparison's own opaque child diffSharedScrollBar (16 px under the Windows style), so 616 of 632 px read as visible. This was not a width problem; the splitter's 0 is the collapsed repositories drawer. Negative control: a covering sibling still fails the check."}}]
---

# BPR-1256: Linux: application_diff_workbench_public_path and application_appearance_store_fallback_contract never see the comparison; its splitter share is zero

Found by bpr-main-linux on 2026-10-01: WSL2 Linux 6.18, GCC 15.2, Qt offscreen
(`GITHUB_FIXTURE_QT_STYLE:QWindowsStyle`), `build/dev` Debug, at `db021262a`.
Reported first by bpr-main-win's BPR-0905 full suite in this checkout. A serial
rerun failed the same way.

## Observation

`application_diff_workbench_public_path` fails in 1.35 s:

```
DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED:modality=Review host_modality=Review centre=comparison offered=comparison,files_drawer,conversations_drawer withheld=repositories_drawer files_drawer=expanded host_files_drawer=expanded conversations_drawer=expanded host_conversations_drawer=expanded repositories_drawer=collapsed host_repositories_drawer=collapsed drawer_controls=hidden outstanding_requests=1 splitter_sizes=0,320,632,320
PUBLIC_PATH_FAILED
DIFF_WORKBENCH_FAILED
DIFF_INTERACTION_EVIDENCE_FAILED
CONVERSATION_ANCHOR_EVIDENCE_FAILED
SYNTAX_THEME_REPAINT_FAILED
```

`application_appearance_store_fallback_contract` fails in 1.71 s with
`fallback session process failed (1)`, and its session prints the same
`DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` line.

Every drawer is in its expected state. The first splitter share is 0 pixels, so
nothing in the comparison is visible. [[BPR-1102]] (Windows, capture state) and
[[BPR-0823]] were different causes.

This is a width rule at Linux metrics, like [[BPR-1255]] and [[BPR-1251]]'s
Linux half. Measure them together once before splitting fixes.

## Scope

- Find which share is zero and why: the window's width at capture, the
  drawers' fixed widths, or the minimum width of the comparison.
- Fix it where the layout owns it.

## Acceptance

- Both rows pass on Linux.
- The card names the cause.

## Cause and fix (bpr-main-linux, 2026-10-01)

**This was not a width problem.** `splitter_sizes=0,320,632,320` lists the
central splitter's panes in order: repositories (collapsed, 0), files,
comparison (632) and conversations. The comparison had its full share. A
probe, not committed, in `comparison_is_wholly_visible` (`src/app/main.cpp`)
printed:

```
central=1280x932 window=1280x1000 placed=324,56 632x876 visible=0,0 616x876
COVER QScrollBar:diffSharedScrollBar at=940,56 16x876 isComparisonOrChild=1
```

`QWidget::visibleRegion()` leaves out what the widget's own opaque children
cover. The comparison's shared scroll bar is a descendant of the comparison,
16 pixels wide under the Windows style that the Linux Qt rows use
(`QT_STYLE_OVERRIDE=Windows`). So the comparison's own right edge read as
clipped. The check dates from BPR-0219. The scroll bar being shown in this
composition is what exposed it on Linux. The cause on macOS was not measured:
the row passes there.

**Fix (`2191da24f`, in the `BPR_BUILD_TESTING` probe).** What the comparison
shows is now its own visible region together with each visible descendant's,
mapped into the comparison. Containment in the central widget is unchanged. A
sibling laid over the comparison's edge still fails the check. The
round-trip composition contract's production pin is resealed, with a reseal
note. No product action changes.

| Check | Result |
|---|---|
| `application_diff_workbench_public_path` | Passed, 3.10 s (was `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED`) |
| `application_appearance_store_fallback_contract` | Passed, 4.03 s |
| `application_appearance_theme_customization_contract`, which uses the same check | Passed, 6.30 s |
| Negative control: an opaque 16-pixel sibling raised over the comparison's right edge before the check (not committed) | `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED`, as before |
| `application_review_round_trip_composition_contract` after the reseal | Passed, 2.21 s |
| `ctest --preset dev -R '^(qt_\|application_)' -E '^application_composition_contract$' -j10` (with the tab-strip change from [[BPR-1251]]'s Linux half) | 306 of 314 pass. The 8 failures: [[BPR-1255]]'s six, and the [[BPR-1245]] and [[BPR-1236]] rows already recorded. |
| clang-format 22.1.8 and `tidy.py --paths src/app/main.cpp` | Clean, `clang-tidy passed` |

The shared-width hypothesis from triage was wrong for this card. See
[[BPR-1255]].
