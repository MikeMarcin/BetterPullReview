---
schema: "implement-rfc.card.v1"
id: "BPR-1121"
title: "Fifty test sources end a failed expectation with std::exit and leave their scratch roots, as sqlite_profile_activation did"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux", "mac", "win"]
metadata: {"found_by":"BPR-1118's source scan (its 'Seen elsewhere, not changed here' section), re-scanned at HEAD 45191097","reserved_title":"Thirty test sources end a failed expectation with std::exit and leave their scratch roots, as sqlite_profile_activation did","related_cards":["BPR-1118","BPR-1113","BPR-1051","BPR-1103","BPR-0803"],"reservation":"docs/issues/_reservations/BPR-1121--BPR-1121--2eea9dcdb3a9.json"}
---

# BPR-1121: Fifty test sources end a failed expectation with std::exit and leave their scratch roots, as sqlite_profile_activation did

## Why

[[BPR-1118]] fixed one test source, `sqlite_profile_activation`. Its `expect`
printed `FAILED:` and then called `std::exit(EXIT_FAILURE)`, which runs no
destructor of a local object. So every failing run left its `/tmp` scratch
roots behind. The fix made `expect` throw and `main` catch it, so a failure
unwinds through the owners that remove the roots.

That card's scan found thirty other sources with the same shape. The title
reserved for this card says thirty. This station saw one of them leak on
2026-09-24: `application_device_flow_cli_choice` failed on Linux (see
[[BPR-0803]]) and left `/tmp/device_flow_cli_choice-nsdREo`, a whole
application data root, because its `QTemporaryDir` is never destroyed.

## The re-scan at HEAD (`45191097`)

**Method.** A test source is in the class when a failure helper in it
(`expect`, `require`, `fail` or a one-off site) ends with `std::exit`, and
it makes a root in the system temporary directory:
`std::filesystem::temp_directory_path()`, `mkdtemp`, `QTemporaryDir` or
`QDir::tempPath()`.

- **Fifty-two sources match.** [[BPR-1118]]'s thirty are the ones that use
  `temp_directory_path` or `mkdtemp`. Its scan did not count
  `QTemporaryDir`, which leaks the same way: twenty-two more sources.
- **Two of the thirty are false positives**, so the class is fifty:
  - `tests/application/mcp_context_helper_tests.cpp` keeps its scratch in a
    function-local static, which `std::exit` does destroy ([[BPR-1051]]).
  - `tests/application/epoch_application_host_tests.cpp` only names a path
    under the temporary directory and creates nothing there.
- **Seen, not in the class:** `tests/sqlite_contract/storage_root_tests.cpp`
  and `appcontainer_private_descriptor_tests.cpp` also end with `std::exit`,
  but their scratch sits under a directory the registration passes, in the
  build tree.

The fifty, by directory, with the rows that run them on Linux:

| Directory | Sources | Roots |
| --- | --- | --- |
| `tests/sqlite_contract/` | `anonymous_epoch_authority`, `appearance_settings_storage`, `contained_catalog_probe`, `device_binding_storage`, `epoch_catalog_behavior`, `epoch_catalog_storage`, `private_store_open`, `profile_registry_storage`, `profile_runtime_lease`, `profile_storage_root`, `queue_enrollment_storage`, `release_maintenance_files`, `review_ledger_storage`, `review_mutation_recovery_storage`, `review_submission_recovery_storage`, `window_review_restoration_storage`, `workbench_settings_storage` (17) | scratch classes; two bare roots in `main` |
| `tests/git_contract/` | `anonymous_public_read`, `epoch_maintenance_integration`, `git_cache_contract`, `git_process_contract`, `haves_trimmed_pack_integration` (5) | `temporary_root` classes |
| `tests/github_contract/` | `gh_cli_transport`, `github_cli_process_runner`, `live_review_submission_provider`, `gh_cli_git_smart_transport` (macOS only) (4) | classes and `QTemporaryDir` |
| `tests/application/` | `designated_review_authority_bootstrap` (1) | a scratch class |
| `tests/adapters_qt/` | `agent_launch_canary`, `markdown_body_view` (2) | bare directories and marker files; `QTemporaryDir` |
| `tests/end_to_end/` | `application_logging`, `attended_recent_replay`, `device_binding_launch_renewal`, `device_binding_restart`, `device_flow_cli_choice` (macOS row), `github_application_liveness`, `personal_discovery_public_path`, `profile_empty_desktop_native_menu_bar` (macOS only), `profile_management_public_path`, `profile_multi_window_public_path`, `profile_second_window_close_public_path`, `profile_startup_public_path`, `profile_two_account_isolation_public_path`, `release_maintenance_public_path`, `release_update_public_path`, `release_update_recovery`, `release_update_transaction` (macOS only), `setup_destination_public_path`, `windows_profile_storage_startup_contract` (Windows only) (19) | `QTemporaryDir` data roots; one scratch class |
| `tests/logging/` | `quill_logging` (1) | `QTemporaryDir` |
| `tests/syntax_contract/` | `syntax_classification_contract` (1) | a fixed-name canary directory |

