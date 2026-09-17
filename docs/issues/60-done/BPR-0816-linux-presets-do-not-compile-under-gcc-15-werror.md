---
schema: "implement-rfc.card.v1"
id: "BPR-0816"
title: "Linux presets do not compile under GCC 15.2 with -Werror"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux during task-20260917-lnx-evidence","found_at":"17f495fea40f6985c0d2f6b0430c7a71062f8085","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
acceptance: [{"criterion":"cmake --preset app-release and cmake --preset dev configure and build with the stock flags on the Linux station under GCC 15.2, with no demoted diagnostic classes","outcome":"pass","revision":"4883dbd9","evidence":{"kind":"artifact","detail":"bpr-linux stock --fresh configure and --clean-first build of both presets: app-release 429/429, dev 1241/1241, no first-party diagnostics, empty CMAKE_CXX_FLAGS; docs/verification/BPR-0816-linux-gcc15-stock-build.md. Integrated on main as fb84f41c."}},{"criterion":"The macOS dev build and the focused tests covering the touched sources still pass","outcome":"pass","revision":"99712ec3ada8e8482f3a855d3873d20690283f95","evidence":{"kind":"command","detail":"r2-dev (AppleClang -Werror) built 99712ec3ada8e8482f3a855d3873d20690283f95 with exit 0 and no warnings (dsp2-int2-build.log). The 16 focused tests the station passed on Linux all pass on macOS (dsp2-int2-units.log, dsp2-int2-courses.log). Of the 7 pre-existing Linux failures, 5 pass on macOS. The 2 appearance contracts that fail with DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED after all appearance stages pass were identical before and after this diff on Linux; they are classified pre-existing and carded as BPR-0823 (Linux-only remainder BPR-0824)."}}]
---

# BPR-0816: Linux presets do not compile under GCC 15.2 with -Werror

## Defect

On the WSL2 Ubuntu station, GCC 15.2 with glibc's `_FORTIFY_SOURCE=3` cannot
build either Linux preset at `17f495fe` with the repository's `-Werror`
(`cmake/Warnings.cmake`). The station's evidence builds for [[BPR-0622]],
[[BPR-0799]] and [[BPR-0731]] had to demote four diagnostic classes to
warnings. Code generation was unchanged, but no stock Linux build exists.

`app-release` (tests disabled):

- `-Wmaybe-uninitialized` in `profile_startup_admission.cpp`,
  `syntax_classification_application_host.cpp`,
  `appearance_preferences_application_host.cpp`, `review_ledger_database.cpp`
  and `github_mutation_contract.cpp`;
- `-Wdangling-reference` at `github_capability_probe.cpp:1468`, which the
  station judges a false positive;
- `-Werror=unused-result` in `unix_agent_supervisor.cpp:117`, already fixed by
  station commit `9bbe3604` (merged in `a9076de1`).

`dev` (tests enabled), in addition:

- `-Wdangling-pointer=` in `profile_registry_storage_tests.cpp:97,195`,
  `queue_enrollment_storage_tests.cpp:550`,
  `github_mutation_contract_tests.cpp:432`,
  `live_review_submission_provider_tests.cpp:222` and
  `workbench_window_tests.cpp:6268`;
- `-Wunused-function` in `dif_runtime_tests.cpp:169,174,183`.

The exact per-site messages are in
[the BPR-0622 Linux evidence](../../verification/BPR-0622-linux-native-evidence.md).

## Scope

- Fix each site at its source: initialize, restructure the lifetime, or
  remove dead code. Suppress a diagnostic only for a demonstrated false
  positive, and then narrowly at that site with the reason. Do not demote a
  warning class globally.
- Keep macOS (clang) and Windows (MSVC) builds unchanged.

## Acceptance

- `cmake --preset app-release` and `cmake --preset dev` configure and build
  with the stock flags on the Linux station under GCC 15.2, with no demoted
  diagnostic classes.
- The macOS dev build and the focused tests covering the touched sources still
  pass.

## Linux station evidence, 2026-09-17 (task-20260917-lnx-build, dispatch dsp-001-r2)

bpr-linux fixed every site at `4883dbd9`. The full record is
[the Linux stock build evidence](../../verification/BPR-0816-linux-gcc15-stock-build.md).

