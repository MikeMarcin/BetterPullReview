---
schema: "implement-rfc.card.v1"
id: "BPR-1036"
title: "application_uiforetw_selected_diff_contract is red at HEAD: its exact strip row predates BPR-0990's refreshQueueAction, and the comparison is 842/1000 of the window height against an 850 floor"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-0863's baseline run","found_at":"dec43dda","reproduced_at":["b93adf45","3dca2d7a","1dfe0600 (macOS)","4a4c1e40","c127cb38"],"last_green_linux":"271bb452 (BPR-0875's twelve-course run)","folded_from":["BPR-0864"],"related_cards":["BPR-0859","BPR-0863","BPR-0864","BPR-0990","BPR-1033","BPR-1037","BPR-0246","BPR-0653","BPR-0929","BPR-1021","BPR-1106"],"reservation":"docs/issues/_reservations/BPR-1036--BPR-1036--0e7760d6ea6e.json","candidate":"d3b583d0","height_cause":"RFC-BPR-0037 composition entry row (cc038748, BPR-0929), 20 px between the strip and the epoch line; hidden, the comparison is 862/1000","owner_answer":"2026-09-24, attended session cbd48b91, BPR-0653-entry-row-v001 decision 007a6eb5: RFC-BPR-0028 AC-01 amended to admit the entry rows; the floor is taken from the measured stacks (Linux 842, macOS 835)","height_floor":{"linux":842,"mac":835,"win":850,"source":"measured stacks under RFC-BPR-0028 AC-01 as amended 2026-09-24; win unmeasured, unchanged"}}
acceptance: [{"criterion":"AC1 application_uiforetw_selected_diff_contract passes on Linux and macOS, with the strip row still asserted exactly, in order","outcome":"pass","revision":"b18888aa","evidence":{"kind":"command","detail":"Linux: ctest --preset dev -R '^application_uiforetw_selected_diff_contract$' passes at d3b583d0 (15.16 s) and on this change before the rebases (15.05 s, 15.11 s) and after the first (15.27 s), all four profiles at 842/1000 with navigation_row_exact_order true and no UIFORETW_NAVIGATION_ROW_UNEXPECTED. macOS owed: bpr-main-mac runs the card's named command at d3b583d0 or later, expecting a pass at 835/1000. macOS (bpr-main-mac, 2026-09-24): after cmake --build --preset dev --target app_better_pull_review, the named ctest passes at b18888aa (contains d3b583d0) in 69.88 s with UIFORETW_SELECTED_DIFF_CONTRACT_OK, no UIFORETW_NAVIGATION_ROW_UNEXPECTED and no UIFORETW_CAPTURE_LAYOUT_FAILED; all four profiles at 835/1000 height, 985/1000 width, navigation_row_exact_order true."}},{"criterion":"AC2 A too-short comparison fails with a line that names the widgets taking the height; the card records what took it and whether the floor or the product changed","outcome":"pass","revision":"d3b583d0","evidence":{"kind":"observation","detail":"What took the height: RFC-BPR-0037's composition entry row (cc038748), the only new row in 271bb452..dec43dda. The floor changed, not the product, under the owner's RFC-BPR-0028 AC-01 amendment (decision 007a6eb5): 842 Linux, 835 macOS, 850 Windows unchanged and unmeasured. Deliberately broken runs on Linux: the capture's floor at 843 fails with UIFORETW_CAPTURE_LAYOUT_FAILED:987:842 naming the whole stack, composeReviewActionButton/composeInlineDraftActionButton@49+20 in it; the script's floor alone at 843 fails its own assertion; break 101 named reviewReadiness@49+26. Both 843 edits reverted byte for byte."}},{"criterion":"AC3 None of the probe's waits can reach RUN_BUDGET_EXPIRED before naming why; each new term fires with its own facts","outcome":"pass","revision":"3d941d47","evidence":{"kind":"command","detail":"Linux: thirteen diagnostic breaks, one per term, each failed by its own term (eleven at 5.0 s through the shared keep_waiting, the two walk terms at once), none with RUN_BUDGET_EXPIRED. HEAD's probe with the same gesture break expired the 40 s budget with only stage=2. Logs build/r2-logs/bpr1036-diag-break{1..13}.log, bpr1036-headcontrol-break2.log."}},{"criterion":"AC4 The round-trip contract passes with the resealed pins, and application_composition_contract passes","outcome":"pass","revision":"d3b583d0","evidence":{"kind":"command","detail":"Linux: application_review_round_trip_composition_contract failed on the drifted full digest before the reseal and passes resealed over e6134dbc..d3b583d0 (1.65 s rebased, 1.59 s at d3b583d0); application_composition_contract passes at d3b583d0 (442.53 s) and before the rebases (452.64 s). The macOS round-trip run is in AC1's owed command. macOS: application_review_round_trip_composition_contract passes at b18888aa (11.51 s, REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK)."}}]
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
| Linux, this card, HEAD | `c127cb38` | Fail: `actions=28`, `987:842`, 2.08 s | `build/r2-logs/bpr1036-uiforetw-head-c127cb38.log` |
| Linux, candidate | `c127cb38` + candidate | Fail at the layout floor only, naming the entry row; 2.05 s | `build/r2-logs/bpr1036-c2-rows.log` |
| Linux, candidate, entry row hidden (diagnostic) | `c127cb38` + candidate + break 100 | Pass, 862/1000 in all four profiles, 20.90 s and 15.55 s | `build/r2-logs/bpr1036-diag-break100.log`, `…-break100-rerun.log` |

Logs without a station name are on bpr-main-linux. None are committed.

