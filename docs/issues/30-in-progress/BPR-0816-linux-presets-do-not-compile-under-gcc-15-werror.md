---
schema: "implement-rfc.card.v1"
id: "BPR-0816"
title: "Linux presets do not compile under GCC 15.2 with -Werror"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"found_by":"bpr-linux during task-20260917-lnx-evidence","found_at":"17f495fea40f6985c0d2f6b0430c7a71062f8085","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
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
