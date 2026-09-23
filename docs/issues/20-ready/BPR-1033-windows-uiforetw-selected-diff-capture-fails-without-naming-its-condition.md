---
schema: "implement-rfc.card.v1"
id: "BPR-1033"
title: "Windows: application_uiforetw_selected_diff_contract fails at UIFORETW_CAPTURE_FIRST_TARGET_FAILED without naming which of its nine conditions is false"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-0875 thirteen-row run, bpr-main-win","found_at":"c4130c3a","first_seen":"2b2e5549 (dsp-004 Windows sweep, 2026-09-17)","related_cards":["BPR-0875","BPR-0859","BPR-0890"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1033--BPR-1034--6684dcafdd51.json"}
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