## Diagnosis and candidate (bpr-main-linux, 2026-09-23)

Station bpr-main-linux: WSL2 Ubuntu, GCC 15.2, Qt 6.11.2, offscreen with the
basic Windows style. Every run used a fresh `HOME` and fresh XDG roots, with
`DISPLAY` and `WAYLAND_DISPLAY` unset. The base is `c127cb38` (this card's
claim, on `669a5a58`).

### Cause 1: the strip row

The exact row now carries `window->queue_refresh_action()` at 23, straight
after `github_refresh_action()`, and everything after it moves along by one.
That is 28 actions off Windows. On Windows the same `lead` and `trailing`
offsets make 30, and `workbenchCaptionButtonsAction` is still the last one, at
`named(28)`, index 29. The row is still asserted whole, in order and exactly,
and hidden actions are not skipped. The `[[BPR-0859]]` comment no longer gives
a count; a `[[BPR-1036]]` paragraph says why the hidden action is in the row.
With the candidate, the capture prints no `UIFORETW_NAVIGATION_ROW_UNEXPECTED`
line, and the diagnostic run below records `navigation_row_exact_order: true`
in all four profiles. Against HEAD's expectation of 27, the same window prints
`actions=28` (`bpr1036-uiforetw-head-c127cb38.log`). That is the negative
control.

### Cause 2: what took the height

`UIFORETW_CAPTURE_LAYOUT_FAILED` now prints the window, the menu bar, the
strip, the epoch line, the comparison and the status bar, as `y+height` or
`hidden`/`absent`. After that it prints every shown widget stacked above or
below the comparison's branch, at every level up to the window (`stack=`).
Then comes the height no stacked row accounts for (`unstacked=`: margins and
spacing), the widgets beside the comparison (`beside=`), and the modality facts.
At the candidate:

```text
UIFORETW_CAPTURE_LAYOUT_FAILED:987:842 width_floor=750 height_floor=850
window=1280x1000 menu_bar=0+19 strip=19+30 epoch_line=69+50
comparison=0,139 1264x842 status_bar=981+19
stack=workbenchMenuBar@0+19,workbenchPrimaryToolbar@19+30,composeReviewActionButton@49+20,composeInlineDraftActionButton@49+20,epochComparisonControl@69+50,diffWorkbenchStatus@119+14,QStatusBar@981+19
unstacked=6 beside=diffSharedScrollBar@1264+16 modality=Review host_modality=Review centre=comparison …
```

The rows add up to the window: 19 + 30 + 20 + 50 + 14 + 6 + 842 + 19 = 1000.

**What took the height is RFC-BPR-0037's composition entry row**: Compose
review and Draft comment or reply, from `cc038748` ([[BPR-0929]]). The review
surface inserts it above the epoch line (`review->insertLayout(1, …)`), and it
is 20 px at y 49. It is the only row in the stack that is new in
`271bb452..dec43dda`:

- `cc038748` is in `dec43dda` and not in `271bb452`.
- The menu bar (`f1ca3058`, [[BPR-0457]]), the full-width epoch line
  (`4ebb0009`, [[BPR-0737]]) and the comparison's status line (`da39b3ad`)
  are all ancestors of `271bb452`.
- [[BPR-0795]]'s readiness line is in the range too, but it is hidden here.

It is the same row that [[BPR-0653]] and [[BPR-1021]] found between the strip
and the epoch line on Windows (`composeReviewActionButton@30+20`).

**Counterfactual.** A diagnostic build hid only those two buttons; it is not
committed. With the candidate otherwise unchanged, the comparison was 862/1000
in all four profiles and the whole row passed: 20.90 s, and 15.55 s on the
rerun. `capture-facts.json` for light is kept as
`build/r2-logs/bpr1036-diag-break100-light-capture-facts.json`.

So the entry row is the whole of the Linux height regression, and together
with the strip row it is the whole failure on Linux.

**Limit.** `271bb452` was not rebuilt here. It predates the Qt 6.11.2 pin
(`92c4e159`), and this station caches only 6.11.2. The 862 is measured at
HEAD with the row removed, not at `271bb452`. BPR-0875's pass log from
`271bb452` is on bpr-linux.

**AC2's broken run.** A second diagnostic build kept the entry row hidden and
forced BPR-0795's readiness line on (break 101). It failed with its own name:
`…:987:836 … stack=…,reviewReadiness@49+26,epochComparisonControl@75+50,…`
(`build/r2-logs/bpr1036-diag-break101.log`). The line names whichever row
takes the room.

### Product or floor: the owner's call, already asked on BPR-0653

The floor is not stale, so this card does not lower it:

- 850/1000 of the window's height is the owner's space budget from
  [[BPR-0246]]: "every surface other than the two code panes and their gutter
  is on a strict space budget in the review state", and "the focused state must
  not reserve large persistent regions for any non-diff surface".
- The course script asserts the same 850 on its own
  (`comparison_height LESS 850`).
- Lowering it would admit the entry row by the back door.

This card does not move the entries either:

- Where they belong is already the owner's pending decision, ask 3 on
  [[BPR-0653]] (restated at `919e2154`, and tracked for [[BPR-1021]]).
  Either RFC-BPR-0028 AC-01 and the chrome paragraph are amended to admit the
  review surface's entry row, or the two entries move out of it, for example
  into the strip's review controls menu.
- RFC-BPR-0037's own layout study put the Compose review entry "in toolbar"
  (`conversation-surface-layout-study.md`).
- The owner accepted the Compose pages natively (`rfc0037-compose-pages`),
  but that accept did not place the entry row.
