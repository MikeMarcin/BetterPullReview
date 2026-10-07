---
schema: "implement-rfc.card.v1"
id: "BPR-1303"
title: "After a refresh in Review a newly chosen line draws no draft, so the label-authoring course stalls at its suggestion phase"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"application_label_authoring_contract on macOS at c01e04974 and later; passed at 6984585b9 (2026-10-06)","reservation":"docs/issues/_reservations/BPR-1303--BPR-1303--546a2bd2ce3f.json","related_cards":["BPR-1083","BPR-1084","BPR-1132","BPR-1277","BPR-1292","BPR-1293"],"first_bad_commit":"7778886c9"}
acceptance: [{"criterion":"BPR-1303:bisect","outcome":"pass","revision":"7778886c9","evidence":{"kind":"command","command":"git bisect run (build better_pull_review, one offscreen probe run per step) between 6984585b9 and c01e04974","detail":"good 3d8466f95, 311c9821e, 0f5317a1f, f5a64e041, 5d5de1c02; bad 3754fee9b, 7778886c9. First bad commit 7778886c9 (BPR-1293: Conversations as a fourth modality). BPR-1295, BPR-1089 and BPR-1301 are clear."}},{"criterion":"BPR-1303:cause","outcome":"pass","revision":"bba8a8d26","evidence":{"kind":"observation","path":"src/app/src/github_application.cpp","detail":"A temporary phase-9 diagnostic (removed) showed host selected=none, host generation 4 against publication generation 2, outstanding=0, modality Review. A trace showed two handoff observations (generations 3 and 4) with no file selection after them. Every accepted handoff calls WorkbenchHost->observe, which drops the selected file. A same-head refresh keeps the old diff on screen, and only the return to Review from a tabless page called document_tab_host::refresh. BPR-1293's reveal_conversation_in_review keeps Review after Publish, where the old reveal moved to Discuss."}},{"criterion":"BPR-1303:fix-and-control","outcome":"pass","revision":"bba8a8d26","evidence":{"kind":"command","command":"BPR_1303_CONTROL=1 ctest --test-dir build/dev -R '^application_label_authoring_contract$' (3 runs; temporary switch, removed)","detail":"With represent_refreshed_tab forced off, 3 of 3 runs failed with phase=9 line=1 draft=- comment=0. The same binary without the switch passed. At base ebb2cf11e, 3 of 3 direct probe runs failed with the same signature."}},{"criterion":"BPR-1303:stable","outcome":"pass","revision":"bba8a8d26","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^application_label_authoring_contract$' --repeat until-fail:10","detail":"10 of 10 consecutive passes (3.8-4.8 s each), macOS arm64 Debug, offscreen."}},{"criterion":"BPR-1303:neighbours","outcome":"pass","revision":"bba8a8d26","evidence":{"kind":"command","command":"ctest --test-dir build/dev -j1 -R '<19 neighbouring rows>'","detail":"18 of 19 passed, including application_review_round_trip_composition_contract (resealed, mutation pin unchanged), application_review_behaviour_contract, application_threaded_reply_contract, application_composition_contract, application_large_review_benchmark and architecture_boundaries. application_uiforetw_selected_diff_contract (title_row_exact_order) fails the same way under the control and is not caused by this change."}}]
---

# BPR-1303: After a refresh in Review a newly chosen line draws no draft

## Defect

`application_label_authoring_contract` ([[BPR-1083]], [[BPR-1084]]) fails on
`main` on macOS, every run. The course publishes its labelled comment,
refreshes, and then stops at its first suggested-change phase until the run
budget expires:

```text
PUBLIC_PATH_BUDGET:expired=1 budget_ms=40000
LABEL_AUTHORING_FAILED
LABEL_AUTHORING_DIAGNOSTIC:... | phase=9 line=1 draft=- comment=0
```

Line 1 is selected on the right side, but no draft is drawn there and Comment
stays unavailable ("No editable draft is available"). It passed on this Mac at
`6984585b9` and fails on `c01e04974`.

## Outcome

Find the first bad commit and the cause, fix it in the product or the test,
show a control that fails without the fix, and pass the row 10 times in a row.
Offscreen only.

## Acceptance

- `BPR-1303:bisect`: the first bad commit between `6984585b9` and `c01e04974`.
- `BPR-1303:cause`: the cause, with the facts that show it.
- `BPR-1303:fix-and-control`: the fix, and a control that fails without it.
- `BPR-1303:stable`: 10 consecutive passes of the row at the fix.
- `BPR-1303:neighbours`: rows that share the changed path still pass, and
  the round-trip pins are resealed.

## Bisect

`git bisect run` between `6984585b9` (good) and `c01e04974` (bad). Each step
built `better_pull_review` and ran the probe once offscreen. A pass was good;
the `phase=9 line=N draft=- comment=0` stall was bad. Without the fix the
stall is deterministic: 3 of 3 runs at `ebb2cf11e`.

| Commit | Result |
|---|---|
| `3d8466f95` | good |
| `311c9821e` | good |
| `0f5317a1f` (Claim BPR-1301) | good |
| `3754fee9b` | bad |
| `f5a64e041` (BPR-1301 published commit) | good |
| `5d5de1c02` (Claim BPR-1293) | good |
| `7778886c9` | bad, the first bad commit |

