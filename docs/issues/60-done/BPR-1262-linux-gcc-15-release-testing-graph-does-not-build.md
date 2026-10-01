---
schema: "implement-rfc.card.v1"
id: "BPR-1262"
title: "Linux: the GCC 15 Release graph with BUILD_TESTING=ON does not build (warn_unused_result and -O3 maybe-uninitialized)"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux, BPR-0967's Linux station run, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1262--BPR-1262--b8a0576f8ab5.json","related_cards":["BPR-0967","BPR-0816"]}
acceptance: [{"criterion":"cmake --build build/bpr0967-release (app-release, BUILD_TESTING=ON, GCC 15) builds every target with no first-party diagnostic","outcome":"pass","revision":"c1336c137","evidence":{"kind":"command","detail":"The whole tests-on Release graph builds after five ninja -k 0 rounds, with 0 warning: or error: lines; a rebuild is a no-op. 17 sites in 16 files are fixed at the reporting site, with no pragma, no -Wno-error and no flag change."}},{"criterion":"build/dev builds clean, and the touched rows pass there","outcome":"pass","revision":"c1336c137","evidence":{"kind":"command","detail":"The dev build has 0 warnings. The full dev suite passes 558 of 567; the failures are the 8 known rows (BPR-1255 x6, BPR-1245, BPR-1236) and coding_format_check, which ran mid-change and passes after the fix. After the last edit, the paged-read, probe, architecture, MSVC-CRT, format and round-trip rows pass 6 of 6. clang-format and tidy report nothing on changed lines."}},{"criterion":"The BPR-0967 Linux handoff runs at the fix","outcome":"pass","revision":"c1336c137","evidence":{"kind":"freeform","detail":"The handoff's Release configure and bench target build now succeed at this commit. The BPR-0967 measurement itself is recorded on BPR-0967 and in docs/verification/BPR-0967-station/."}}]
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

## Fix (bpr-main-linux, 2026-10-01, `c1336c137`)

Five `ninja -k 0` rounds of `cmake --build build/bpr0967-release` (app-release,
`BUILD_TESTING=ON`, `BPR_DEPENDENCY_OFFLINE=ON`, GCC 15.2). The first round
failed 11 units, the third round 3 more that the earlier failures had hidden,
and the fifth built everything. 17 sites in 16 files are fixed where GCC
reports them. There is no `#pragma`, no `-Wno-error` and no warning flag
change.

| Class | Sites | Change |
|---|---|---|
| `-Werror=unused-result` (glibc `warn_unused_result` under `_FORTIFY_SOURCE`) | `designated_review_authority_bootstrap.cpp` `linkat` (testing hook); `linux_launcher_fault_shim.cpp` `write`; `profile_runtime_lease_tests.cpp` `write` | A named `[[maybe_unused]]` local, as in `4519a44f8`. In each case a later step already sees the failure: the course that asked for the hard link sees no refusal, the record reader fails to find its directory, and the parent's `read(...) == 1 && 'y'` check fails. |
| [[BPR-0816]]'s nested-aggregate double-destroy shape: a nested aggregate holding a `std::string`, braced without naming its type | `agent_invocation_host_tests.cpp` (`.Workspace`), `review_cycle_verification_tests.cpp` (`.ClaimedAuthor`), `pull_request_overview_page_tests.cpp` (`.Overview`), `src/app/src/github_public_path_fixture.cpp` (`.Correlation`) | The nested type is named. |
| `-O3` `-Wmaybe-uninitialized` through `std::expected` | `src/app/src/profile_window_layout_port.hpp` `store` (the reassigned `expected<void, E>` shape that [[BPR-0816]] met in the appearance host); `epoch_application_host_tests.cpp` history fake | `perform_store` returns each terminal where it is built, and a throw still answers `unavailable`. The fake moves out the value or the failure on its own. |
| `-O3` `-Wmaybe-uninitialized` through `std::optional` | `release_update_tests.cpp` | The candidate is copied out of its optional before it changes. |
| `-O3` `-Wmaybe-uninitialized` on a glaze-read struct | `github_thread_map_tests.cpp` | `thread_fixture_json fixture{ };`. `error_on_missing_keys` still fails a missing key. |
| `-O3` `-Wmaybe-uninitialized` inside glaze `generic_json`'s variant move | `github_paged_read_tests.cpp` (5 sites), `github_capability_probe_tests.cpp` (2 sites) | The finished fixture value is copied into its array (`emplace_back` of an lvalue, a named local) instead of moved or pushed as a temporary. A comment by `fixture_json.hpp`'s aliases says why. |
| `-Wmismatched-new-delete` | `github_application_host_tests.cpp` (55 reports from its replaced `operator delete`) | Both replaced `operator delete` overloads are `[[gnu::noinline]]` under `#if defined(__GNUC__)`, so GCC no longer inlines `free` into delete-expressions whose memory came from `operator new`. The attribute is an inlining hint, not a diagnostic switch. MSVC does not see it. |

`src/app` changed, so the round-trip composition contract's production pin is
resealed with a note (`e14b564f…`).

| Check | Result |
|---|---|
| `cmake --build build/bpr0967-release` (whole tests-on Release graph) | Builds; 0 `warning:` or `error:` lines; a second build is `no work to do` |
| `cmake --build --preset dev` | 0 `warning:` or `error:` lines |
| `ctest --preset dev -j10` (full suite, mid-change, 18 m 23 s) | 558 of 567 pass. Failures: [[BPR-1255]]'s six, the [[BPR-1245]] workshop row and the [[BPR-1236]] restoration row, which were known before this card, and `coding_format_check`, which ran while one file was still unformatted. After the fix it passes, with `core_review_cycle_verification` and `review_cycle_verification`. |
| After the last edit: `github_paged_read*`, `github_capability_probe*`, `architecture_boundaries`, `msvc_secure_crt_contract`, `coding_format_check`, `application_review_round_trip_composition_contract` | 6 of 6 pass |
| `/usr/bin/clang-format` 22 `--dry-run --Werror` on the changed sources; `git diff --check`; `verify_conditional_branches.py`; `DOCUMENTATION_OK` | Clean |
| `tidy.py --paths` on the 15 changed `.cpp` files | No finding on a changed line, after the five `modernize-use-emplace` findings were addressed. The other 20 findings are on existing lines. |

Not built here: Apple clang or MSVC. The C++ changes are portable. The one
GNU attribute is preprocessor-guarded.