- Moving the entries here would answer that question without the owner, in
  code the Mac integrator is currently correcting for RFC-BPR-0037.

Each answer settles this card:

- **The entries move out of the row.** The floor stays at 850. Re-run this
  row on Linux and macOS; on Linux it is expected to pass at 862.
- **AC-01 is amended to admit the row.** The owner's BPR-0246 budget then
  admits it too. This card takes the floor from the measured stacks: 842 on
  Linux, plus the macOS figure. It records the amendment here, changes the
  floor in both the capture and the course script, and adds the entry row
  to `docs/ux/workbench-specification.md`'s primary layout.

So the card is Blocked on `owner-entry-row-placement` until BPR-0653's ask 3
is answered. **When the owner is asked, tell them this answer also decides
this card's floor.** No attended session was started and no owner question
was put by this dispatch.

### Cause 3: the waits

`awaiting_observation`, both gesture-qualified caps and the six silent
`512U` caps now return through BPR-1037's shared `keep_waiting`. That is
BPR-0864's shape: `wait(term, facts)` and `advance(phase)`, 5 s of wall clock
with a floor of 16 calls, and the `course_modality_facts` tail. `advance` is
the only thing that restarts the clock, and every phase change now goes
through it. The probe state gains a `detail::fixture_probe_wait`.

A publication wait takes its term from whether the drawer's selection
registered: `…_SELECTION_STALLED` if it did not, `…_PUBLICATION_STALLED` if
it did. Every diff term prints the same facts:

- the path;
- the file selections, or the intent dispatches, against their expected
  counts;
- the current row;
- begins, publications against the expected count, and failures;
- the last publication's path and both blobs against the expected blobs;
- whether the comparison holds a result, and whether that result is the
  publication's;
- the diff and epoch jobs.

The change-group walk's cap and its refusal now fail as
`UIFORETW_RC_CHANGE_WALK_EXHAUSTED` and `…_REFUSED`. Both print how far the
walk went, the group count, the target and selected group indexes, and Next
change's enablement. The tick-bounded, already-named waits are unchanged:
`UIFORETW_REVIEW_PANES_HIDDEN`, both `…_ROLE_PAINT_STALLED`, the reveal
refusals and `UIFORETW_FOCUSED_LAYOUT_STALLED`.

Each term fired in its own deliberately broken run of the full course. The
diagnostic patch (`build/r2-logs/bpr1036-diagnostic-patch.py`, selected by
`BPR1036_BREAK`) was applied, built, run, and reverted. None of the logs holds
`RUN_BUDGET_EXPIRED`.

| Break | What was broken | Term | Waited | Run elapsed |
|---|---|---|---|---|
| 1 | the handoff never reaches `WorkbenchHost->observe` | `UIFORETW_OBSERVATION_STALLED` | 5001 ms | 14.9 s |
| 2 | the Resource.h drawer selection is never made | `UIFORETW_RESOURCE_SELECTION_STALLED` | 5000 ms | 7.7 s |
| 3 | Resource.h's first publication is dropped | `UIFORETW_RESOURCE_PUBLICATION_STALLED` | 5009 ms | 6.4 s |
| 4 | published, never computed | `UIFORETW_RESOURCE_BLOBS_STALLED` | 5003 ms | 6.4 s |
| 5 | Next file is never triggered | `UIFORETW_SUPPORT_PUBLICATION_STALLED` | 5009 ms | 6.4 s |
| 6 | Support.cpp published, never computed | `UIFORETW_SUPPORT_RESULT_STALLED` | 5007 ms | 6.4 s |
| 7 | Previous file is never triggered | `UIFORETW_RESOURCE_RETURN_PUBLICATION_STALLED` | 5004 ms | 6.4 s |
| 8 | the return publication, never computed | `UIFORETW_RESOURCE_RETURN_BLOBS_STALLED` | 5006 ms | 6.3 s |
| 9 | the .rc drawer selection is never made | `UIFORETW_RC_SELECTION_STALLED` | 5000 ms | 6.3 s |
| 10 | the .rc publication is dropped | `UIFORETW_RC_PUBLICATION_STALLED` | 5002 ms | 6.2 s |
| 11 | the .rc published, never computed | `UIFORETW_RC_BLOBS_STALLED` | 5005 ms | 6.3 s |
| 12 | forced: the walk entered and never stepped | `UIFORETW_RC_CHANGE_WALK_EXHAUSTED` (walked=8, groups=6) | — | 1.5 s |
| 13 | forced: the walk's step refused | `UIFORETW_RC_CHANGE_WALK_REFUSED` | — | 1.4 s |

Logs: `build/r2-logs/bpr1036-diag-break{1..13}.log`.

### Pinned-code review and reseal

This is the implementer's review of the pinned diff, recorded as the
contract's reseal notes require. It is not an independent review.

The only pinned file changed is `src/app/src/github_application.cpp`: the
full-application digest and the production manifest. `git log
669a5a58..c127cb38 -- src/app/` is empty, so the reviewed range is exactly
this change.

Every hunk is in a `BPR_BUILD_TESTING` region:

- the two Qt includes, which move into the testing-only include block;
- the UIforETW probe state;
- the new `report_capture_layout` and its two floor constants, in the
  fixture-access anonymous namespace;
- `exercise_uiforetw_selected_diff`;
- `capture_uiforetw_selected_diff_evidence`.

The edits read window, host, diff-host, epoch and fixture state. They write
only to standard output and standard error. Every product action the probe
and the capture take is unchanged: the modality press, both drawer
selections, Next and Previous file, Next and Previous change, the walk, the
drawer toggles and the grabs. The floors are unchanged at 750 and 850. No
product, credential, provider, vault, transport or mutation path changes. The
mutation-branch pin is unchanged at `a7e2081e…d1e8a`.

