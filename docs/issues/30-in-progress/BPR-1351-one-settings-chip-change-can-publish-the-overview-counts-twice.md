---
schema: "implement-rfc.card.v1"
id: "BPR-1351"
title: "One Settings chip change can publish the Overview counts twice"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"bpr-main-linux, BPR-1094's large-review course, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1349--BPR-1351--2371e399c373.json","related_rfcs":["RFC-BPR-0049"],"related_cards":["BPR-1094","BPR-1088","BPR-1089"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7f0430233","evidence":{"kind":"command","path":"tests/end_to_end/large_review_benchmark.cpp","command":"ctest --test-dir build/dev -R '^application_large_review_responsiveness$' -V","detail":"No change publishes twice. The large-review course makes 24 Settings chip changes (12 ticks up to all twelve, 12 unticks back to none), not 23: BPR-1094 took 23 from the test's lower bound 2n-1. Each change has at least one publication (the wait needs a projection carrying the new column count), so 24 publications over 24 changes is exactly one each. Measured per change: per_change=1 x 24, trailing=0, publications=24, at 392826f8f and again rebased at 7f0430233. No settings echo, change-back or window republication. No product change."}},{"criterion":"AC2","outcome":"pass","revision":"7f0430233","evidence":{"kind":"command","path":"tests/end_to_end/large_review_benchmark.cpp","command":"ctest --test-dir build/dev -R '^application_large_review_responsiveness$' -V","detail":"The course counts publications per chip change and asserts exactly 1 for each of 24, with 0 in a 250 ms drain after the last. Controls in github_application.cpp's chip dispatch, each built, run and reverted: M1 publishes twice per request (per_change all 2, 48; fails; the old >= 23 bound passed it at 48); M2 adds one deferred echo to the first change (per_change 1,2,1..., 25; fails, BPR-1094's shape); M3 adds one deferred echo after the last change (trailing=1; fails)."}},{"criterion":"AC3","outcome":"pass","revision":"7f0430233","evidence":{"kind":"command","path":"tests/end_to_end/large_review_benchmark.cpp","command":"ctest --test-dir build/dev -R '^(application_large_review_responsiveness|application_overview_feedback_public_path|application_feedback_counter_costs)$'","detail":"BPR-1094's structural check is kept as its own require with its message unchanged (C6/C8 still name it): over the 24 changes, provider reads 0, observations 0, bodies parsed 0, scopes re-tallied 0, units tallied 0 (96 on-request tallies of the four listed files with no conversation, as before). application_overview_feedback_public_path still asserts +2 publications for two chips."}}]
---

# BPR-1351: One Settings chip change can publish the Overview counts twice

## Finding

In [[BPR-1094]]'s large-review course at `269f7ccff` (515 files, 1,000
threads, the real application, offscreen), 23 Settings chip changes
published the Overview counts 24 times. No GitHub request was made, no body
was parsed and no conversation was tallied. Counts and focus were unaffected.
It was not investigated further.

## Outcome

Each chip change publishes the counts once, or a second publication is shown
to be a deliberate, idempotent republish, documented as such.

## Acceptance

- Find which change published twice and why. Candidates are a settings
  publication echo, the BPR-1088 change-back path, or a window
  republication.
- A test counts publications per chip change and asserts one, with a
  failing-before control. Alternatively, the card records why two is
  correct.
- BPR-1094's structural assertions (zero requests, zero parses, zero
  tallies) still hold.

## Boundaries

No live GitHub; offscreen only.

## Investigated on Linux (bpr-main-linux, 2026-10-08)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high). The
test commit is `7f0430233`, rebased from a run at `392826f8f`. Debug
`-Werror` `build/dev`, GCC 15.2.0, Qt 6.11.2, offscreen. No live GitHub and
no model call. Nothing visual changed, so the frontend-design skill was not
needed.

### Root cause: a miscount, not a double publication

No chip change publishes twice. The large-review course
(`application_large_review_responsiveness`, `large_review_benchmark.cpp`)
makes **24** Settings chip changes, not 23:

- 12 ticks: labels 0 and 1, the measured third, labels 3 to 10, and the
  measured twelfth;
- 12 unticks: labels 11 down to 2, label 1, and the measured last.

BPR-1094 took "23" from the course's lower bound
`publications >= 2 * 12 - 1`, which was never the number of changes.

