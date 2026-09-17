---
schema: "implement-rfc.card.v1"
id: "BPR-0854"
title: "Four source-scan contracts are red on main"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep; confirmed on macOS by dsp-004","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0854: Four source-scan contracts are red on main

## Defect

`bpr-linux`'s full sweep at `a1864fa2` found four source-scan rows failing,
and each fails again when run alone
(`docs/verification/dsp4-linux-confirmation.md`). The integrator reproduced
all four on macOS at `20d99914`: `build/r2-logs/dsp4-linux-sweep-mac-classify.log`,
plus a clean `git archive` export for the two script-only checks. No card
recorded any of them. Integration passes have run focused selections, so the
rows stayed red.

1. **`contained_temporary_usage_contract`**
   (`tests/scripts/contained_temporary_usage_tests.py`,
   `test_no_test_creates_a_temporary_through_tempfile`). Thirteen Python course
   sites call `tempfile` instead of `scripts/contained_temporary_support`:
   - `device_binding_relaunch_contract.py` :63, :92 (`1925d0cc`, 2026-09-17);
   - `profile_activation_contract.py` :28, :63;
   - `profile_owner_admission_contract.py` :33, :49;
   - `profile_window_layout_contract.py` :14 (`ad3abf50`, 2026-09-08);
   - `profile_window_restart_contract.py` :13;
   - `profile_window_review_restoration_contract.py` :59;
   - `rfc0015_cli_publish_fixture_tests.py` :15;
   - `rfc0015_cli_read_fixture_tests.py` :28, :76 (`2625472b`);
   - `tests/github_contract/probe_gh_clone_git_bridge.py` :28 (`cdf6175c`).

   [[BPR-0847]] (:28) and [[BPR-0849]] (:33) each added one site while the
   gate was already red. Their owner-only parents must keep working through
   the contained support. On Linux and macOS, the parent must still be
   owner-only when `BPR_TEST_SOCKET_ROOT` is unset.
2. **`msvc_secure_crt_contract`**
   (`test_every_getenv_read_is_spelled_for_msvc`).
   `tests/sqlite_contract/profile_activation_tests.cpp:39` reads
   `std::getenv("BPR_TEST_SOCKET_ROOT")` without the MSVC spelling
   (`a654669c`, 2026-09-08).
3. **`conditional_branch_identifiers`** (`scripts/verify_conditional_branches.py`,
   `CONDITIONAL_BRANCH_IDENTIFIERS_FAILED:11`). All eleven findings are in
   branches this host does not compile:
   - `src/app/main.cpp:8621`: `schedule_native_notification_run` against the
     declared `scheduleNativeNotificationRun` (`47de7ae8`, 2026-09-08). This
     may be a real misspelling in a branch no platform compiles.
   - `src/app/src/github_application.cpp`:
     - :13375-13378 and :13387-13388: `options` and `config` lambda
       init-captures (`a654669c`);
     - :13899-13905: `moves` and `accepted` init-captures (`a5427499`,
       2026-09-15).

     These look like the gate misreading init-captures. The gate was last
     changed in `26cd2f35`.
4. **`dependency_modernization_contract`**
   (`test_dependency_discovery_is_centralized_and_package_based`).
   `tests/end_to_end/CMakeLists.txt:2199` calls
   `find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)` outside
   central discovery (`2df4d110`, 2026-09-12).

## Scope

- Fix each at its cause.
  - Move the Python course sites to `scripts/contained_temporary_support`
    without weakening their private-parent checks.
  - Spell the `getenv` read the way the MSVC gate requires.
  - Use central Python discovery in the end-to-end CMake.
  - For the conditional-branch findings: fix `main.cpp:8621` if it is a real
    misspelling. Teach the gate lambda init-captures only if the rest are
    false positives, and keep it strict otherwise.
- `src/app/main.cpp` and `github_application.cpp` are pinned by the round-trip
  contract. A change there reseals the pins with a recorded review.

## Acceptance

- All four rows pass on macOS and Linux. `msvc_secure_crt_contract` and
  `contained_temporary_usage_contract` also pass on Windows, if they are
  registered there.
- The courses whose files changed still pass on the platforms where they are
  registered.