New values:

- full application
  `53da7b64776c4452b834b016e8e41adbfc0053410fe594445e6e15c91b399d2a`;
- production manifest
  `a9239f72bca76c6823a0d90a3423072bfc2a1616670f968def39acc53fe8e82a`.

Both were computed by replicating the contract's LF-normalised SHA-256. The
same computation reproduces the previous pins from `c127cb38`'s tree. Before
the reseal, the contract failed on the drifted full digest
(`bpr1036-roundtrip-unsealed.log`). That is the negative control.

**Documentation impact: none.** The change is test-only diagnostics.
`src/app/AGENTS.md` already requires a waiting stage to name its condition.
The layout rule and the floors are unchanged, so
`docs/ux/workbench-specification.md` is unchanged. It changes only if the
owner amends AC-01, as described above.

### Verification at the candidate (bpr-main-linux)

Every command ran with `QT_QPA_PLATFORM=offscreen`, a fresh `HOME` and fresh
XDG roots, and no display. The build is
`cmake --build --preset dev --target app_better_pull_review`.

| Check | Result | Log |
|---|---|---|
| `ctest --preset dev -R '^application_uiforetw_selected_diff_contract$'` | **Fail, as expected until the owner decides:** no row line; `UIFORETW_CAPTURE_LAYOUT_FAILED:987:842` naming the entry row; 2.14 s | `bpr1036-c2-final-rows.log` |
| the same row, entry row hidden (diagnostic break 100) | Pass, 862/1000, all four profiles | `bpr1036-diag-break100{,-rerun}.log` |
| the same row, breaks 1–13 (diagnostic) | each fails by its own term at 5.0 s, or at once for the walk; no `RUN_BUDGET_EXPIRED` | `bpr1036-diag-break{1..13}.log` |
| HEAD's probe (`c127cb38`) with break 2 (diagnostic) | `RUN_BUDGET_EXPIRED:elapsed_ms=40019 … stage=2`, 40.48 s, no term | `bpr1036-headcontrol-break2.log` |
| `application_review_round_trip_composition_contract` | Pass, 1.68 s (resealed); failed on the full digest before the reseal | `bpr1036-c2-final-rows.log`, `bpr1036-roundtrip-unsealed.log` |
| `application_composition_contract` (the tests-off Release graph and its forbidden bytes) | Pass, 537.55 s (its timeout is 600 s) | `bpr1036-composition-contract-c2.log` |
| `application_workbench_composition_contract`, `application_github_public_path_uiforetw_74` | Pass, 3.11 s and 1.29 s | `bpr1036-c2-final-rows.log` |
| `architecture_boundaries`, `test_network_isolation_manifest` | Pass | `bpr1036-c2-boundaries.log` |
| `_coding-standard/scripts/tidy.py --paths src/app/src/github_application.cpp` | `clang-tidy passed.` (6 m 21 s) | `bpr1036-tidy-c2.log` |
| `_coding-standard/scripts/format_check.py --clang-format /usr/bin/clang-format` (22.1.8), `git diff --check` | Pass | `bpr1036-format-c2.log` |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | `bpr1036-docgate-*.log` |

The diagnostic builds were reverted. The candidate's full digest was checked
against the new pin before the final build. The final rows ran on that
build.

### Acceptance state

- **AC1** (the row passes on Linux and macOS, with the strip row asserted
  exactly, in order): **pending**. The strip row passes on Linux. The layout
  floor waits on the owner's entry-row decision above. With the entry row
  gone, the whole row passes on Linux. macOS has not run.
- **AC2** (a too-short comparison names what took the height; the card
  records what took it and whether the floor or the product changed):
  **naming and record pass on Linux**, from the candidate's own failure and
  break 101. **Floor or product: pending**, on the owner's answer.
- **AC3** (no wait reaches `RUN_BUDGET_EXPIRED` before naming why; each new
  term fires with its own facts): **pass on Linux**. There are thirteen breaks,
  one per term; 12 and 13 are forced checks. The same gesture break against
  HEAD's probe expires the budget.
- **AC4** (the round-trip contract passes with the resealed pins, and
  `application_composition_contract` passes): **pass on Linux**.
- **Windows** stays [[BPR-1033]]'s. The only Windows-specific edit is the
  offset: the caption buttons are `named(28)`, index 29, the last of 30. This
  station cannot compile the `_WIN32` branch. The Windows capture still stops
  first at `UIFORETW_CAPTURE_FIRST_TARGET_FAILED`, before the row check.

### On main

The candidate is `3d941d47` on `main`: the change, the reseal and this record.
It was rebased onto `239f3861`, which changed only another card, so its source
is the tree every check above ran against. The card is Blocked on
`owner-entry-row-placement`.

### Evidence still owed

1. **The owner's answer to [[BPR-0653]]'s ask 3.** It decides the floor, as
   set out above. Relay it with that ask; this card asks no separate question.
2. **macOS (bpr-main-mac), at `3d941d47` or later:**
   `ctest --preset dev -R '^(application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract)$' --output-on-failure`.
   - Expected: no `UIFORETW_NAVIGATION_ROW_UNEXPECTED`. The UIforETW row fails
     only at `UIFORETW_CAPTURE_LAYOUT_FAILED:985:<h>`, and its `stack=` names
     the macOS rows, `composeReviewActionButton` among them (the menu bar
     should read `hidden` or be absent, being merged into the system bar).
   - The round-trip row passes. Record `<h>` and the stack here; if AC-01 is
     amended, the macOS floor is taken from it.
   - If the owner moves the entries instead, the row is expected to pass at
     850.