- **Criterion 1: met on Linux.** `cmake --preset app-release --fresh` and
  `cmake --preset dev --fresh`, each followed by a `--clean-first` build, both
  exit 0 under GCC 15.2. The cache holds an empty `CMAKE_CXX_FLAGS`, and no
  first-party diagnostic prints.
  - app-release: 429 of 429 steps in 100 s.
  - dev: 1241 of 1241 steps in 149 s.
  - No warning class is demoted, and the diff has no `#pragma`.
- **How the sites were fixed.**
  - The dangling reference and the `-Wmaybe-uninitialized` sites in the
    application hosts are fixed by restructuring at the site.
  - The two adapter sites turned out to be a **real GCC 15.2 miscompile**, not
    only a warning. A brace-initialized nested aggregate member is destroyed
    twice when a later member's initializer throws; AddressSanitizer reports a
    double free. Naming the member's type avoids it.
  - The test `-Wdangling-pointer=` sites are GCC heuristics. Each is fixed by
    naming the expected value.
  - The `dif_runtime` helpers are defined only on Apple, where their one caller
    is.
- **Criterion 2: Linux half only. Not Done.**
  - Of 23 focused Linux tests for the touched sources, 16 pass. The other 7
    fail identically on the pre-change tree, so none is a regression.
  - The clang 22 proxy passes on all 12 touched translation units, and so do
    the `clang-format` 22 and `clang-tidy` gates.
  - An independent review found nothing blocking.
  - **The macOS dev build and the macOS focused tests cannot run on this
    station.** They stay open. Main should finish the card after its macOS
    build of the merge.
- **Portable files touched.**
  - Product sources: `src/adapters/github/src/github_capability_probe.cpp`,
    `src/adapters/github/src/github_mutation_contract.cpp`,
    `src/adapters/sqlite/src/review_ledger_database.cpp`,
    `src/application/src/profile_startup_admission.cpp`,
    `src/application/src/appearance_preferences_application_host.cpp`,
    `src/application/src/syntax_classification_application_host.cpp`.
  - Tests: `tests/sqlite_contract/profile_registry_storage_tests.cpp`,
    `tests/sqlite_contract/queue_enrollment_storage_tests.cpp`,
    `tests/github_contract/github_mutation_contract_tests.cpp`,
    `tests/github_contract/live_review_submission_provider_tests.cpp`,
    `tests/adapters_qt/workbench_window_tests.cpp`,
    `tests/adapters_qt/dif_runtime_tests.cpp`.

## macOS half, 2026-09-17 (dsp-002)

main merged `linux-lane` `4e4fd6b9` `--no-ff` as `fb84f41c`. The `r2-dev`
tree (AppleClang, `-Werror` from `cmake/Warnings.cmake`) then built
`99712ec3ada8e8482f3a855d3873d20690283f95` with exit 0 and no `warning:` line (`build/r2-logs/dsp2-int2-build.log`).

The focused set the station ran, on macOS at that head:

- **The station's 16 Linux passes pass here too.**
  `build/r2-logs/dsp2-int2-units.log` covers `github_capability_probe`,
  `github_mutation_contract`, `live_review_mutation_provider`,
  `live_review_mutation_restart_recovery`, `live_review_submission_provider`,
  `live_review_submission_restart_recovery`, `sqlite_review_ledger_storage`,
  `application_review_ledger_host`, `application_profile_startup_admission`,
  `application_appearance_host`, `application_appearance_profile_consumers`,
  `sqlite_profile_registry_storage`, `sqlite_queue_enrollment_storage` and
  `qt_dif_runtime`. `build/r2-logs/dsp2-int2-courses.log` covers
  `application_threaded_reply_contract` and
  `application_profile_startup_public_path`.
- **Of the station's seven pre-existing Linux failures, five pass on macOS:**
  `qt_workbench_window_scale_{100,125,200}`,
  `application_review_ledger_composition_contract` and
  `application_appearance_gesture_contract`. [[BPR-0824]] owns the
  workbench and ledger failures, which occur only on Linux; [[BPR-0823]]
  covers the gesture contract's Linux seed failure.
- **The other two fail on macOS as well:**
  `application_appearance_theme_customization_contract` and
  `application_appearance_store_fallback_contract`, both with
  `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED` after every appearance stage has
  passed. The station showed their Linux output identical before and after
  this card's diff, so they are classified as pre-existing and unrelated to
  it. [[BPR-0823]] owns them. BPR-0816 is not claimed to have fixed them.