The first bad commit is `7778886c9`, "BPR-1293: Conversations, the pull
request's finder, as a fourth modality". BPR-1295, BPR-1089 and BPR-1301 are
not the cause.

## Cause

The defect is in the product, and it was there before BPR-1293. BPR-1293
made it reachable after every inline publish.

1. Every accepted pull-request handoff calls `WorkbenchHost->observe` with
   the new snapshot's authority. That includes the application's own refresh
   after Publish and the reviewer's Refresh. The workbench host's
   `observe_now` bumps its generation and drops its selected file. When the
   handoff keeps the same head (`preserves_selected_diff`), the earlier diff
   stays on screen.
2. Nothing selects the file again while the modality stays in Review.
   `document_tab_host::refresh` exists for this case. Its header says "The
   pull request refreshed: the active tab shows its exact view again." The
   only caller is the return to Review from a page with no tab
   (`advance_tab_presentation`).
3. A line chosen afterwards reaches `present_review_draft`. That function
   returns early because the workbench generation and selected file no
   longer match the shown publication. No draft is drawn, so Comment says "No
   editable draft is available". Choosing the file again or leaving Review
   recovers.

Before BPR-1293, the reveal after Publish (`reveal_conversation`) moved the
window to Discuss. The course returned to Review, and that return called
`TabHost->refresh()`. BPR-1293 changed the reveal to
`reveal_conversation_in_review`, which keeps Review and opens the thread in
place, so nothing restored the file.

A temporary diagnostic in the phase-9 note confirmed this. It has been
removed. It showed
`host selected=none, host generation 4 / publication generation 2,
outstanding=0, modality=Review`. A trace showed two handoff observations
(generations 3 and 4) after the pick, with no tab load and no file selection
after either one. The pick's own `select_workbench_file_intent` named the
already selected file, so it did nothing.

## Fix

`src/app/src/github_application.cpp` now does two things:

- `accept()` records whether the handoff kept the selected diff
  (`RefreshKeptTabView = preserves_selected_diff(...)`).
- `observe_tab_changes` calls `represent_refreshed_tab()`. Once for that
  handoff, Review's active tab shows its exact view again through
  `TabHost->refresh()` when all of these hold:
  - an active tab names a file the workbench still lists
  - the workbench has observed its files but has no selected file
  - no tab load or Resume is in progress

This is the path the return from Discuss already takes. The driver selects
the file under the new authority, presents the diff and restores the tab's
reading position. The course and its checks are unchanged.

A first version keyed the re-present on the workbench generation alone. It
also fired after a head-moving refresh, where the tab's comparison change
already reloads the tab. That broke `application_review_behaviour_contract`'s
seed: `conversation-roots` was 2 where 4 were expected, and the run hit its
120 s budget at stage 4, 3 of 3 times. The re-present is now limited to a
handoff that kept the diff, and that row passes.

`application_review_round_trip_composition_contract` pins
`github_application.cpp` and the `src/app` manifest. Both are resealed over
the BPR-1086 reseal, and the note is in the file. The mutation pin is
unchanged.

## Evidence

macOS arm64 Debug, `build/dev`, in a worktree off `origin/main` (`efa26ca02`)
that uses the main checkout's CPM cache. Every run was offscreen
(`QT_QPA_PLATFORM=offscreen`). Nothing drove or captured the desktop.

| Run | Result |
|---|---|
| Base `ebb2cf11e`, direct probe, 3 runs | 3 failed, `phase=9 line=1 draft=- comment=0` |
| Control: the candidate with `represent_refreshed_tab()` forced off (temporary environment switch, since removed), ctest, 3 runs | 3 failed, same signature |
| The same binary with the switch unset | passed |
| Candidate, `ctest -R '^application_label_authoring_contract$' --repeat until-fail:10` | 10 of 10 passed (3.8-4.8 s each) |
| Rebased on `origin/main` `b219f3a2a` (fix `bba8a8d26`), 10 runs | 10 of 10 passed; the round-trip (pins match), review-behaviour and threaded-reply rows pass |
| Candidate, 19 neighbouring rows run serially | 18 passed; `application_uiforetw_selected_diff_contract` failed, and it also fails under the control (see Left alone) |

The neighbouring rows were `architecture_boundaries`,
`application_github_contract`, `application_epoch_contract`,
`application_review_submission_contract`,
`application_window_review_restoration`, `application_document_tab_host`,
`qt_workbench_conversation_reveal`, `application_composition_contract`,
`application_large_review_benchmark` (it includes BPR-1293's Conversations
phases), `application_first_tab_comparison`,
`application_workbench_composition_contract`,
`application_review_ledger_composition_contract`,
`application_review_round_trip_composition_contract` (resealed),
`application_defect_composition_contract`,
`application_overview_since_review_contract`,
`application_review_behaviour_contract`, `application_saved_server_contract`
and `application_threaded_reply_contract`. clang-format 23.1.1 reports no
change to `github_application.cpp`.

## Left alone

- `application_uiforetw_selected_diff_contract` fails on macOS with
  `UIforETW light compact navigation omitted title_row_exact_order`. It fails
  the same way with this fix switched off, so this change does not cause it.
  It has no card. The ID budget for this task was one, so it is recorded here
  for the coordinator to file.
- Linux and Windows did not run this row for this card. The change is
  composition logic shared by every platform. The station commands are in the
  hand-off.