3. **Windows (bpr-main-win):** any
   `cmake --build --preset dev --target app_better_pull_review` at
   `3d941d47` or later, to compile the `_WIN32` branch. Then
   `ctest --preset dev -R '^application_uiforetw_selected_diff_contract$'`.
   The expected result is unchanged: BPR-1033's
   `UIFORETW_CAPTURE_FIRST_TARGET_FAILED`, with no new Windows-only failure.
4. **After the answer, on Linux and macOS:** re-run
   `application_uiforetw_selected_diff_contract`, expecting a pass, plus the
   round-trip contract if the pinned file changes again.

## Windows: the `_WIN32` branch compiles; the row stops where BPR-1033 records, with nothing new (bpr-main-win, 2026-09-23)

The run used `7c1dbb9c`, which contains `3d941d47`. It was
`build/windows-dev` (Visual Studio generator, MSVC 19.51, Debug, Qt 6.11.2),
rebuilt first. The build exited 0 with no `error` and no `warning` line, so
the `_WIN32` strip-row branch compiles: 30 actions on Windows, with the
caption buttons last at `named(28)`. Then
`ctest --preset windows-dev -j1 --output-on-failure -V -R
'^application_uiforetw_selected_diff_contract$'` ran. The row fails in
9.51 s, and the contract reports `UIforETW light capture failed (1)`
(`application_uiforetw_selected_diff_contract.cmake:93`). The fixture's
output is:

```text
PUBLIC_PATH_FAILED
PUBLIC_PATH_STAGE:2
PUBLIC_PATH_VERDICT:presentation=0 shutdown_complete=1 cleanup_failed=0
UIFORETW_EXACT_COMMIT_PAIR_OK … UIFORETW_SELECTED_DIFF_TRACE_OK   (all twelve, as before)
UIFORETW_SELECTED_DIFF_EVIDENCE_FAILED
stderr: UIFORETW_CAPTURE_FIRST_TARGET_FAILED
```

**The stopping line** is `UIFORETW_CAPTURE_FIRST_TARGET_FAILED`, the same as
[[BPR-1033]] recorded at `c4130c3a`.

**Nothing is new.** There is no `UIFORETW_NAVIGATION_ROW_UNEXPECTED`, no
`UIFORETW_CAPTURE_LAYOUT_FAILED`, no stall term from [[BPR-1037]]'s bounded
waits, and no `RUN_BUDGET_EXPIRED`. The capture stops at the first target,
before it reaches the strip row or the height floor, so on Windows this
card's two causes cannot yet be observed.

**Relation to BPR-1033.** No work on BPR-1033 itself has been done from this
station yet. Its AC1, naming which of the nine clauses fails, is still open,
and this run adds nothing there beyond confirming the stop at this head. The
logs are station records under `build/logs/bpr-1036-main-win/` and are not
committed.

## macOS: the strip row holds; the row stops only at the height floor, 835/1000 (bpr-main-mac, Mac batch 6, 2026-09-23)

This is owed item 2. The source is `8595684c`, which contains `3d941d47`. The
checkout held one more local commit that changes only RFC and card documents,
so the built tree is `8595684c`'s. Apple Silicon, AppleClang, Qt 6.11.2,
`build/dev` Debug, rebuilt incrementally first (exit 0, no `warning:` or
`error:` line). The command was the one the card names, with `-V`:
`ctest --preset dev -V -R '^(application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract)$' --output-on-failure`.

| Row | Result |
|---|---|
| `application_uiforetw_selected_diff_contract` | **Failed as expected**, 14.60 s, at the height floor only |
| `application_review_round_trip_composition_contract` | Passed, 12.02 s, `REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK` |

The UIforETW row prints every marker from `UIFORETW_RESOURCE_GESTURE_OK` to
`UIFORETW_SELECTED_DIFF_TRACE_OK`, then `UIFORETW_SELECTED_DIFF_EVIDENCE_FAILED`
and this, as its only failure line:

```text
UIFORETW_CAPTURE_LAYOUT_FAILED:985:835 width_floor=750 height_floor=850
window=1280x1000 menu_bar=0+20 strip=20+30 epoch_line=72+50
comparison=0,143 1262x835 status_bar=978+22
stack=workbenchMenuBar@0+20,workbenchPrimaryToolbar@20+30,composeReviewActionButton@50+22,composeInlineDraftActionButton@50+22,epochComparisonControl@72+50,diffWorkbenchStatus@122+15,QStatusBar@978+22
unstacked=6 beside=diffSharedScrollBar@1262+18 modality=Review
```

Against the expectations in owed item 2:

- **No `UIFORETW_NAVIGATION_ROW_UNEXPECTED`.** Met. The log has no
  `UIFORETW_NAVIGATION_ROW` line at all, so the 28-entry row matched exactly,
  with `refreshQueueAction` in place.
- **The row fails only at `UIFORETW_CAPTURE_LAYOUT_FAILED:985:<h>`, and
  `stack=` names the macOS rows, `composeReviewActionButton` among them.**
  Met, with `<h>` = **835**. The rows add up to the window:
  20 + 30 + 22 + 50 + 15 + 6 + 835 + 22 = 1000. The entry row is 22 px here,
  against 20 on Linux.
