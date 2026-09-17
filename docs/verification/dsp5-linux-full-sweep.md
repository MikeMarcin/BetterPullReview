# dsp-005 Linux full sweep (bpr-linux/int-13)

The whole Linux suite at the merged head, as a measurement. Nothing in this
task repairs anything: no product, test, CMake or script file is edited, no
card changes status or lane, no ID is minted. The only commit is this file.

## Binding

- **Run** `run-20260916-stations`, dispatch `dsp-005`, station dispatch
  `bpr-linux/int-13`, mandate r2, task `task-20260917-lnx-full-sweep`,
  reply to `bpr-main-mac`. Expiry 2026-09-18T20:00:00Z.
- **Station** `bpr-linux` (`stn-3f7fcc92`, `wsl2`), checkout
  `/home/mmarcin/bpr-linux`, branch `linux-lane`, authority `lane-commit`.
- **Head** `2b2e5549d81c4f9ba92cd93d542b5e6064b68c2e`, the envelope's
  `base_commit`, which is `main`. It contains [[BPR-0859]]'s fix `e8e08a57`
  and `bpr-win`'s [[BPR-0653]] merge. The working tree was clean at the
  build, at the sweep and at this commit, and `origin/linux-lane` was at the
  same commit before any work started.
- **Cards named for context only**: [[BPR-0859]], [[BPR-0854]], [[BPR-0855]],
  [[BPR-0856]], [[BPR-0857]], [[BPR-0858]], [[BPR-0860]], [[BPR-0861]],
  [[BPR-0653]]. None is edited here.
- **Host**: GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1), CMake 4.4.2, Ninja 1.13.2,
  CTest 4.4.2, Qt 6.10.3, Ubuntu 26.04 LTS, 20 logical CPUs.
- The Secret Service `login` collection read `(<false>,)` at the start, again
  immediately before the build, again immediately before the sweep, and again
  after it. All four readings are in the logs.

## Step 1: the stock `dev` build at the merged head

Stock `dev` preset, no flag overrides.

| Stage | Command | Result | Time |
|---|---|---|---|
| Configure | `cmake --preset dev --fresh` | Exit 0 | 7 s (19:44:59Z-19:45:06Z) |
| Build | `cmake --build --preset dev --clean-first` | Exit 0, 1253 of 1253 steps | 2 m 28 s (19:45:06Z-19:47:34Z) |

`--clean-first` removed 2284 built files first (`Cleaning... 2284 files.`), so
all 1253 steps are real compiles and links rather than a no-op over the
previous tree.

**Warning-clean, with counts.** The build log holds **0 lines matching
`warning:`, 0 matching `error:`, 0 matching `note:`**, and **0 occurrences of
the word "warning" in any case**. There is no diagnostic to quote.

**The five files [[BPR-0653]] adds are not compiled on this platform, and the
step count says so.** The build is 1253 of 1253 steps, which is the same
number `dsp-004` measured at `754d0538` before [[BPR-0653]] merged. All three
new translation units sit inside `if(WIN32)` guards:

| New file | Gate |
|---|---|
| `src/adapters/qt/src/windows_caption_buttons.cpp` | `if(WIN32)` in `src/adapters/qt/CMakeLists.txt` |
| `src/adapters/qt/include/bpr/qt/windows_caption_buttons.hpp` | included only by the above |
| `src/app/src/windows_title_row_host.cpp` | `if(WIN32)` in `src/app/CMakeLists.txt` |
| `src/app/src/windows_title_row_host.hpp` | included only by the above |
| `tests/end_to_end/windows_title_row_host_tests.cpp` | `if(WIN32)` in `tests/end_to_end/CMakeLists.txt` |

So GCC judged **none** of the five. What GCC did judge is [[BPR-0653]]'s
portable half, and it compiled all of it warning-clean:
`workbench_strip.cpp` (the new `empty_surface_at` rect guard,
`modality_control::set_abbreviated`, `abbreviated()` and the new `relabel()`),
`workbench_window.cpp` (`apply_strip_width`'s last yield and the 900 floor on
the non-Windows branch), `github_application.cpp`, and the two Qt test
translation units `workbench_window_tests.cpp` and `composed_github_tests.cpp`.
The Windows-only presenter and host remain unjudged by any compiler but MSVC.

## Step 2: the full Linux sweep

`ctest --preset dev -j10 --output-junit ...` on 20 logical CPUs, at
`2b2e5549`, on the build above.