Four of the fifty are built only on macOS or Windows, so this station
cannot compile them.

## What a throwing `expect` must not do

A failure that unwinds is not always safe where `std::exit` was. The audit
found these hazards in the fifty:

- **A helper thread still running.** A joinable `std::thread` destroyed while
  unwinding calls `std::terminate`. A `std::jthread` joins, and some of them
  wait on a condition the failing path never sets, so the join never
  returns. For example, `git_process_contract` has `require` calls while an
  `index_pack` thread is blocked on a cancellation that comes after them.
- **A forked child.** After `fork` the child's thread is the parent's, so a
  throw in the child unwinds into the parent's frames. It would run the
  parent's destructors, including the one that removes the shared root.
  Five of the sqlite sources, `git_process_contract`,
  `github_cli_process_runner` and `designated_review_authority_bootstrap`
  fork.
- **A destructor.** Four `direct_database`/`fake_cache_session` destructors
  call `expect`. A throw from a destructor calls `std::terminate`.
- **A Qt event dispatch.** A throw from a slot or an event handler is
  undefined behaviour in Qt. The device-flow course's consent timer can
  reach `expect`, and two courses call it in startup and quit callbacks
  that the event loop delivers.
- **Code under test that catches it.** Product code has several hundred
  `catch (...)` handlers, and some courses call `expect` from hooks the
  product invokes. A caught throw would let the course run on and possibly
  pass.

## Scope

- **One shared helper**, `tests/support/test_expectation.hpp`, replaces each
  source's own `std::exit` ending, grouped by the helper the sources share:
  - `expect` prints the `FAILED:` line and throws;
  - `fail()` ends a failure whose line the caller has already printed;
  - `main` hands anything that reaches it to one handler, which names an
    unexpected exception and returns the failing exit.
- **It unwinds only where that is safe.** On any other thread, in a forked
  child, while an exception is already unwinding, or inside a scope marked
  `exit_on_failure`, it ends the process with `std::exit` as before.
- **A caught failure still fails.** If an expectation's exception is
  swallowed, the run still exits with failure. A failure that does not
  finish unwinding within a bound ends the process instead of hanging.
- **Every root is owned from the moment it exists.** That covers a scratch
  root whose constructor fails part-way, and the bare roots in `main`.
- **Each hazard above is marked or made safe**, and listed here.
- **Record what still leaves a root**: a kill, a timeout, a crash, and each
  marked scope.

## Acceptance

- AC1 The fifty sources end every failed expectation through the shared
  helper. No source in the class ends one with its own `std::exit`, and the
  exceptions are the hazard scopes listed on this card.
- AC2 Trials that force an expectation to fail while a root exists leave no
  root for every Linux row of the class. The row still fails with its
  `FAILED:` line. The same trials at `HEAD`'s sources, with `std::exit`,
  leave the roots. That run is the negative control that shows the trial
  can see a leak.
- AC3 An exception from the standard library fails the row with a named
  `FAILED:` line and removes the roots.
- AC4 A marked hazard scope still ends with `std::exit`, and does not hang. A
  swallowed expectation still fails the run. A trial shows each of these.
- AC5 The passing rows are unchanged on Linux: every Linux row of the class
  passes, and prints the same lines as at `HEAD`.
- AC6 The change is portable: the sources build and their rows pass on
  macOS and Windows. The four sources built only there are compiled first
  there.
- AC7 Gates: `/usr/bin/clang-format` 22, scoped `tidy.py
  --warnings-as-errors` on the touched sources, `git diff --check`,
  `msvc_secure_crt_contract` and `DOCUMENTATION_OK`.

## Boundaries

- Test sources and one test-support header only. No product source, no row
  registration and no composition pin changes.
- Every run is through `ctest`, offscreen, with `DISPLAY`, `WAYLAND_DISPLAY`
  and `DBUS_SESSION_BUS_ADDRESS` unset, and with fresh absolute `HOME` and
  XDG roots. Nothing is created in `/run/user/1000`.
- A trial's own leftovers are removed by the exact names the run printed.
  Nothing else in `/tmp` is removed.