- **The menu bar should read `hidden` or be absent.** **Not met, and the
  expectation was wrong for this row.** It reads `menu_bar=0+20` and
  `workbenchMenuBar@0+20` is in the stack. The row runs under
  `QT_QPA_PLATFORM=offscreen` on macOS as on Linux (the log's
  `GITHUB_FIXTURE_QT_PLATFORM:offscreen`), and the offscreen platform has no
  native menu bar to merge into, so Qt lays the bar out inside the window.
  On the Cocoa platform the bar would move to the system bar; this row never
  runs there.
- **The round-trip row passes.** Met.

**For the floor decision.** macOS's comparison is 835/1000. That is 7 px
shorter than Linux's 842, because four rows are taller here: the menu bar
(+1), the entry row (+2), the status line (+1) and the status bar (+3).
With the entry row hidden, the
same arithmetic gives 835 + 22 = 857/1000, so moving the entries would pass
the 850 floor on macOS too, by 7 px. This was worked out from the stack and
was not run. If AC-01 is amended instead, 835 is the macOS reading. It
matches `1dfe0600`'s `985:835`, so the macOS height has not moved since
BPR-0863 measured it.

The same row also failed in this station's full sweep at `239f3861`, which
is before `3d941d47`, with the old `UIFORETW_NAVIGATION_ROW_UNEXPECTED:actions=28`
and `985:835` ([sweep record](../../verification/sweep-2026-09-23-mac.md)).
The log is `build/logs/mac-batch6/bpr1036-mac-rows.log`, a station record
that is not committed. This note does not move the card, which stays Blocked
on the owner's entry-row answer.

## Owner answer, 2026-09-24: AC-01 is amended, so the floor moves (attended session cbd48b91)

The owner answered [[BPR-0653]]'s ask 3 in attended session
`cbd48b91000b00f1e72e1ad62c7c384c` (packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589). Candidate `BPR-0653-entry-row-v001`, fingerprint
`308902012d73…`, decision `007a6eb5-adf3-427c-8473-547f9d0c4d9e`,
2026-09-24T18:54:09Z, `actor: human`: **select**, verbatim:

> Amend RFC-BPR-0028 AC-01 and the chrome paragraph to admit the entry rows
> between the strip and the epoch line

Note, verbatim:

> We should do a UX design iteration with the /frontend-design skill to figure
> out where best to put these. For now you can ammend the card but open a
> follow-up for the ux design.

This is the second of the two answers this card set out under "Product or
floor", so the card leaves `owner-entry-row-placement` and is **Ready**:

- RFC-BPR-0028 AC-01 and its chrome paragraph now admit the composition entry
  row and, while a pull request loads, the readiness line. The owner's
  [[BPR-0246]] budget admits the row with them.
- The work is the one this card named for that answer: take the floor from the
  measured stacks (842/1000 on Linux, 835/1000 on macOS, both with the entry
  row at 20 and 22 px), change it in both the capture and the course script
  (`comparison_height LESS 850`), record the amendment here, and add the entry
  row to `docs/ux/workbench-specification.md`'s primary layout.
- Then re-run the row on Linux and macOS (owed item 4), expecting a pass, and
  the round-trip contract if `github_application.cpp` changes again. That run
  is also what [[BPR-1033]]'s AC3 waits for.
- [[BPR-1106]] is the design round the note asks for. If it moves the entries
  out of the row, the floor returns toward 850, and that card says so.

## The floor from the measured stacks (bpr-main-linux, 2026-09-24)

This follows the owner's answer above, as recorded there. The station is
bpr-main-linux: WSL2 Ubuntu, GCC 15.2, Qt 6.11.2, `build/dev`, offscreen with
the basic Windows style (`QT_STYLE_OVERRIDE=Windows`, the row's own
environment). Every run used a fresh `HOME` and fresh XDG roots under `/tmp`,
with `DISPLAY` and `WAYLAND_DISPLAY` unset. The work began on `a1c35078`,
this card's claim. Before it was pushed it was rebased twice. The first
rebase, onto `ac1c37ad`, brought two changes. One is [[BPR-0897]]'s comment
above `select_observed_review_mutation` (`d393bb5c`), in the same file, with
its own reseal of the same pins. The other is [[BPR-0986]]'s Colours card
change (`8f0ec40b`), which is in `src/adapters/qt`, outside the pinned scope.
The second rebase, onto `7df4cb41`, brought only cards.

### The change

The floor is the measured stack's, in both the capture and the course script:

| Platform | Height floor | Stack it comes from |
|---|---|---|
| Linux | 842/1000 | this station: 19 + 30 + 20 + 50 + 14 + 6 + **842** + 19 = 1000 |
| macOS | 835/1000 | bpr-main-mac, above: 20 + 30 + 22 + 50 + 15 + 6 + **835** + 22 = 1000 |
| Windows | 850/1000, unchanged | no stack measured; see below |

- **Capture** (`src/app/src/github_application.cpp`).
  `uiforetw_capture_height_floor` is now one branch per platform: `__APPLE__`
  835, `_WIN32` 850, otherwise 842. Its comment says why. The width floor
  stays at 750. `UIFORETW_CAPTURE_LAYOUT_FAILED` and `report_capture_layout`
  are unchanged, so the line still names the stack; its `height_floor=` is now
  the platform's floor.
- **Course script**
  (`tests/end_to_end/application_uiforetw_selected_diff_contract.cmake`). It
  still asserts the floor on its own, now as `bpr_comparison_height_floor` by
  host (`CMAKE_HOST_APPLE` 835, `CMAKE_HOST_WIN32` 850, otherwise 842), in both
  of its height comparisons. Its failure message now prints `height_floor=`.