Each change publishes at least once. The course's wait needs the Overview's
projection to carry the new column count, and only
`publish_overview_feedback` publishes such a projection; no observation runs
during the changes. So 24 publications over 24 changes means exactly one
each. The run confirms it: `per_change=1` for all 24, `trailing=0`,
`publications=24`. That holds at `392826f8f` and again at `7f0430233`. The
code paths are unchanged since BPR-1094's `269f7ccff`.

The path for one change, for the record:

1. The Settings tick goes to the profile's durable write.
2. The published behaviour reaches the page, whose `set_label_columns`
   returns early if the columns are unchanged.
3. `dispatch_feedback_request` is deduplicated by `AskedFeedback`.
4. The window's dispatch calls `publish_overview_feedback` once.

None of the three suspected paths publishes again: no settings echo, no
BPR-1088 change-back and no window republication.

### What changed

Only the test. The course records the Overview's count publications for each
chip change, from the tick to the settled, quiet window that shows it. It
then counts a 250 ms drain after the last change, so a late echo of the last
change is caught too. It asserts:

- 24 changes, each publishing exactly 1;
- 0 publications after the last;
- a total of 24.

BPR-1094's structural assertion stays a separate `require` with its message
unchanged, so its controls C6 and C8 still name it. Over the 24 changes it
checks 0 provider reads, 0 observations, 0 bodies parsed, 0 scopes
re-tallied and 0 units tallied (96 on-request tallies of the four listed
files with no conversation, as before). The old `>= 23` lower bound is
replaced by the exact count. No product source changed, so no pin moved.

### Failing-before controls

There is no product defect to revert, so the new assertion is shown to fail
under mutations that publish too often. Each mutation was in the window's
chip dispatch (`github_application.cpp`, `set_overview_feedback_dispatch`),
built, run and reverted in `build/dev`.

| # | Mutation | Result |
|---|---|---|
| M1 | Publish twice per request | Fails: `per_change` all 2, 48 publications. The old test (`>= 23`), rebuilt from `HEAD` under the same mutation, **passed** with 48. |
| M2 | One deferred (`QTimer::singleShot(0)`) echo on the first change | Fails: `per_change=1,2,1,…`, 25 publications. The echo lands after the first change's wait, in the second change's count; this is the 25-for-24 shape BPR-1094 described. The old bound passes it by arithmetic. |
| M3 | One deferred echo on the last change | Fails: `trailing=1`, 25 publications. |

### Verification

At `7f0430233` (after the rebase over `b864e319d`), `build/dev`:
`application_large_review_responsiveness`,
`application_first_tab_comparison`,
`application_overview_feedback_public_path`,
`application_review_round_trip_composition_contract`,
`test_network_isolation_manifest` and `architecture_boundaries`, 6 of 6.

Before the rebase (`392826f8f` plus the test), 23 of 23 passed:

- architecture and isolation: `architecture_boundaries`,
  `test_network_isolation_manifest` and
  `application_review_round_trip_composition_contract`;
- feedback counters: `application_feedback_counters` and
  `application_feedback_counter_costs`;
- overview, application: `application_overview_feedback`,
  `application_pull_request_overview`,
  `application_overview_feedback_public_path` and
  `application_overview_labels_public_path`;
- overview, Qt: `qt_pull_request_overview_page`, its `chips_scale_125` and
  `_200` rows, and `qt_workbench_overview`;
- settings: `application_workbench_preferences_consumers`,
  `sqlite_workbench_settings_storage`, `qt_settings_window` and its 125% and
  200% rows, `qt_settings_effect_boundary`, `qt_feedback_label_settings` and
  `core_feedback_label_columns`;
- both 100 ms rows: `application_large_review_responsiveness` and
  `application_first_tab_comparison`.

`core-only`: `type_read_binding_contract`, `core_dependency_boundary`,
`core_boundary_contract` and `core_feedback_label_columns`, 4 of 4.

`/usr/bin/clang-format` 22.1.8 `--dry-run --Werror` is clean on the changed
test, `git diff --check` is clean, and the documentation gate passes.

**Pins.** No pinned `src/app` source changed. The round-trip contract passes
without a reseal.

### Documentation

`docs/verification/RFC-BPR-0047-0051.md`'s finding 3 now reads as the
correction. [[BPR-1094]]'s card (parked, coordinator-owned) still says "23
chip changes" and "24 count publications". Its numbers are right except
the change count, which is 24.

### For the coordinator

Ready for the board finish: AC1 to AC3 pass at `7f0430233`. There is no
product change, no pin change and no owner look.
