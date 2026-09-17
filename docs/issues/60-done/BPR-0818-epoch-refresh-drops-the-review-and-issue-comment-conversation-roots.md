---
schema: "implement-rfc.card.v1"
id: "BPR-0818"
title: "Epoch refresh drops the review and issue-comment conversation roots"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"integration run at the BPR-0810/BPR-0811 merge; confirmed at a clean 8b2bea5b worktree","found_at":"8b2bea5b83639c78f69f431e24a724e0fccdb3e9","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
acceptance: [{"criterion":"application_review_behaviour_contract passes on macOS within its budget, and the cause is named on this card","outcome":"pass","revision":"99712ec3ada8e8482f3a855d3873d20690283f95","evidence":{"kind":"command","detail":"ctest --test-dir build/r2-dev -j1 at the integrated head: application_review_behaviour_contract Passed 87.88 s (build/r2-logs/dsp2-int2-courses.log), budget unchanged. Slice runs passed 93.71 s and 86.44 s. Cause on the card: first bad 4a2be1b1 (BPR-0751), with parent c202d11c passing; the refresh hold outlived the observation terminal and filtered out the review-wide roots; stage 4's overrun was ~22 s of waiting for those roots. The three same-symptom contracts also pass."}},{"criterion":"The Qt conversation courses covering the changed presentation still pass","outcome":"pass","revision":"99712ec3ada8e8482f3a855d3873d20690283f95","evidence":{"kind":"command","detail":"qt_workbench_window_scale_100/125/200, qt_composed_github_scale_100/125/200, qt_github_conversation_panel, qt_conversation_cards_view, qt_github_thread_map_scale_100/125/200 and qt_github_conversation_model_property all pass at the integrated head (dsp2-int2-courses.log). The new unselected-refresh test fails at its terminal assertion with the product change reverted (dsp2-0818c-negative-control-new-test.log). application_large_review_history passes quietly (321.29 s); its thin pulse margin is recorded as a residual risk on this card and on BPR-0673."}}]
---

# BPR-0818: Epoch refresh drops the review and issue-comment conversation roots

## Defect

`application_review_behaviour_contract` (`tests/end_to_end/CMakeLists.txt:1876`)
fails on macOS. It failed at `f33a10c9`, and again, twice, at a clean scratch
worktree of `8b2bea5b`, which is before that merge. Each run reports:

    RUN_BUDGET_EXPIRED: elapsed_ms=40053 budget_ms=40000 over_ms=53 stage=4
    EPOCH_REFRESH_PRESENTATION_IDENTITIES
      expected: review:PRR_fixture_1, review-thread:PRRT_fixture_1, review-comment:PRRC_fixture_1,
                review-thread:PRRT_fixture_3, review-comment:PRRC_fixture_4, review-comment:PRRC_fixture_5,
                review-thread:PRRT_fixture_2, review-comment:PRRC_fixture_2, review-comment:PRRC_fixture_3,
                review-thread:PRRT_fixture_file, review-comment:PRRC_fixture_file, issue-comment:IC_fixture_1
      actual:   the same without review:PRR_fixture_1 and issue-comment:IC_fixture_1
    expected conversation roots 6, actual 4

The marker is printed at `src/app/main.cpp:4427`. After an epoch refresh, the
review summary root and the issue-comment root no longer appear in the
presented conversation. Two things are unknown:

- whether the budget overrun is a cause, a consequence of waiting for roots
  that never arrive, or a separate slowdown;
- when the regression started. A red architecture gate and a nested checkout
  on `main` hid failures like this one for days.

## Scope

- Find the first bad revision and the mechanism. It may be a product
  presentation regression (for example from RFC-BPR-0028 conversation or
  drawer changes, or RFC-BPR-0032 Home and accounts work), or a fixture or
  course drift.
- Fix the product if a person no longer sees review summaries or issue
  comments after a refresh. Change the course only if an accepted RFC decision
  removed those roots, and cite that decision.
- Name what fills the 40 s budget at stage 4. Do not raise the budget to make
  the course pass.

## Acceptance

- `application_review_behaviour_contract` passes on macOS within its budget,
  and the cause is named on this card.
- The Qt conversation courses covering the changed presentation still pass.

## Cause and fix (dsp-002, run-20260916-stations)

**First bad revision:** `4a2be1b1` ("feat: bound RFC-0013 public refresh
projection", [[BPR-0751]]). Its parent `c202d11c` passes in 87.34 s; it
fails in 40.76 s (`build/r2-logs/dsp2-0818-contract-{c202d11c,4a2be1b1}.log`).

**Mechanism: a real product regression.** BPR-0751 added a visual hold on
the comments drawer's file scope during a refresh gap
(`RefreshConversationAnchor` in `workbench_window::publish`). Its
`restoringObservedFile` branch kept the hold past the observation's terminal
publication, on the assumption that "Composition restores the exact file in
its next local publication". Nothing does: a head-moving refresh clears the
diff selection. So the drawer stayed filtered to `review.cpp` with no file
selected, and the two review-wide roots, the review summary and the issue
comment, have no path to match. No accepted RFC decision removed them:
RFC-BPR-0028 scopes the drawer to a file's threads only while a file is the
anchor.

**What filled stage 4's 40 s.** The epoch refresh takes about 7.6 s: about
6.5 s of "Refreshing retained revisions…", then about 1.1 s of "Resolving the
exact comparison…". Every other stage-4 term held at 18.06 s. The remaining
~22 s was waiting for roots that never arrived. The overrun was a
consequence of the bug, not a slowdown, and the budget was not raised.

**Fix (`9eed16c1`).** The hold ends with the gap. The widening that ends it
runs on its own zero-interval `QTimer` turn. Done inline, re-projecting the
1000-thread large review (about 51 ms) pushed
`application_large_review_history`'s refresh-32 pulse to 103.7–105.4 ms,
over its 100 ms limit. The course itself is unchanged. The new Qt test
`test_refresh_without_a_selection_shows_review_wide_roots` fails at its
terminal assertion when the product change is reverted
(`dsp2-0818c-negative-control-new-test.log`).

**Integrated evidence at `99712ec3ada8e8482f3a855d3873d20690283f95`** (`build/r2-logs/dsp2-int2-courses.log`,
`dsp2-int2-history.log`):

- `application_review_behaviour_contract` passes in 87.88 s;
- the three contracts that failed the same way pass:
  `application_overview_since_review_contract` (89.09 s),
  `application_saved_server_contract` (87.80 s) and
  `application_epoch_persistence_github-com` (44.91 s);
- the Qt conversation courses pass: `qt_workbench_window_scale_{100,125,200}`,
  `qt_composed_github_scale_{100,125,200}`, `qt_github_conversation_panel`,
  `qt_conversation_cards_view`, `qt_github_thread_map_scale_{100,125,200}`
  and `qt_github_conversation_model_property`;
- `application_large_review_history` passes on a quiet machine (load 1.07)
  in 321.29 s.

**Residual risk.** The history course's margin is thin. On the quiet
integrated run, refresh 32's longest pulse was 93.3 ms against the 100 ms
limit, and refreshes 20–31 reached 88–89 ms. Before this fix they held at
65–71 ms, so the deferred widening costs about 20 ms per refresh. The
slice's two runs gave 98.6 ms and 92.8 ms. The course may fail under load.
This is recorded on [[BPR-0673]] as the next thing that card's measurement
should break up.
