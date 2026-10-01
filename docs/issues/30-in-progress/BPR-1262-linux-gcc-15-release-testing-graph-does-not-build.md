---
schema: "implement-rfc.card.v1"
id: "BPR-1262"
title: "Linux: the GCC 15 Release graph with BUILD_TESTING=ON does not build (warn_unused_result and -O3 maybe-uninitialized)"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux, BPR-0967's Linux station run, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1262--BPR-1262--b8a0576f8ab5.json","related_cards":["BPR-0967","BPR-0816"]}
---

# BPR-1262: Linux: the GCC 15 Release graph with BUILD_TESTING=ON does not build (warn_unused_result and -O3 maybe-uninitialized)

Found by bpr-main-linux on 2026-10-01. BPR-0967's station handoff configures
`cmake --preset app-release -B build/bpr0967-release -DBUILD_TESTING=ON
-DBPR_DEPENDENCY_OFFLINE=ON` at `cef11b982`. Its build of
`bench_better_pull_review_large_review` stopped at
`designated_review_authority_bootstrap.cpp:965`: GCC does not let
`static_cast<void>` discard glibc's `warn_unused_result` on `linkat` under
`_FORTIFY_SOURCE`.

[[BPR-0816]] and `4519a44f8` made the tests-off Release graph build. Nobody
had built the tests-on Release graph with GCC.

## Observation

A full `ninja -k 0` of that tree at `43831783c` (WSL2, GCC 15.2, 4 min)
fails 11 units:

| Site | Diagnostic |
|---|---|
| `src/application/src/designated_review_authority_bootstrap.cpp:965` (`linkat`) | `-Werror=unused-result` |
| `tests/network_isolation/linux_launcher_fault_shim.cpp:95` (`write`) | `-Werror=unused-result` |
| `tests/sqlite_contract/profile_runtime_lease_tests.cpp:106` (`write`) | `-Werror=unused-result` |
| `tests/application/agent_invocation_host_tests.cpp:222` | `-Wmaybe-uninitialized`: a nested aggregate with a `std::string` member, brace-initialized without naming its type. This is [[BPR-0816]]'s real miscompile shape. |
| `tests/workspace_contract/review_cycle_verification_tests.cpp:119` | `-Wmaybe-uninitialized`: the same shape (`.ClaimedAuthor = { … }`) |
| `tests/core/release_update_tests.cpp:205` | `-Wmaybe-uninitialized` through `std::optional` |
| `tests/application/epoch_application_host_tests.cpp:807` | `-Wmaybe-uninitialized` through `std::expected` |
| `src/app/src/profile_window_layout_port.hpp:79` | `-Wmaybe-uninitialized`: a reassigned `expected<void, E>`, the shape [[BPR-0816]] met in the appearance host |
| `tests/application/github_application_host_tests.cpp:60` and others | `-Wmismatched-new-delete` and `-Wmaybe-uninitialized` |
| `tests/github_contract/github_capability_probe_tests.cpp:1391`, `github_paged_read_tests.cpp:941` | `-Wmaybe-uninitialized` inside glaze's `generic_json` variant |

The application library failed, so its dependents were not compiled. More
sites may appear once these are fixed.

## Scope

- Fix each site where GCC reports it, as [[BPR-0816]] did. Do not use a
  `#pragma`, `-Wno-error` or a changed warning flag. Check results where they
  matter. Use `4519a44f8`'s named `[[maybe_unused]]` local only where a later
  step already observes the failure.
- Reseal the round-trip pin if `src/app` changes.
- Repeat `ninja -k 0` until the whole tests-on Release graph builds.

## Acceptance

- `cmake --build build/bpr0967-release` (app-release, `BUILD_TESTING=ON`, GCC
  15) builds every target with no first-party diagnostic.
- `build/dev` builds clean, and the touched rows pass there.
- The BPR-0967 Linux handoff runs at the fix.