- **Windows keeps 850.** Neither the recorded answer nor the dispatch names a
  Windows figure, and no Windows stack has been measured. On Windows the strip
  is the title row and there is no menu bar row ([[BPR-0653]]), so the entry
  row fits inside the owner's 850 budget: [[BPR-1033]]'s committed run passed
  this row at 850 in all four profiles. Keeping 850 lowers nothing without a
  measurement and adds no Windows-only failure. Measuring the Windows stack
  and moving its floor to it is a one-line change on each side, if wanted.
- **Specification.** The primary layout in
  `docs/ux/workbench-specification.md` now names the chrome above the
  comparison in the amended AC-01's order: the strip, the entry rows, the
  epoch line. It also gives the measured floors this course holds.

### Pinned-code review and reseal

This is the implementer's review of the pinned diff, as the contract's reseal
notes require. It is not an independent review.

The only pinned file changed is `src/app/src/github_application.cpp`, which
moves the full-application digest and the production manifest. The pins
matched the tree at `e6134dbc`, after BPR-0897's reseal: that tree's file
hashes to BPR-0897's full pin, `4cbedb68…`. So the reviewed range is
`e6134dbc..d3b583d0`, and `d3b583d0` is the only change to the pinned scope in
it: `git diff e6134dbc d3b583d0 -- src/app/` is the one hunk below. The two
reseal notes sit side by side in the contract, BPR-0897's first.

There is one hunk, in the `BPR_BUILD_TESTING` fixture-access anonymous
namespace. `uiforetw_capture_height_floor` goes from one constant to the
platform branch above, with a `\details` comment, and the width floor's
declaration loses one space of alignment. The constants are read only by the
capture's layout comparison and its failure line. Every product action the
probe and the capture take is unchanged. No product, credential, provider,
vault, transport or mutation path changes. The mutation-branch pin is
unchanged at `a7e2081e…d1e8a`.

New values, printed by the contract's own `BPR_PRINT_PINNED_DIGESTS` mode on
the rebased tree:

- full application
  `fb638412a13a54dfcb1ecff46b23bcdbb3e112f796238e8e2152497abe26b92b`;
- production manifest
  `77cecb38b7ee325c1d1b5b1357bca10f49fd6401cf7d13e5dce6ea56d036c331`.

Before the reseal, the contract failed on the drifted full digest in 0.01 s
(`bpr1036-floor-roundtrip-unsealed.log`, before the rebase). That is the
negative control.

### Verification

The build is `cmake --build --preset dev --target app_better_pull_review`
(exit 0, no `warning:` or `error:` line). The logs are station records under
`build/r2-logs/` on bpr-main-linux and are not committed.

The first seven rows ran before the rebases, on `a1c35078` plus this change.
The rebases added BPR-0897's comment to the capture's file, outside the
capture, and BPR-0986's Colours card, which the capture does not reach, so
those rows stand. After the first rebase the application was rebuilt, and the
four rows in the "rebased" line ran again. `application_composition_contract`
also ran again at `d3b583d0`.

| Check | Result | Log |
|---|---|---|
| `ctest --preset dev -V -R '^application_uiforetw_selected_diff_contract$' --output-on-failure` | **Pass**, 15.05 s: `UIFORETW_SELECTED_DIFF_CONTRACT_OK`. All four profiles hold `comparison_height_ratio_permille` 842, width 987 and `navigation_row_exact_order` true | `bpr1036-floor-uiforetw.log` |
| Negative control 1: the capture's Linux floor set to 843, rebuilt | **Fails as it should**, 1.99 s. The capture exits 1 in light with `UIFORETW_CAPTURE_LAYOUT_FAILED:987:842 width_floor=750 height_floor=843 window=1280x1000 menu_bar=0+19 strip=19+30 epoch_line=69+50 comparison=0,139 1264x842 status_bar=981+19 stack=workbenchMenuBar@0+19,workbenchPrimaryToolbar@19+30,composeReviewActionButton@49+20,composeInlineDraftActionButton@49+20,epochComparisonControl@69+50,diffWorkbenchStatus@119+14,QStatusBar@981+19 unstacked=6 beside=diffSharedScrollBar@1264+16 modality=Review …` | `bpr1036-negctl-843-capture.log` |
| Negative control 2: only the script's Linux floor set to 843, capture at 842 | **Fails as it should**, 2.11 s: `UIforETW light is not a native pane-dominant focused review: ratio=987x842 computed=987x842 comparison=1264x842 crop=1264x842 height_floor=843`. The script's own assertion holds without the capture's | `bpr1036-negctl-843-script.log` |
| Both controls reverted | Byte-for-byte: both files' SHA-256 match the candidate's (`977f7c7b…` before the rebase, `8a23cb65…`), and the binary was rebuilt from the candidate before the rows below | `bpr1036-candidate-sha256.txt`, `bpr1036-revert-build.log` |
| `application_review_round_trip_composition_contract` | Fails on the drifted full digest before the reseal (0.01 s); **passes** after it (1.63 s) | `bpr1036-floor-roundtrip-unsealed.log`, `bpr1036-floor-final-rows.log` |
| `application_uiforetw_selected_diff_contract`, `application_workbench_composition_contract`, `application_github_public_path_uiforetw_74`, resealed | **Pass**: 15.11 s, 3.03 s, 1.21 s | `bpr1036-floor-final-rows.log` |
| `application_composition_contract` (the tests-off Release graph and its forbidden bytes) | **Pass**, 452.64 s (its timeout is 600 s) | `bpr1036-floor-composition-contract.log` |
| Rebased (`ac1c37ad` plus this change, the same sources as `d3b583d0`), rebuilt and resealed: `application_uiforetw_selected_diff_contract`, `application_review_round_trip_composition_contract`, `application_workbench_composition_contract`, `application_github_public_path_uiforetw_74` | **Pass**: 15.27 s, 1.65 s, 3.05 s, 1.23 s. All four UIforETW profiles hold 842/1000 and the exact row. The capture file's SHA-256 is `fb638412…`, the new full pin | `bpr1036-floor-rebased-rows.log` |
| `application_composition_contract` at `d3b583d0` | **Pass**, 442.53 s | `bpr1036-composition-contract-d3b583d0.log` |
| `/usr/bin/clang-format` 22.1.8 `--dry-run --Werror` on the capture, `git diff --check` | Clean | — |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | `bpr1036-floor-docgate-*.log` |

