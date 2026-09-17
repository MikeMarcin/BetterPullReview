# BPR-0820 Windows confirmation

Station `bpr-win` (JaegAW, `windows-native`, branch `windows-lane`), task
`task-20260917-win-private-roots`, step 1. [[BPR-0820]] is integrator-owned
and Done on macOS; this file is evidence only.

**Base.** `6c44b6eaedfc7a752f0ab78255c2403bc4f406e4`, after a full
`cmake --preset windows-dev` and `cmake --build build\windows-dev --config Debug`,
which succeeded.

**Host.** Windows 11 Home 10.0.26200; Visual Studio 18 Community, MSVC
14.51.36231; Qt 6.10.3 `msvc2022_64`. The shell token is not elevated, and no
person's BetterPullReview instance was running.

**Command.** `ctest --preset windows-dev -R "^(<the fifteen rows>)$" --output-on-failure -V`.
Log: `build/logs/task-20260917-win-private-roots/ctest-bpr0820.log`.

"Contained" means the row runs under `better_pull_review_network_sandbox.exe`
(the Windows AppContainer). "Uncontained" means it is pinned
`WINDOWS_UNCONTAINED`.

## Results

| Row | Registration | Result | First marker |
|---|---|---|---|
| `application_public_path` | uncontained | Passed, 2.75 s | `AUTOMATED_PROFILE_PRIVATE_OK`, `PUBLIC_PATH_OK` |
| `application_public_path_ghes_cli_unavailable` | uncontained | Passed, 2.92 s | |
| `application_device_authorization_public_path` | uncontained | Passed, 3.15 s | |
| `application_device_authorization_unavailable` | uncontained | Passed, 6.99 s | |
| `application_composition_capabilities_inside_boundary` | contained | **Failed** | `AUTOMATED_PROFILE_PRIVATE_FAILED` |
| `application_composition_capabilities_product_run` | contained | **Failed** | `AUTOMATED_PROFILE_PRIVATE_FAILED` |
| `application_device_authorization_ghes_refusal` | uncontained | Passed, 3.05 s | |
| `application_device_authorization_release_contract` | uncontained | Passed, 7.16 s | |
| `application_anonymous_public_preview_unavailable` | uncontained | Passed, 3.23 s | |
| `application_anonymous_public_preview_release_contract` | uncontained | Passed, 2.91 s | |
| `application_anonymous_public_preview_enabled_probe` | uncontained | Passed, 2.93 s | |
| `application_anonymous_public_preview_live_candidate_wiring` | uncontained | Passed, 2.96 s | |
| `application_render` | uncontained | Passed, 3.53 s | |
| `runtime_process_surface` | uncontained | Passed, 3.42 s | `RUNTIME_SURFACE_OK` |
| `runtime_audit_contract` | contained | **Failed**, 0.51 s | 18 tests: 1 failure, 3 errors, 1 skipped |

**The script's `mkdtemp` parent was accepted on Windows.** `runtime_process_surface`
passes with `--private-data-root`. No `startup_failure` value was printed by
any row.

## The two capability rows

Both fail **only when contained**. Run by hand, uncontained, with the same
arguments (`--self-test --composition-capability-probe --network-denied
--automated-run-budget-ms 8000`), both print `AUTOMATED_PROFILE_PRIVATE_OK`
and then their expected answer:

- **With `BPR_AUTOMATED_TEST_BOUNDARY=1`:** `COMPOSITION_TEST_BOUNDARY:inside`.
  Exit 0.
- **With the marker unset,** as the product row configures it:
  `COMPOSITION_TEST_BOUNDARY:outside`, `COMPOSITION_BROWSER:system`,
  `COMPOSITION_CLI_READ:unavailable`. Exit 0.

So the private profile works on Windows. Inside the AppContainer, narrowing the
temporary profile is what fails. The contained cause of
`application_composition_capabilities_inside_boundary` is measured under
[[BPR-0833]] (step 2 of this task).

## `runtime_audit_contract`

The contained unit contract `tests/scripts/verify_runtime_surface_tests.py`
has two separate problems on Windows. Both are reported as new defects, not
fixed, because the card is integrator-owned.

- **FAIL: `test_application_command_names_a_root_only_when_one_is_owned`.**
  The test builds `Path("/private/root/data")` and compares
  `application_command(...)` with the literal `"/private/root/data"`. On
  Windows `str(Path(...))` is `\private\root\data`, so the lists differ. It is
  a test portability defect and independent of containment.
- **ERROR, three tests:**
  - `test_private_root_is_owner_private_uncreated_and_removed_after_a_pass`
  - `test_private_root_is_removed_when_inspection_raises`
  - `test_private_root_that_was_never_composed_fails_and_is_removed`

  All three stop in `scripts/verify_runtime_surface.py`
  `inspect_with_private_data_root` (line 290) at `Path.resolve()` of the new
  `mkdtemp` parent, with `PermissionError: [WinError 5] Access is denied:
  '...\Packages\bprautomatedtest\AC\Temp\bpr-runtime-surface-...'`. Inside
  the AppContainer, CPython's `realpath` cannot resolve a DOS path
  (`dos-volume-name`), and a `tempfile` root is also unusable there
  ([[BPR-0350]]).

## Rerun after BPR-0844 and BPR-0833's storage-root creation (task-20260917-win-creation-descriptor)

The base is `31fff50675a71bc2c5ce55857c72e57dc36933df`, plus:

- [[BPR-0844]] `59df97a151ef13ae206a1af3c6a72eb4073cc649`, the runtime surface
  script contract;
- [[BPR-0833]] `aa3c6ef18cc4046fab9d4960dc861cbffe9e91b2`, Windows storage-root
  creation with the private descriptor. BPR-0833's `main.cpp` seam change is
  **not** included: it is blocked, see the card.

The windows-dev tree was fully rebuilt. Log:
`build/logs/task-20260917-win-creation-descriptor/ctest-bpr0820-rerun.log`.

**13 of 15 pass.**

| Row | Result | First marker |
|---|---|---|
| `application_public_path` | Passed, 1.68 s | |
| `application_public_path_ghes_cli_unavailable` | Passed, 1.75 s | |
| `application_device_authorization_public_path` | Passed, 2.03 s | |
| `application_device_authorization_unavailable` | Passed, 5.88 s | |
| `application_composition_capabilities_inside_boundary` | **Failed**, contained | `AUTOMATED_PROFILE_PRIVATE_FAILED` |
| `application_composition_capabilities_product_run` | **Failed**, contained | `AUTOMATED_PROFILE_PRIVATE_FAILED` |
| `application_device_authorization_ghes_refusal` | Passed, 1.78 s | |
| `application_device_authorization_release_contract` | Passed, 5.81 s | |
| `application_anonymous_public_preview_unavailable` | Passed, 1.75 s | |
| `application_anonymous_public_preview_release_contract` | Passed, 1.68 s | |
| `application_anonymous_public_preview_enabled_probe` | Passed, 1.67 s | |
| `application_anonymous_public_preview_live_candidate_wiring` | Passed, 1.65 s | |
| `application_render` | Passed, 2.02 s | |
| `runtime_process_surface` | Passed, 1.68 s | `RUNTIME_SURFACE_OK` |
| `runtime_audit_contract` | **Passed**, 0.32 s (was failed) | `OK (skipped=1)` |

The two capability rows still fail contained. The cause is now measured
further and recorded on [[BPR-0833]]: Qt 6 creates the `QTemporaryDir` the seam
uses with a protected owner-only DACL that names no AppContainer SID, so inside
the container no child of it can be created. Removing that obstacle needs a
decision on which object owns the private directory's removal.