| | |
|---|---|
| Parallelism | `-j10` |
| Start / end | 19:48:34Z / 19:56:34Z |
| Wall time | **479.73 s** (CTest's own total; the wrapper measured 480 s) |
| `ctest` exit | **0** |
| Total rows | **367** |
| Passed | **367** |
| Failed | **0** |
| Timed out | **0** |
| Not run | **0** |
| Disabled | **0** |
| Skipped | **0** |

`100% tests passed out of 367`. The JUnit document agrees independently:
`tests="367" failures="0" disabled="0" skipped="0"`. There is no `***Failed`,
`***Timeout`, `***Not Run` or `***Exception` line anywhere in the sweep log.

**No failure table, and no rerun-alone table, because there is no failure.**
The rerun-each-failure-alone step had nothing to run.

The row count rose from 366 to 367 against the previous sweep. The one added
row is `application_approximate_diff_public_path`, which [[BPR-0858]]'s fix
introduced; it passed in 3.72 s. No row was removed or renamed.

### Watch item: `application_uiforetw_selected_diff_contract` ([[BPR-0859]])

**Passed, 15.72 s**, marker `UIFORETW_SELECTED_DIFF_CONTRACT_OK`. This is the
Linux run the card was still open for: the "18 of 18" pass was at `754d0538`,
which predates the fix `e8e08a57`.

- **actions = 27, and the exact strip row held.** The row is
  `toolbarActions.size() == 27 + lead + trailing`, and off Windows `lead` and
  `trailing` are both zero, so the expectation on this platform is exactly 27.
  `UIFORETW_NAVIGATION_ROW_UNEXPECTED` **did not print**, and there is no
  `UIFORETW_NAVIGATION_ROW_ACTION` line, so there is no found-row to quote.
  The capture records the same fact positively, on all four profiles:
  `navigation_row_exact_order: true`.
- **It did not expire its budget.** It finished in 15.72 s against a 120 s row
  timeout and a 40 s per-profile run budget, so the waits (512 ticks for the
  modality, 256 for the reveal) were never near their bound. There is no
  `RUN_BUDGET_EXPIRED` in the output.
- **All four profiles ran, including the capture stage.** Linux is about 4 s
  per profile where macOS is about 17 s, which is why the row is 15.72 s here
  against macOS's ~68 s; it is not a stage that was skipped. The evidence
  directories were all written during this sweep (19:52Z) and each carries
  `capture-facts.json`, `full-window.png` and `intraline-comparison.png`.
- **The rasterised role-paint measurement matches the macOS table exactly**,
  which is the card's criterion 2 on a second platform:

| profile | removed role | added role | exact row |
|---|---|---|---|
| light | `#fff1b9b4` | `#ff9effb5` | true |
| dark | `#ff933030` | `#ff145e20` | true |
| high-contrast-light | `#ffff9999` | `#ff55ff55` | true |
| high-contrast-dark | `#ffff9999` | `#ff55ff55` | true |

  Each profile reports `intraline_edit_count: 5` and
  `fixture_diff_helper_invocations: 0`, so the shipped drawer-selection wire
  rather than the test shortcut, and all four navigation interactions
  (`previous_change`, `next_change`, `previous_file`, `next_file`) are
  recorded as exercised.

### Watch item: `qt_workbench_window_scale_100` / `_125` / `_200` ([[BPR-0653]])

**All three passed**: 23.51 s, 24.54 s and 29.97 s. Read closely, these are
the pins that were exercised on this platform:

- **The floor stays 900.** `expect(window.minimumWidth() == 900, "the strip's
  width floor stays 900 off Windows")` is the `#else` of the owner's 902
  Windows floor, and it held. `setMinimumSize(900, 360)` is the non-Windows
  branch in `workbench_window.cpp`.
- **The new portable pin held: labels never shorten.** `expect(!window.modality()
  ->abbreviated(), "where the strip fits at every width the modality labels
  never shorten")` is asserted at **every swept width**, inside `#if
  !defined(_WIN32)`. The second, all-platform pin — labels shorten only after
  the identity has yielded whole — also held. Both are new with [[BPR-0653]].
  This is the visible half of `apply_strip_width`'s last yield, which resets
  `set_abbreviated(false)` on entry and only sets it true when the toolbar
  still asks for more than the window after every earlier step. On Linux at or
  above the floor it never does.
- **The identity ladder keeps the owner and the repository at 1280 off
  Windows.** The width cases are `{1280, false, true, true}` here against
  `{1280, false, false, false}` on Windows, because the Windows strip spends
  about 180 more pixels on the menu control and caption buttons.
- **The latent bug the macOS repair exposed is exercised here too.**
  `empty_surface_at` now refuses a point outside the strip's own rect, and the
  zoom/restore row proves the point is inside the strip before double-clicking
  it. Whichever branch each scale took — the double-click, or the
  sub-floor-geometry branch that asserts `window.width() <
  window.minimumWidth()` and restores through the public window state — its
  assertion held, because both branches assert and the rows passed. Nothing
  here can silently skip.

### Watch item: `conditional_branch_identifiers`

**Passed, 6.72 s.** The exact marker, verbatim and whole:

```
CONDITIONAL_BRANCH_IDENTIFIERS_OK:700
```

**700 is the expected number.** It was 695 at `754d0538`; the marker is
`CONDITIONAL_BRANCH_IDENTIFIERS_OK:{len(sources)}`, where `sources` is every
file with a source suffix under `src` and `tests`, and [[BPR-0653]] added
exactly five such files. The gate counts them whether or not the platform
compiles them, which is the point of a gate that polices branches this host
does not build. This is the fourth independent measurement of the count and
the second platform.

### Watch item: `dependency_modernization_contract`

**Passed, 85.05 s**, 26 cases. **The observability gap is still there.** The
row's whole output is:

```
.....................s....
----------------------------------------------------------------------
Ran 26 tests in 84.984s

OK (skipped=1)
```

It reports `skipped=1` and **does not name the case or the reason**. A reader
of the ctest log cannot tell which of the 26 cases did not run, or why. Main
asked to be told if this was still the case, so that it can be carded: it is.

For main's triage, the skip is identifiable from the source but not from the
run. It is
`tests/scripts/dependency_modernization_tests.py::test_syntax_prefix_installs_one_library_per_configuration`,
which calls `self.skipTest("fewer than two installed KSyntaxHighlighting
configurations: not a multi-configuration tree")` when fewer than two
installed KSyntaxHighlighting export files are found. That is the expected
state on a single-configuration Ninja tree, so the skip is legitimate; only
its invisibility in the row's output is the gap.

## Step 3: delta against the previous Linux sweep, and the verdict

The previous Linux sweep was `dsp-004` / `bpr-linux/int-10`, task
`task-20260917-lnx-0851-sweep`, run at `bcc7148d` (the evidence commit on a
lane that started at `a1864fa2`). It was **355 of 366**, eleven failures, each
of which also failed alone. Every one of those eleven now passes:

| Row | Then (`bcc7148d`) | Card | Now (`2b2e5549`) |
|---|---|---|---|
| `conditional_branch_identifiers` | Failed, `..._FAILED:11` | [[BPR-0854]] | Passed, 6.72 s, `OK:700` |
| `contained_temporary_usage_contract` | Failed, 2.46 s alone | [[BPR-0854]] | Passed, 4.49 s |
| `dependency_modernization_contract` | Failed, 63.96 s alone | [[BPR-0854]] | Passed, 85.05 s |
| `msvc_secure_crt_contract` | Failed, 0.30 s alone | [[BPR-0854]] | Passed, 0.44 s |
| `application_epoch_host` | Failed, 0.23 s alone | [[BPR-0855]] | Passed, 0.29 s |
| `sqlite_anonymous_epoch_authority` | Failed alone | [[BPR-0857]] | Passed, 0.32 s |
| `application_large_review_benchmark` | Failed, `shutdown failed` | [[BPR-0856]] | Passed, 5.44 s |
| `application_unavailable_diff_public_path` | Failed, 3.54 s | [[BPR-0858]] | Passed, 3.84 s |
| `application_native_notification_attended_contract` | Failed, 0.42 s alone | [[BPR-0861]] | Passed, 4.69 s |
| `application_uiforetw_selected_diff_contract` | Failed, 1.41 s alone | [[BPR-0859]] | Passed, 15.72 s |
| `application_workbench_composition_contract` | Failed, 40.42 s alone | [[BPR-0860]] | Passed, 4.07 s |

`sqlite_epoch_catalog_storage_contract` ([[BPR-0857]]'s second row) also
passes, in 0.33 s.

**Delta: +12 passing rows, -11 failures.** 355 of 366 becomes 367 of 367: the
eleven reds are green and one new row was added green.

> **Verdict: this head is green on Linux.** Every registered row passes, with
> nothing skipped, disabled, timed out or not run.

Wall time fell from 583.49 s to 479.73 s at the same `-j10`, although the
suite gained a row — the earlier number included eleven rows burning their
budgets before failing.

## Observations recorded, not acted on

Nothing below was changed. They are handed to main to decide.

1. **`dependency_modernization_contract` does not name its skip.** The
   one-line repair would be to raise the runner's verbosity, or to print the
   skip reasons after the run, so `OK (skipped=1)` names the case. Described
   above; **not applied**.
2. **The sweep's wall time is one row.** `application_composition_contract`
   alone takes **466.72 s** of the 479.73 s wall. The sum of all row times is
   2481.27 s, so `-j10` is not the constraint and raising it would not bring
   the sweep below about 467 s. If the sweep's wall time is ever a target,
   that row is the whole of it. Not a defect; an observation.
3. **[[BPR-0653]]'s Windows-only half has no non-MSVC reader.** Recorded under
   step 1: all five new files are `if(WIN32)`-gated, so this station's
   warning-clean result says nothing about them. The envelope expected GCC to
   have new code to judge; it had the portable strip only.

## Logs

`/home/mmarcin/bpr-station-logs/task-20260917-lnx-full-sweep/`:
`step0.summary`, `run-step1.sh`, `configure-dev.log`,
`build-dev-clean-first.log`, `step1.summary`, `step2-show-only.log`,
`run-step2.sh`, `step2.summary`, `step2-sweep.log`, `step2-junit.xml`,
`step2-rows.md`, `tmp-before.txt`, `tmp-after.txt`. None is committed; the
results are transcribed here.

## Every row, in ctest order

All 367 rows, with the time CTest recorded for each.

| # | Row | Result | Time (s) |
|---:|---|---|---:|
| 1 | `designated_live_mutation_gate` | Passed | 0.01 |
| 2 | `designated_live_mutation_runner_digest_mismatch` | Passed | 0.06 |
| 3 | `core_dependency_boundary` | Passed | 0.51 |
| 4 | `core_boundary_contract` | Passed | 0.20 |
| 5 | `architecture_boundaries` | Passed | 13.92 |
| 6 | `architecture_boundary_contract` | Passed | 187.68 |
| 7 | `git_protocol_boundary` | Passed | 0.58 |
| 8 | `conditional_branch_identifiers` | Passed | 6.72 |
| 9 | `conditional_branch_identifier_contract` | Passed | 0.09 |
| 10 | `rfc0017_test_inventory` | Passed | 0.09 |
| 11 | `rfc0017_release_absence_negative_control` | Passed | 1.84 |
| 12 | `rfc0016_test_inventory` | Passed | 0.10 |
| 13 | `documentation_structure` | Passed | 1.34 |
| 14 | `documentation_issue_graph_contract` | Passed | 0.47 |
| 15 | `issue_id_allocation_contract` | Passed | 0.59 |
| 16 | `github_recorded_fixture_contract` | Passed | 0.09 |
| 17 | `large_review_fixture_contract` | Passed | 1.33 |
| 18 | `github_conversation_fixture_contract` | Passed | 0.07 |
| 19 | `attended_review_verdict_contract` | Passed | 1.66 |
| 20 | `operator_response_validation_contract` | Passed | 1.96 |
| 21 | `project_source_dump_contract` | Passed | 0.19 |
| 22 | `runtime_audit_contract` | Passed | 0.10 |
| 23 | `binary_dependency_audit_contract` | Passed | 0.11 |
| 24 | `composed_host_verifier_contract` | Passed | 0.51 |
| 25 | `contained_temporary_usage_contract` | Passed | 4.49 |
| 26 | `coding_standard_format_gate_contract` | Passed | 0.28 |
| 27 | `platform_configuration_contract` | Passed | 0.07 |
| 28 | `application_icon_assets_contract` | Passed | 0.09 |
| 29 | `dif_assets_contract` | Passed | 198.42 |
| 30 | `dependency_modernization_contract` | Passed | 85.05 |
| 31 | `github_contract_fixture_security` | Passed | 2.88 |
| 32 | `rfc0016_device_capture_security` | Passed | 5.90 |
| 33 | `rfc0016_relocated_operator_contract` | Passed | 0.68 |
| 34 | `rfc0017_public_preview_decision_contract` | Passed | 0.51 |
| 35 | `rfc0017_test_inventory_contract` | Passed | 0.12 |
| 36 | `rfc0016_device_live_runner_security` | Passed | 0.19 |
| 37 | `rfc0016_production_identity_security` | Passed | 1.37 |
| 38 | `rfc0016_production_course_security` | Passed | 0.17 |
| 39 | `msvc_secure_crt_contract` | Passed | 0.44 |
| 40 | `type_read_binding_contract` | Passed | 0.25 |
| 41 | `rfc0016_platform_course_launch_contract` | Passed | 0.08 |
| 42 | `rfc0017_platform_course_launch_contract` | Passed | 0.07 |
| 43 | `rfc0017_sanitizer_course_contract` | Passed | 0.08 |
| 44 | `test_network_isolation_structure_contract` | Passed | 1.09 |
| 45 | `test_network_isolation_manifest` | Passed | 1.98 |
| 46 | `test_network_isolation_external` | Passed | 0.04 |
| 47 | `test_network_isolation_child` | Passed | 0.10 |
| 48 | `test_network_isolation_identity` | Passed | 0.05 |
| 49 | `test_network_isolation_loopback` | Passed | 0.05 |
| 50 | `test_network_isolation_application` | Passed | 0.10 |
| 51 | `live_mutation_isolation` | Passed | 55.89 |
| 52 | `live_mutation_isolation_contract` | Passed | 0.12 |
| 53 | `core_foundation` | Passed | 0.01 |
| 54 | `core_github_policy` | Passed | 0.01 |
| 55 | `core_epoch_policy` | Passed | 0.01 |
| 56 | `core_diff_model` | Passed | 0.66 |
| 57 | `core_diff_compute` | Passed | 2.59 |
| 58 | `core_diff_semantics` | Passed | 0.41 |
| 59 | `core_diff_transition` | Passed | 0.00 |
| 60 | `core_workbench_interaction` | Passed | 0.01 |
| 61 | `core_appearance_policy` | Passed | 0.00 |
| 62 | `core_appearance_contrast` | Passed | 0.02 |
| 63 | `core_theme_exchange` | Passed | 0.01 |
| 64 | `core_review_ledger` | Passed | 0.01 |
| 65 | `core_review_round_trip` | Passed | 0.00 |
| 66 | `core_defect_workflow` | Passed | 0.00 |
| 67 | `core_review_queue` | Passed | 0.00 |
| 68 | `core_review_queue_attention` | Passed | 0.00 |
| 69 | `core_agent_profile` | Passed | 0.00 |
| 70 | `core_agent_session` | Passed | 0.01 |
| 71 | `core_agent_chat_scope` | Passed | 0.00 |
| 72 | `core_agent_context_supplement` | Passed | 0.01 |
| 73 | `core_agent_finding` | Passed | 0.01 |
| 74 | `core_agent_finding_promotion` | Passed | 0.00 |
| 75 | `core_device_authorization` | Passed | 0.00 |
| 76 | `core_anonymous_public_read` | Passed | 0.00 |
| 77 | `core_review_submission` | Passed | 0.01 |
| 78 | `core_setup_destination` | Passed | 0.00 |
| 79 | `core_profile_window` | Passed | 0.00 |
| 80 | `core_github_window_read` | Passed | 0.00 |
| 81 | `core_revisioned_review_draft` | Passed | 0.00 |
| 82 | `core_window_review_restoration` | Passed | 0.00 |
| 83 | `application_host` | Passed | 0.00 |
| 84 | `application_review_mutation_application_host` | Passed | 0.01 |
| 85 | `application_designated_review_authority_bootstrap` | Passed | 187.27 |
| 86 | `application_github_host` | Passed | 0.03 |
| 87 | `application_anonymous_read_host` | Passed | 0.01 |
| 88 | `application_anonymous_diff_authority` | Passed | 0.01 |
| 89 | `application_github_contract` | Passed | 0.01 |
| 90 | `application_github_capability_cache` | Passed | 0.00 |
| 91 | `application_github_credential_vault` | Passed | 0.00 |
| 92 | `application_git_protocol_v2` | Passed | 0.01 |
| 93 | `application_epoch_catalog_contract` | Passed | 0.00 |
| 94 | `application_epoch_contract` | Passed | 0.01 |
| 95 | `application_epoch_host` | Passed | 0.29 |
| 96 | `application_diff_host` | Passed | 0.03 |
| 97 | `application_appearance_host` | Passed | 0.02 |
| 98 | `application_workbench_host` | Passed | 0.01 |
| 99 | `application_review_ledger_host` | Passed | 0.00 |
| 100 | `application_review_mutation` | Passed | 0.00 |
| 101 | `application_review_synchronization` | Passed | 0.00 |
| 102 | `application_defect_correlation` | Passed | 0.00 |
| 103 | `application_review_queue_refresh` | Passed | 0.00 |
| 104 | `application_review_queue_enrollment` | Passed | 16.57 |
| 105 | `application_review_queue_notification` | Passed | 0.01 |
| 106 | `application_agent_workspace` | Passed | 0.00 |
| 107 | `application_agent_launch` | Passed | 0.01 |
| 108 | `application_agent_protocol_loop` | Passed | 0.01 |
| 109 | `application_agent_finding_protocol` | Passed | 1.39 |
| 110 | `application_agent_comment_workshop_host` | Passed | 0.00 |
| 111 | `application_agent_invocation_host` | Passed | 0.01 |
| 112 | `application_notification_delivery` | Passed | 0.00 |
| 113 | `application_device_authorization_host` | Passed | 0.01 |
| 114 | `application_device_authorization_refresh` | Passed | 0.00 |
| 115 | `application_workbench_snapshot_observation` | Passed | 0.00 |
| 116 | `application_pull_request_overview` | Passed | 0.00 |
| 117 | `application_mcp_context_helper` | Passed | 0.02 |
| 118 | `application_review_submission_contract` | Passed | 0.00 |
| 119 | `application_review_submission_application_host` | Passed | 0.00 |
| 120 | `application_setup_destination` | Passed | 0.00 |
| 121 | `application_profile_registry` | Passed | 0.00 |
| 122 | `application_profile_storage_authority` | Passed | 0.00 |
| 123 | `application_profile_startup_admission` | Passed | 0.00 |
| 124 | `application_profile_management_host` | Passed | 0.01 |
| 125 | `application_github_window_read_host` | Passed | 0.00 |
| 126 | `application_appearance_profile_consumers` | Passed | 0.01 |
| 127 | `application_workbench_preferences_consumers` | Passed | 0.00 |
| 128 | `application_shared_draft_application_host` | Passed | 0.01 |
| 129 | `application_window_review_restoration` | Passed | 0.00 |
| 130 | `product_contract` | Passed | 0.01 |
| 131 | `sqlite_epoch_catalog_storage_contract` | Passed | 0.33 |
| 132 | `sqlite_epoch_catalog_behavior_contract` | Passed | 171.35 |
| 133 | `sqlite_review_mutation_recovery_storage` | Passed | 0.03 |
| 134 | `sqlite_review_ledger_storage` | Passed | 1.39 |
| 135 | `sqlite_queue_enrollment_storage` | Passed | 0.76 |
| 136 | `sqlite_workbench_settings_storage` | Passed | 0.61 |
| 137 | `sqlite_appearance_settings_storage` | Passed | 0.46 |
| 138 | `sqlite_storage_root_contract` | Passed | 0.13 |
| 139 | `contained_catalog_probe` | Passed | 0.00 |
| 140 | `sqlite_anonymous_epoch_authority` | Passed | 0.32 |
| 141 | `sqlite_device_binding_storage` | Passed | 0.57 |
| 142 | `sqlite_review_submission_recovery_storage` | Passed | 0.98 |
| 143 | `sqlite_profile_registry_storage` | Passed | 0.47 |
| 144 | `sqlite_profile_storage_root` | Passed | 0.00 |
| 145 | `sqlite_profile_runtime_lease` | Passed | 0.01 |
| 146 | `sqlite_profile_activation` | Passed | 4.13 |
| 147 | `sqlite_window_review_restoration_storage` | Passed | 0.24 |
| 148 | `qt_foundation_window` | Passed | 0.02 |
| 149 | `qt_system_tray_notification_sink` | Passed | 0.01 |
| 150 | `qt_composed_github_scale_100` | Passed | 4.76 |
| 151 | `qt_composed_github_scale_125` | Passed | 5.22 |
| 152 | `qt_composed_github_scale_200` | Passed | 6.98 |
| 153 | `qt_device_authorization_scale_100` | Passed | 4.74 |
| 154 | `qt_device_authorization_scale_125` | Passed | 4.74 |
| 155 | `qt_device_authorization_scale_200` | Passed | 4.75 |
| 156 | `qt_anonymous_public_preview_scale_100` | Passed | 0.62 |
| 157 | `qt_anonymous_public_preview_scale_125` | Passed | 0.57 |
| 158 | `qt_anonymous_public_preview_scale_200` | Passed | 0.67 |
| 159 | `qt_github_conversation_panel` | Passed | 0.06 |
| 160 | `qt_conversation_cards_view` | Passed | 1.79 |
| 161 | `qt_files_tree` | Passed | 0.03 |
| 162 | `qt_repositories_drawer` | Passed | 0.03 |
| 163 | `qt_github_thread_map_scale_100` | Passed | 0.24 |
| 164 | `qt_github_thread_map_scale_125` | Passed | 0.27 |
| 165 | `qt_github_thread_map_scale_200` | Passed | 0.39 |
| 166 | `qt_github_conversation_model_property` | Passed | 29.39 |
| 167 | `qt_sensitive_text_input` | Passed | 0.02 |
| 168 | `qt_epoch_comparison_scale_100` | Passed | 0.26 |
| 169 | `qt_epoch_comparison_scale_125` | Passed | 0.28 |
| 170 | `qt_epoch_comparison_scale_200` | Passed | 0.47 |
| 171 | `qt_pull_request_overview_page` | Passed | 0.04 |
| 172 | `qt_comparison_scroll_controller_scale_100` | Passed | 0.03 |
| 173 | `qt_comparison_scroll_controller_scale_125` | Passed | 0.04 |
| 174 | `qt_comparison_scroll_controller_scale_200` | Passed | 0.04 |
| 175 | `qt_diff_link_map_scale_100` | Passed | 0.19 |
| 176 | `qt_diff_link_map_scale_125` | Passed | 0.23 |
| 177 | `qt_diff_link_map_scale_200` | Passed | 0.33 |
| 178 | `qt_appearance_settings_panel` | Passed | 0.30 |
| 179 | `qt_code_sample_painter` | Passed | 0.03 |
| 180 | `qt_theme_preview_strip` | Passed | 0.11 |
| 181 | `qt_appearance_role_display` | Passed | 0.00 |
| 182 | `qt_colour_role_list` | Passed | 0.11 |
| 183 | `qt_type_preview` | Passed | 0.05 |
| 184 | `qt_settings_window` | Passed | 0.59 |
| 185 | `qt_settings_window_scale_125` | Passed | 0.52 |
| 186 | `qt_settings_window_scale_150` | Passed | 0.58 |
| 187 | `qt_settings_window_scale_200` | Passed | 0.67 |
| 188 | `qt_agent_launch_panel` | Passed | 0.03 |
| 189 | `qt_side_chat_panel` | Passed | 0.02 |
| 190 | `qt_agent_launch_canary` | Passed | 60.61 |
| 191 | `agent_parent_death` | Passed | 20.51 |
| 192 | `qt_syntax_pane_composition` | Passed | 0.14 |
| 193 | `qt_diff_workbench_scale_100` | Passed | 3.53 |
| 194 | `qt_diff_workbench_scale_125` | Passed | 3.89 |
| 195 | `qt_diff_workbench_scale_200` | Passed | 4.74 |
| 196 | `qt_workbench_window_scale_100` | Passed | 23.51 |
| 197 | `qt_workbench_window_scale_125` | Passed | 24.54 |
| 198 | `qt_workbench_window_scale_200` | Passed | 29.97 |
| 199 | `qt_appearance_profile` | Passed | 0.37 |
| 200 | `qt_dif_runtime` | Passed | 1.32 |
| 201 | `qt_motion_observer` | Passed | 0.02 |
| 202 | `qt_diff_axis_scale_100` | Passed | 0.04 |
| 203 | `qt_diff_axis_scale_125` | Passed | 0.06 |
| 204 | `qt_diff_axis_scale_200` | Passed | 0.07 |
| 205 | `qt_review_queue` | Passed | 0.01 |
| 206 | `qt_review_draft_editor` | Passed | 0.05 |
| 207 | `qt_finding_queue_view` | Passed | 0.02 |
| 208 | `qt_defect_card` | Passed | 0.02 |
| 209 | `qt_review_landing_model` | Passed | 0.27 |
| 210 | `qt_home_list_scale_100` | Passed | 14.74 |
| 211 | `qt_home_list_scale_125` | Passed | 14.80 |
| 212 | `qt_home_list_scale_200` | Passed | 14.93 |
| 213 | `qt_review_submission_editor_scale_100` | Passed | 0.03 |
| 214 | `qt_review_submission_editor_scale_125` | Passed | 0.04 |
| 215 | `qt_review_submission_editor_scale_200` | Passed | 0.05 |
| 216 | `qt_settings_effect_boundary` | Passed | 0.14 |
| 217 | `qt_profile_management_window` | Passed | 0.19 |
| 218 | `qt_surface_capture_course_scale_100` | Passed | 2.33 |
| 219 | `qt_surface_capture_course_scale_125` | Passed | 2.87 |
| 220 | `qt_surface_capture_course_scale_150` | Passed | 3.06 |
| 221 | `qt_surface_capture_course_scale_200` | Passed | 3.34 |
| 222 | `native_https_transport_contract` | Passed | 0.01 |
| 223 | `native_git_smart_transport_contract` | Passed | 0.01 |
| 224 | `native_https_device_authorization_contract` | Passed | 0.01 |
| 225 | `native_https_anonymous_read_contract` | Passed | 0.43 |
| 226 | `native_https_transport_fixture` | Passed | 10.50 |
| 227 | `rfc0017_credential_free_native_https` | Passed | 0.37 |
| 228 | `native_https_anonymous_read_fixture` | Passed | 0.63 |
| 229 | `native_git_smart_transport_fixture` | Passed | 22.69 |
| 230 | `native_https_device_authorization_fixture` | Passed | 11.72 |
| 231 | `github_device_authorization_refresh_contract` | Passed | 4.23 |
| 232 | `gh_cli_transport` | Passed | 0.04 |
| 233 | `github_cli_process_runner` | Passed | 0.98 |
| 234 | `github_read_contract` | Passed | 0.19 |
| 235 | `github_anonymous_read_contract` | Passed | 0.01 |
| 236 | `github_device_authorization_contract` | Passed | 0.01 |
| 237 | `github_mutation_contract` | Passed | 0.01 |
| 238 | `live_review_mutation_provider` | Passed | 0.02 |
| 239 | `live_review_mutation_restart_recovery` | Passed | 0.07 |
| 240 | `live_review_submission_provider` | Passed | 0.12 |
| 241 | `live_review_submission_restart_recovery` | Passed | 0.22 |
| 242 | `github_paged_read` | Passed | 3.11 |
| 243 | `github_provider` | Passed | 17.76 |
| 244 | `github_host_profile` | Passed | 0.01 |
| 245 | `github_capability_probe` | Passed | 8.27 |
| 246 | `linux_secret_service_contract` | Passed | 0.01 |
| 247 | `linux_secret_service_native_setup` | Passed | 0.02 |
| 248 | `linux_secret_service_native` | Passed | 0.65 |
| 249 | `linux_secret_service_native_cleanup` | Passed | 0.02 |
| 250 | `epoch_history_fixture_contract` | Passed | 0.10 |
| 251 | `profile_window_restoration_fixture_contract` | Passed | 0.08 |
| 252 | `git_process_contract` | Passed | 6.42 |
| 253 | `git_cache_contract` | Passed | 6.53 |
| 254 | `git_sparse_cache_contract` | Passed | 2.01 |
| 255 | `epoch_maintenance_integration` | Passed | 1.39 |
| 256 | `git_haves_trimmed_pack_update` | Passed | 0.57 |
| 257 | `git_haves_trimmed_pack_missing-object` | Passed | 0.68 |
| 258 | `git_haves_trimmed_pack_unconnected` | Passed | 0.27 |
| 259 | `epoch_kill_recovery` | Passed | 3.01 |
| 260 | `epoch_transaction_kill_recovery` | Passed | 4.76 |
| 261 | `git_anonymous_public_read` | Passed | 0.64 |
| 262 | `agent_workspace_ownership` | Passed | 0.99 |
| 263 | `syntax_classification_contract` | Passed | 0.07 |
| 264 | `application_public_path` | Passed | 0.42 |
| 265 | `application_public_path_ghes_cli_unavailable` | Passed | 0.45 |
| 266 | `application_device_authorization_public_path` | Passed | 0.54 |
| 267 | `application_device_authorization_unavailable` | Passed | 0.57 |
| 268 | `application_device_authorization_registered_course` | Passed | 5.52 |
| 269 | `application_device_authorization_refresh_course` | Passed | 7.93 |
| 270 | `application_device_authorization_orphan_course` | Passed | 1.17 |
| 271 | `application_device_authorization_browser_handoff` | Passed | 5.44 |
| 272 | `application_device_authorization_browser_absent` | Passed | 5.42 |
| 273 | `application_composition_capabilities_inside_boundary` | Passed | 0.41 |
| 274 | `application_composition_capabilities_product_run` | Passed | 0.45 |
| 275 | `application_device_authorization_panel_course` | Passed | 5.45 |
| 276 | `application_device_authorization_panel_absent` | Passed | 5.50 |
| 277 | `application_device_authorization_ghes_refusal` | Passed | 0.43 |
| 278 | `application_device_authorization_release_contract` | Passed | 0.53 |
| 279 | `application_anonymous_public_preview_unavailable` | Passed | 0.45 |
| 280 | `application_anonymous_public_preview_release_contract` | Passed | 0.45 |
| 281 | `application_anonymous_public_preview_enabled_probe` | Passed | 0.46 |
| 282 | `application_anonymous_public_preview_live_candidate_wiring` | Passed | 0.41 |
| 283 | `application_worker_executor` | Passed | 0.01 |
| 284 | `application_public_path_wait` | Passed | 0.00 |
| 285 | `application_device_authorization_browser_address` | Passed | 0.01 |
| 286 | `application_composition_contract` | Passed | 466.72 |
| 287 | `application_large_review_benchmark` | Passed | 5.44 |
| 288 | `application_unavailable_diff_public_path` | Passed | 3.84 |
| 289 | `application_approximate_diff_public_path` | Passed | 3.72 |
| 290 | `large_review_replay_contract` | Passed | 13.30 |
| 291 | `application_agent_crash_recovery` | Passed | 20.21 |
| 292 | `application_agent_materialization_crash_recovery` | Passed | 9.85 |
| 293 | `application_agent_chat_composition_contract` | Passed | 2.03 |
| 294 | `application_agent_chat_stale_contract` | Passed | 1.91 |
| 295 | `application_agent_chat_continued_contract` | Passed | 2.71 |
| 296 | `application_agent_chat_anchored_continuation_contract` | Passed | 2.63 |
| 297 | `application_agent_chat_construction_cut_contract` | Passed | 1.85 |
| 298 | `application_agent_finding_composition_contract` | Passed | 2.04 |
| 299 | `application_agent_finding_full_agent_contract` | Passed | 2.11 |
| 300 | `application_agent_finding_finding_host_construction_cut_contract` | Passed | 2.03 |
| 301 | `application_agent_finding_workshop_press_contract` | Passed | 2.33 |
| 302 | `application_agent_finding_workshop_construction_cut_contract` | Passed | 2.04 |
| 303 | `application_agent_finding_mcp_construction_cut_contract` | Passed | 1.97 |
| 304 | `application_agent_finding_chat_panel_construction_cut_contract` | Passed | 2.04 |
| 305 | `application_agent_finding_queue_construction_cut_contract` | Passed | 2.08 |
| 306 | `application_agent_finding_dif_context_contract` | Passed | 8.07 |
| 307 | `application_agent_invocation_composition_contract` | Passed | 11.09 |
| 308 | `application_agent_invocation_construction_cut_contract` | Passed | 0.93 |
| 309 | `application_agent_invocation_launcher_construction_cut_contract` | Passed | 0.94 |
| 310 | `application_agent_invocation_gesture_cut_contract` | Passed | 1.70 |
| 311 | `application_agent_invocation_cancellation_contract` | Passed | 8.87 |
| 312 | `application_agent_invocation_staleness_contract` | Passed | 8.61 |
| 313 | `application_agent_invocation_cleanup_retry_contract` | Passed | 8.71 |
| 314 | `application_agent_invocation_workspace_diagnostic_contract` | Passed | 1.93 |
| 315 | `application_queue_composition_contract` | Passed | 4.68 |
| 316 | `application_native_notification_attended_contract` | Passed | 4.69 |
| 317 | `application_poller_liveness` | Passed | 0.28 |
| 318 | `application_icon` | Passed | 0.03 |
| 319 | `application_native_appearance_probe_contract` | Passed | 4.11 |
| 320 | `application_github_public_path_github_com` | Passed | 1.81 |
| 321 | `application_github_public_path_uiforetw_74` | Passed | 1.85 |
| 322 | `application_uiforetw_selected_diff_contract` | Passed | 15.72 |
| 323 | `application_diff_workbench_public_path` | Passed | 3.78 |
| 324 | `application_workbench_composition_contract` | Passed | 4.07 |
| 325 | `application_review_ledger_composition_contract` | Passed | 5.61 |
| 326 | `application_review_round_trip_composition_contract` | Passed | 2.25 |
| 327 | `application_defect_composition_contract` | Passed | 8.33 |
| 328 | `application_overview_since_review_contract` | Passed | 10.07 |
| 329 | `application_review_behaviour_contract` | Passed | 11.01 |
| 330 | `application_saved_server_contract` | Passed | 10.47 |
| 331 | `application_threaded_reply_contract` | Passed | 3.79 |
| 332 | `application_appearance_theme_customization_contract` | Passed | 7.39 |
| 333 | `application_fixture_verdict_contract` | Passed | 3.83 |
| 334 | `application_attended_launch_contract` | Passed | 20.15 |
| 335 | `application_diff_workbench_shutdown_public_path` | Passed | 1.64 |
| 336 | `application_automated_run_budget` | Passed | 5.40 |
| 337 | `application_github_public_path_ghes_3_18` | Passed | 1.81 |
| 338 | `application_anonymous_public_preview_ghes_refusal` | Passed | 0.44 |
| 339 | `application_anonymous_public_preview_public_path` | Passed | 2.65 |
| 340 | `application_epoch_portable_occurrence_two_catalogues` | Passed | 18.10 |
| 341 | `application_epoch_persistence_github-com` | Passed | 6.05 |
| 342 | `application_epoch_persistence_ghes-3_18` | Passed | 5.79 |
| 343 | `application_epoch_fixture_noncanonical_root_cause` | Passed | 3.79 |
| 344 | `application_epoch_maintenance_public_path` | Passed | 8.31 |
| 345 | `application_appearance_gesture_contract` | Passed | 8.83 |
| 346 | `application_appearance_store_fallback_contract` | Passed | 4.90 |
| 347 | `binary_dependency_surface` | Passed | 0.05 |
| 348 | `runtime_process_surface` | Passed | 0.56 |
| 349 | `composed_application_hosts` | Passed | 3.99 |
| 350 | `application_render` | Passed | 0.48 |
| 351 | `application_device_binding_restart` | Passed | 29.58 |
| 352 | `application_device_binding_launch_renewal` | Passed | 20.08 |
| 353 | `application_device_binding_relaunch` | Passed | 8.42 |
| 354 | `application_logging` | Passed | 5.69 |
| 355 | `application_review_submission_composition_public_path` | Passed | 3.29 |
| 356 | `application_setup_destination_public_path` | Passed | 1.40 |
| 357 | `application_profile_startup_public_path` | Passed | 2.42 |
| 358 | `application_profile_management_public_path` | Passed | 3.64 |
| 359 | `application_profile_multi_window_public_path` | Passed | 14.20 |
| 360 | `application_profile_activation_public_path` | Passed | 3.58 |
| 361 | `application_profile_owner_admission_public_path` | Passed | 1.91 |
| 362 | `application_profile_window_restart_public_path` | Passed | 6.48 |
| 363 | `application_profile_window_layout_public_path` | Passed | 27.18 |
| 364 | `application_profile_window_review_restoration_public_path` | Passed | 123.35 |
| 365 | `application_epoch_ordinary_update_recreated` | Passed | 7.26 |
| 366 | `quill_logging` | Passed | 0.07 |
| 367 | `quill_logging_backpressure` | Passed | 0.10 |