`clang-tidy` was not rerun. The change adds no function, expression or name,
only a constant's value per platform. The last tidy of this file passed at
`3d941d47` (`bpr1036-tidy-c2.log`).

### Acceptance state

- **AC1** (the row passes on Linux and macOS, with the strip row asserted
  exactly, in order): **pass on Linux**, at 842/1000 with the exact row, in all
  four profiles. **macOS is owed**; see below.
- **AC2** (a too-short comparison fails with a line naming the widgets; the
  card records what took the height and whether the floor or the product
  changed): **pass**. RFC-BPR-0037's entry row took the height (recorded
  above). **The floor changed, not the product**, under the owner's AC-01
  amendment. Negative control 1 is a deliberately broken run naming the stack.
- **AC3** (no wait reaches `RUN_BUDGET_EXPIRED` unnamed): **pass**,
  unchanged since `3d941d47`. This change does not touch the probe's waits.
- **AC4** (the round-trip contract passes resealed, and
  `application_composition_contract` passes): **pass on Linux**. The macOS
  round-trip run rides on the same command as AC1.
- **Windows** stays [[BPR-1033]]'s. Its floor is unchanged at 850, the value
  its committed pass ran against.

The card stays **In Progress**. AC1 names macOS, so the Mac's run is part of
acceptance, not a confirmation that can follow a finish.

### On main

The change, the reseal and this record are `d3b583d0` on `main`, pushed on
top of `7df4cb41`. The capture file there hashes to the new full pin,
`fb638412…`. At `d3b583d0` itself, `application_uiforetw_selected_diff_contract`
passes in 15.16 s at 842/1000 in all four profiles, and
`application_review_round_trip_composition_contract` passes in 1.59 s. This is
[[BPR-1033]]'s AC3 Linux run, recorded there
(`build/r2-logs/bpr1033-ac3-linux-d3b583d0.log`).

### Evidence still owed

1. **macOS (bpr-main-mac), at `d3b583d0` or later.** First
   `cmake --build --preset dev --target app_better_pull_review`, then:

   ```sh
   ctest --preset dev -V -R '^(application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract)$' --output-on-failure
   ```

   Expected:
   - Both rows pass. The UIforETW row prints
     `UIFORETW_SELECTED_DIFF_CONTRACT_OK`, with no
     `UIFORETW_NAVIGATION_ROW_UNEXPECTED` and no
     `UIFORETW_CAPTURE_LAYOUT_FAILED`.
   - Each profile's
     `build/dev/tests/end_to_end/uiforetw-selected-diff-contract/data/evidence/<profile>/capture-facts.json`
     holds `comparison_height_ratio_permille` 835 and
     `navigation_row_exact_order` true.
   - The round-trip row prints `REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK`.

   If the macOS comparison measures under 835, the failure line names the
   stack. Record it here: it would be a new row taking height, not a floor to
   lower. A pass completes AC1 and AC4 on macOS, and the card can finish.
2. **Windows (bpr-main-win), not required by this card.** The next Windows
   build compiles the `_WIN32` branch, whose value is the 850 it replaces. The
   expected result of the row is unchanged: it passes, as [[BPR-1033]]
   recorded.

## macOS: both rows pass at `b18888aa` (bpr-main-mac, Mac batch 7, 2026-09-24)

The owed item 1, run on macOS 26.6.2 arm64, `build/dev`, at `b18888aa`
(which contains `d3b583d0`), with a clean working tree:

```sh
cmake --build --preset dev --target app_better_pull_review
ctest --preset dev -V -R '^(application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract)$' --output-on-failure
```

| Row | Result |
|---|---|
| `application_uiforetw_selected_diff_contract` | **Passed**, 69.88 s; printed `UIFORETW_SELECTED_DIFF_CONTRACT_OK` |
| `application_review_round_trip_composition_contract` | **Passed**, 11.51 s; printed `REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK` |

Neither `UIFORETW_NAVIGATION_ROW_UNEXPECTED` nor
`UIFORETW_CAPTURE_LAYOUT_FAILED` appears in the log. Each profile's
`capture-facts.json`, written by this run (13:15 local):

| Profile | `comparison_height_ratio_permille` | `comparison_width_ratio_permille` | `navigation_row_exact_order` | `window_controls_rect_in_row` |
|---|---|---|---|---|
| light | 835 | 985 | true | 0,0,0,0 |
| dark | 835 | 985 | true | 0,0,0,0 |
| high-contrast-light | 835 | 985 | true | 0,0,0,0 |
| high-contrast-dark | 835 | 985 | true | 0,0,0,0 |

The macOS comparison measures exactly the 835 floor, with the strip row
asserted exactly and in order. **AC1 passes on Linux and macOS**, and the
macOS round-trip run completes **AC4**. With AC2 and AC3 already passing,
every criterion is met and the card finishes. The log is a station record,
not committed.
