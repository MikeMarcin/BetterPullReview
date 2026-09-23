---
schema: "implement-rfc.card.v1"
id: "BPR-1036"
title: "application_uiforetw_selected_diff_contract is red at HEAD: its exact strip row predates BPR-0990's refreshQueueAction, and the comparison is 842/1000 of the window height against an 850 floor"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-0863's baseline run","found_at":"dec43dda","reproduced_at":["b93adf45","3dca2d7a","1dfe0600 (macOS)","4a4c1e40"],"last_green_linux":"271bb452 (BPR-0875's twelve-course run)","folded_from":["BPR-0864"],"related_cards":["BPR-0859","BPR-0863","BPR-0864","BPR-0990","BPR-1033","BPR-1037"],"reservation":"docs/issues/_reservations/BPR-1036--BPR-1036--0e7760d6ea6e.json"}
---

# BPR-1036: application_uiforetw_selected_diff_contract is red at HEAD: its exact strip row predates BPR-0990's refreshQueueAction, and the comparison is 842/1000 of the window height against an 850 floor

## Defect

`application_uiforetw_selected_diff_contract` fails on Linux and macOS. It
passed on Linux at `271bb452` ([[BPR-0875]]'s twelve-course run, 15.06 s), and
has failed on every run since `dec43dda`. The probe itself passes. Every
marker from `UIFORETW_RESOURCE_GESTURE_OK` to `UIFORETW_SELECTED_DIFF_TRACE_OK`
prints OK, and so does [[BPR-0863]]'s shared modality step. The failure is in
the capture that follows, `capture_uiforetw_selected_diff_evidence` in
`src/app/src/github_application.cpp`, and it has two separate causes.

**1. The exact strip row is stale.** The capture asserts the whole strip in
order: `toolbarActions.size() == 27 + lead + trailing`, with
`github_refresh_action()` at index 22 and the conversations drawer at 23.
[[BPR-0990]] (`0d56cee5`) added `refreshQueueAction`, Home's discovery refresh,
straight after the pull request's refresh. It is hidden outside Home. A hidden
action is still one of `toolbar->actions()`, so the row now has 28 entries and
every later index is one along. The mismatch is recorded as
`navigation_row_exact_order=false` in the capture facts. The course script
requires that fact to be true, so this cause alone fails the row.

Reproduced on this station at `4a4c1e40`
(`build/r2-logs/bpr1036-uiforetw-head-4a4c1e40.log`, 2.02 s):

```text
UIFORETW_RESOURCE_GESTURE_OK
UIFORETW_NAVIGATION_ROW_UNEXPECTED:actions=28
UIFORETW_NAVIGATION_ROW_ACTION:22=refreshPullRequestAction
UIFORETW_NAVIGATION_ROW_ACTION:23=refreshQueueAction
UIFORETW_NAVIGATION_ROW_ACTION:24=toggleWorkbenchConversationsDrawerAction
UIFORETW_NAVIGATION_ROW_ACTION:25=workbenchStripRightDividerAction
UIFORETW_NAVIGATION_ROW_ACTION:26=workbenchAccountAction
UIFORETW_NAVIGATION_ROW_ACTION:27=openSettingsAction
UIFORETW_CAPTURE_LAYOUT_FAILED:987:842
```

**2. The comparison is too short for the capture's layout floor.** The capture
needs the comparison surface to take at least 750/1000 of the window's width
and 850/1000 of its height, in a window resized to 1280x1000. Width passes
(987). Height does not: 842 on Linux and 835/985 on macOS
(`build/bpr0863-mac-uiforetw.log` on bpr-main-mac, at `1dfe0600`). BPR-0863's
diagnostic build corrected only the strip row. It then failed only on this
ratio (`build/r2-logs/bpr0863-diag-uiforetw-row-corrected.log`), so the two
causes are independent. Nobody has yet measured what took the height. The
regression range is `271bb452..dec43dda`. It contains 61 commits under
`src/adapters/qt` or `src/app`, among them [[BPR-0990]]'s Home discovery status
line and several changes to the review surfaces. The failure line gives two
ratios and no widgets, so the next step is still a guess.

**3. Folded in from [[BPR-0864]]: the probe's own waits are unbounded or
unnamed.** BPR-0864 gave the workbench composition probe one bounded, named
wait per phase. It checked this probe and found both halves of the same
defect:

- `awaiting_observation` returns `pending` without a bound. It waits for the
  accepted snapshot, zero outstanding workbench requests, host navigation,
  both drawer rows and the files view. A stall there runs until the 40 s run
  budget expires and prints only `RUN_BUDGET_EXPIRED … stage=`.
- In `awaiting_resource_diff` and `awaiting_rc_diff`, the publication wait is
  capped only once the file gesture has registered
  (`++probe.Waits > 512U && probe.Evidence.ResourceGesture`). A gesture that
  never registers waits without a bound.
- Six `512U` tick caps fail with no term:
  - the resource blob match;
  - the support-file publication;
  - the support file's exact result;
  - the resource-return publication;
  - the resource-return blob match;
  - the rc blob match.

  The change-group walk's cap (`Waits++ > Groups.size()`) also fails without
  a term. Each of these ends in `UIFORETW_SELECTED_DIFF_TRACE_FAILED` with
  nothing that names the condition.

The named waits already in this probe stay as they are:
`UIFORETW_REVIEW_PANES_HIDDEN`, `UIFORETW_{RESOURCE,RC}_ROLE_PAINT_STALLED`,
the reveal refusals and `UIFORETW_FOCUSED_LAYOUT_STALLED`.

## Relation to other cards

- [[BPR-1033]] is the Windows failure of this row, at
  `UIFORETW_CAPTURE_FIRST_TARGET_FAILED`. That clause runs before both causes
  above, so Windows has not reached them yet. BPR-1033's AC3 ("the row still
  passes on macOS and Linux") cannot hold until this card is done.
- [[BPR-1037]] applies the same bounded-wait shape to four other probes in the
  same file. If one card extracts BPR-0864's `wait`/`advance`/`stall` into a
  helper the probes share, the other should use it rather than add a copy.

## Scope

- **Strip row.** Put `refreshQueueAction` into the exact row, on every
  platform through the existing `lead`/`trailing` offsets. Keep the row
  asserted whole, in order and exactly. Do not relax it to a subset, and do
  not skip hidden actions, since the row pins action order and not what is
  visible. Update the `[[BPR-0859]]` comment's count.
- **Height floor.** First make `UIFORETW_CAPTURE_LAYOUT_FAILED` name what took
  the height. On failure only, print the window, menu bar, strip, epoch line,
  comparison and status bar geometry, and the modality facts that every
  course's failure prints ([[BPR-0863]]). Then fix the cause in the layer that
  owns it:
  - if an element that belongs elsewhere, such as a Home-only line, occupies
    Review, fix the product;
  - if the product change was intended, take the new floor from the measured
    stack, record why on this card, and update
    `docs/ux/workbench-specification.md` if the layout rule changed.

  The menu bar is not the whole answer: macOS merges it into the system menu
  bar and costs the window no height (the specification's menu bar section),
  and macOS fails too.

  Do not lower the floor just to turn the row green.
- **Waits.** Give `awaiting_observation`, the two gesture-qualified caps and
  the six silent caps a bounded wait with a named term that prints the
  inputs it depends on. Reuse BPR-0864's shape, not a new mechanism:
  - `wait(term, facts)` keeps the phase pending until the bound, then stalls
    by name;
  - `advance(phase)` is the only thing that restarts the clock;
  - the bound is 5 s of wall clock with a floor of 16 calls;
  - the tail is `course_modality_facts`.
- `src/app/src/github_application.cpp` is pinned by the round-trip contract.
  Reseal with a recorded review, and run `application_composition_contract`
  so the new terms stay out of the tests-off Release graph.

## Acceptance

- AC1 `application_uiforetw_selected_diff_contract` passes on Linux and macOS,
  with the strip row still asserted exactly, in order.
- AC2 A too-short comparison fails with a line that names the widgets taking
  the height. This is shown by a deliberately broken run. The card records
  what took the height between `271bb452` and `dec43dda`, and whether the
  floor or the product changed.
- AC3 None of the probe's waits can reach `RUN_BUDGET_EXPIRED` before naming
  why. Each new term fires in a deliberately broken run, or an equivalent
  forced check, and prints its own facts.
- AC4 The round-trip contract passes with the resealed pins, and
  `application_composition_contract` passes.
- Windows remains [[BPR-1033]]'s. This card does not require a Windows pass,
  but its change must not add a Windows-only failure. The Windows offsets must
  still reach `workbenchCaptionButtonsAction` at the end of the row.

## Evidence so far

| Where | Revision | Result | Log |
|---|---|---|---|
| Linux, BPR-0875 | `271bb452` | Pass, 15.06 s | `build/r2-logs/dsp7-0875-twelve-courses.log` (bpr-linux) |
| Linux, BPR-0863 baseline | `dec43dda` | Fail: `actions=28`, `987:842` | `build/r2-logs/bpr0863-baseline2-head.log` |
| Linux, BPR-0863 row-only fix | `dec43dda` + diagnostic | Fail: `987:842` only | `build/r2-logs/bpr0863-diag-uiforetw-row-corrected.log` |
| Linux, BPR-0864 known-red check | `3dca2d7a` candidate | Fail: same two lines | `build/r2-logs/bpr0864-known-red-candidate.log` |
| macOS, BPR-0863 | `1dfe0600` | Fail: `23=refreshQueueAction`, `985:835` | `build/bpr0863-mac-uiforetw.log` (bpr-main-mac) |
| Linux, this card | `4a4c1e40` | Fail: `actions=28`, `987:842`, 2.02 s | `build/r2-logs/bpr1036-uiforetw-head-4a4c1e40.log` |

Logs without a station name are on bpr-main-linux. None are committed.
