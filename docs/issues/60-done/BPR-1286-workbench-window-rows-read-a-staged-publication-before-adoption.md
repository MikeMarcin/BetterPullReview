---
schema: "implement-rfc.card.v1"
id: "BPR-1286"
title: "Workbench window rows read a staged large publication before adoption; the installed package course calls tempfile.mkdtemp"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1083 implementer, macOS build/dev at 6e69e121d, 2026-10-05","reservation":"docs/issues/_reservations/BPR-1286--BPR-1286--6ee7aaaa9409.json","related_cards":["BPR-1264","BPR-1182","BPR-1083","BPR-0394"],"acceptance":["BPR-1286:window-rows","BPR-1286:contained-temporary","BPR-1286:large-review-kept"]}
acceptance: [{"criterion":"BPR-1286:window-rows","outcome":"pass","revision":"dfd376a3e+workbench_window_tests.cpp","evidence":{"kind":"command","detail":"macOS arm64 build/dev: qt_workbench_window_scale_100/_125/_200 pass (36.6/40.5/48.6 s); before, _100 failed 'the composed surface accepts its exact publication'. publish_fixture now settles geometry (staged fill included) and requires current_result() to be the published result by identity. Control: unchanged test with the BPR-1264 staging threshold raised out of reach passes, so the test assumed a synchronous publish; product unchanged."}},{"criterion":"BPR-1286:contained-temporary","outcome":"pass","revision":"dfd376a3e+installed_cycle_package_course.py","evidence":{"kind":"command","detail":"contained_temporary_usage_contract passes; it failed at installed_cycle_package_course.py:133 tempfile.mkdtemp (BPR-1182, f57c828ac). The root is now make_contained_temporary_directory(prefix, parent=Path.home()), identical to mkdtemp on macOS; --help imports the helper."}},{"criterion":"BPR-1286:large-review-kept","outcome":"pass","revision":"dfd376a3e+both test files","evidence":{"kind":"command","detail":"Optimized tests-on macOS tree (app-release, BUILD_TESTING=ON, Apple clang -O3): 18 large-review rows 18/18, every report BudgetsEnforced true, KnownInputMisses empty, no input failure. Largest gaps ms: large_file_selection 13.8/14.1, large_source_revisit_render 12.3/11.6, approximate_file_selection 20.5, first_tab_file 11.5; largest gated 25.6 (paged_review_and_git_load). DOCUMENTATION_OK."}}]
---

# BPR-1286: Workbench window rows read a staged large publication before adoption; the installed package course calls tempfile.mkdtemp

## Observation

On main at `6e69e121d` (macOS arm64, `build/dev`) four rows fail without any
BPR-1083 change:

- `qt_workbench_window_scale_{100,125,200}` stop at "the composed surface
  accepts its exact publication".
- `contained_temporary_usage_contract` reports
  `tests/end_to_end/installed_cycle_package_course.py:133: tempfile.mkdtemp`.

## Cause

**Window rows: [[BPR-1264]], `ac0d9d1d4`.** A publication of more than 4,096
lines now fills two detached documents over bounded turns and is adopted
only once both are full. Until then `current_result()` is the previous
result and `geometry_pending()` is true. The test's `publish_fixture` read
`current_result()` after one drain of events, before `settle_geometry`, so
it assumed the old synchronous publish. The product is right; the test read
too early. Control: with the unchanged test and the staging threshold raised
out of reach, `qt_workbench_window_scale_100` passes.

**Contained temporary: [[BPR-1182]], `f57c828ac`.** The course made its root
with `tempfile.mkdtemp(dir=Path.home())`. [[BPR-0394]]'s gate forbids
`tempfile`'s creating calls anywhere under `tests/`.

## Outcome

- `publish_fixture` waits for `geometry_pending()` to clear, which covers the
  staged fill, and then requires the surface's result to be the published
  one by identity.
- The course makes its root with
  `make_contained_temporary_directory(prefix=..., parent=Path.home())`. On
  macOS, the only platform it runs on, that is `mkdtemp` in the same place.
- No product change. BPR-1264's staging and its gains stay.

## Acceptance

- **BPR-1286:window-rows.** `qt_workbench_window_scale_{100,125,200}` pass on
  macOS `build/dev`.
- **BPR-1286:contained-temporary.** `contained_temporary_usage_contract`
  passes.
- **BPR-1286:large-review-kept.** The 18 large-review rows pass with budgets
  enforced in an optimized tests-on macOS tree.

## Result (2026-10-05, macOS arm64 M4 Pro, Qt 6.11.2)

- **Window rows.** `build/dev` (Debug): `qt_workbench_window_scale_100`,
  `_125` and `_200` pass, in 36.6, 40.5 and 48.6 s. Before the change, 100
  failed at "the composed surface accepts its exact publication".
- **Control.** With the unchanged test and the staging threshold raised out
  of reach, `_100` passes. This attributes the failure to BPR-1264's staging
  and shows the test, not the product, assumed a synchronous publish. The
  product source is unchanged.
- **Contained temporary.** `contained_temporary_usage_contract` passes, and
  the course's `--help` imports the helper.
- **Large review kept.** An optimized tree with tests on (`app-release`,
  `BUILD_TESTING=ON`, Apple clang `-O3`) passes the 18 large-review rows
  18/18. Every report has `BudgetsEnforced` true, an empty
  `KnownInputMisses` and no input failure. Largest gaps in ms:
  `large_file_selection` 13.8 and 14.1, `large_source_revisit_render` 12.3
  and 11.6, `approximate_file_selection` 20.5, `first_tab_file` 11.5. The
  largest gated gap is 25.6 (`paged_review_and_git_load`).
- **Checks.** clang-format is clean on the changed file, and
  `DOCUMENTATION_OK`.
- **Owed.** Linux and Windows station runs of the same rows. Both changes
  are portable, and the course runs only on macOS.

**Note: Linux run at `1b47f1bac` (station `bpr-main-linux`, WSL2, GCC 15.2.0,
offscreen).**

- **Dev tree** (rebuilt whole, no warnings):
  `contained_temporary_usage_contract` passes.
  `qt_workbench_window_scale_{100,125,200}` still fail on Linux, but at the
  earlier, known [[BPR-1255]] check ("no review-bar control folds into an
  overflow menu at any width"). That check stops the row before it reaches
  this card's publication check, which is why the [[BPR-1264]] regression was
  not visible in this station's run of it. With only that overflow assertion
  bypassed, locally and not committed, all three rows pass (24.5, 26.0 and
  29.5 s), including "the composed surface accepts its exact publication".
  The test was restored and rebuilt.
- **Optimized tests-on tree** (`app-release`, `BUILD_TESTING=ON`, GCC
  `-O3`, run with `-j1` once the station was idle): 18 of 18 large-review
  rows pass in 93.6 s. Every report has `BudgetsEnforced` true,
  `KnownInputMisses` empty and no input failure. Largest gaps in ms:
  `large_file_selection` 23.7–28.7, `large_source_revisit_render` 18.4 and
  24.1, `approximate_file_selection` 27.9, `first_tab_file` 25.5. The
  largest gated gap is 56.2 (`first_tab_base`). The only build warning is
  the vendored SQLite `-Wstringop-overread`.

`build/app-release-tests` was removed. Done is not reopened.
