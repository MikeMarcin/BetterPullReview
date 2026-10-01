# Run budget survey, 2026-09-25

BPR-0905 surveys the current Mac registration at `97a7564b` plus its four
bound corrections against existing station-run logs. This is an inventory,
not new platform qualification. The [authoring rule](build-and-test.md#run-budgets-and-row-timeouts)
explains sizing and expiry diagnostics.

## Method and coverage

- Enumerated the local Debug graph with `ctest --test-dir build/dev --show-only=json-v1`.
- Matched CTest `Test #…: NAME … Passed SECONDS sec` lines in the 1,029 existing
  `build/r2-logs/*.log` files, retaining the largest passing duration per current name.
- Compared the registered row timeout with that largest duration; less than 3x
  is a survey flag, not automatic evidence that the limit should increase.
- 449 current rows; 299 have an explicit `TIMEOUT`; 244 of those have a matching
  historical passing duration, 55 have none. The other 150 have no explicit row
  timeout in this inventory. Platform-specific rows absent from the Mac graph
  are outside this inventory; a native Windows/Linux inventory is still owed.
- The historical logs span candidates, host loads and station dispatches. Their
  source locations below preserve that context. They do not establish current
  worst-case runtime, and the lack of a timeout line does not prove no expiry.
- A CTest row duration includes setup and every child. It cannot be divided by
  the number of phases to prove the slowest phase, so it is not compared against
  `BUDGET_MS`. Earlier passing application runs lacked an elapsed marker; those
  run margins remain unknown. BPR-0905 now emits that marker on success.

## Later runs

The [2026-10-01 full-suite record](../verification/BPR-0905-full-suite-2026-10-01.md)
repeats the margin table for the current Mac graph from one contended run,
and holds the Windows and Linux tables when those stations return them.

## Additional rows flagged for review

These 21 rows have unchanged bounds. Investigate the row, platform, candidate
and load before changing one; a slow or stalled host is still allowed to fail.

| Current row | Timeout (s) | Largest historical pass (s) | Margin | Evidence in `build/r2-logs/` |
|---|---:|---:|---:|---|
| `qt_agent_launch_canary` | 120 | 83.96 | 1.43x | `dsp5-mac-full-sweep.log:391` |
| `epoch_kill_recovery` | 180 | 120.48 | 1.49x | `dsp7-0901-full-sweep.log:575` |
| `epoch_transaction_kill_recovery` | 300 | 184.03 | 1.63x | `dsp7-0901-full-sweep.log:908` |
| `git_process_contract` | 180 | 105.05 | 1.71x | `dsp7-0901-full-sweep.log:5344` |
| `application_large_review_history` | 600 | 322.18 | 1.86x | `dsp6-0890-sweep.log:16935` |
| `epoch_maintenance_integration` | 90 | 45.02 | 2.00x | `dsp7-0901-full-sweep.log:2860` |
| `application_device_binding_restart` | 60 | 30.01 | 2.00x | `dsp3-int7-ctest.log:13` |
| `application_profile_multi_window_public_path` | 180 | 84.39 | 2.13x | `dsp7-0901-full-sweep.log:1855` |
| `git_sparse_cache_contract` | 180 | 84.13 | 2.14x | `dsp7-0901-full-sweep.log:6262` |
| `application_device_authorization_registered_course` | 15 | 6.6 | 2.27x | `dsp2-0644c-focused.log:21` |
| `git_cache_contract` | 600 | 258.88 | 2.32x | `dsp7-0901-full-sweep.log:5536` |
| `application_device_authorization_panel_course` | 15 | 6.25 | 2.40x | `dsp2-0815-bisect-061dfcdf.log:3` |
| `application_device_authorization_browser_absent` | 15 | 5.96 | 2.52x | `dsp6-0890-rerun.log:390` |
| `application_device_authorization_browser_handoff` | 15 | 5.91 | 2.54x | `dsp2-int5-ctest.log:13` |
| `github_provider` | 30 | 11.48 | 2.61x | `dsp7-0901-full-sweep.log:2084` |
| `large_review_replay_contract` | 30 | 11.49 | 2.61x | `dsp7-0901-full-sweep.log:1531` |
| `application_device_authorization_panel_absent` | 15 | 5.59 | 2.68x | `dsp2-0815-bisect-061dfcdf.log:5` |
| `application_epoch_portable_occurrence_two_catalogues` | 700 | 244.03 | 2.87x | `dsp7-0901-full-sweep.log:1498` |
| `application_device_authorization_refresh_course` | 25 | 8.59 | 2.91x | `dsp2-0644-baseline-existing.log:3` |
| `live_mutation_isolation` | 300 | 102.6 | 2.92x | `dsp7-0901-full-sweep.log:706` |
| `application_profile_window_review_restoration_public_path` | 1800 | 606.73 | 2.97x | `dsp7-0901-full-sweep.log:16952` |

## The four corrected rows

| Row | Old → new bound | Derivation |
|---|---|---|
| `application_saved_server_contract` | 40 → 120 s per phase; CTest 240 → 660 s | Existing epoch class: 120 / recorded 23.7 s = 5.1x; four runs plus 4 s reserve require at least 645.34 s under the 75% rule. |
| `application_defect_composition_contract` | 40 → 120 s per phase; CTest 240 → 660 s | Same epoch class: 120 / recorded 18.0 s = 6.7x; the current driver has four runs (seed/reopen for both servers), not the original report’s three. |
| `application_review_ledger_composition_contract` | 40 → 120 s per phase; CTest 180 → 500 s | Same epoch class: 120 / recorded 18.4 s = 6.5x; two epoch phases plus one unavailable/single phase (unchanged 40 s) are conservatively counted as three maximum run allowances plus 4 s reserve, requiring at least 485.34 s. |
| `sqlite_epoch_catalog_behavior_contract` | CTest 180 → 420 s | Recorded Linux isolated 122.65 s × 3 = 367.95 s, rounded up to a minute. Two recorded 180.03 s timeouts are censored observations, not completion times. New Linux contention proof remains pending. |

The epoch class itself retains its earlier derivation in
`tests/end_to_end/CMakeLists.txt`: 25 s quiet, 34.7 s observed loaded, and
68.6 s on a recorded failing host; 120 s is 4.8x, 3.5x and 1.75x respectively.
No epoch work, retry policy, fixture content or other budget changed.

## Per-run budget coverage

The source scan finds 18 CMake drivers that accept `BPR_RUN_BUDGET_MS`,
including the new success/expiry contract.
Only the three misclassified drivers above move classes. Drivers that request
`--epoch-fixture-phase single` keep their render budget. Successful timing is
now emitted by the application on every automated terminal. The three corrected
drivers forward that marker into successful CTest logs through
`bpr_automated_run_result.cmake`; existing drivers still own their output capture.
A captured child’s omitted success output is a measurement gap, not a zero.

The reusable result helper gives the three corrected drivers an actionable
`RUN_BUDGET_EXPIRED` headline, or `PROCESS_TIMEOUT` when CMake killed a child
before it produced its own expiry record. Direct application rows also print
the expiry before `PUBLIC_PATH_FAILED`. Existing epoch/appearance wrappers
already retain expiry in their diagnostic preamble; wrappers not yet using the
helper can be migrated when touched.

## Unmeasured row-timeout margins

The 55 bounded rows with no matching historical pass are listed below. These
are unknown margins, not failures or an invitation to raise the bound.

- `coding_format_check`
- `core_document_tabs`
- `application_document_tab_host`
- `application_release_maintenance`
- `sqlite_private_store_open`
- `sqlite_release_maintenance_files`
- `qt_release_maintenance_scale_100`
- `qt_release_maintenance_scale_125`
- `qt_release_maintenance_scale_200`
- `qt_composed_loading_scale_100`
- `qt_composed_home_sign_in`
- `qt_composed_cli_read_consent`
- `qt_composed_resident_drawer_filters`
- `qt_shared_snapshot_navigation`
- `qt_document_tabs_scale_100`
- `qt_document_tabs_scale_125`
- `qt_document_tabs_scale_200`
- `qt_workbench_focus_order_scale_100`
- `qt_workbench_focus_order_scale_125`
- `qt_workbench_focus_order_scale_200`
- `qt_workbench_title_row_scale_100`
- `qt_workbench_title_row_scale_125`
- `qt_workbench_title_row_scale_200`
- `qt_workbench_home_discovery_scale_100`
- `qt_workbench_home_discovery_scale_125`
- `qt_workbench_home_discovery_scale_200`
- `qt_workbench_conversation_reveal`
- `qt_workbench_overview`
- `qt_native_window_geometry`
- `qt_diff_axis_scale_150`
- `qt_markdown_body_1_0`
- `qt_markdown_body_1_25`
- `qt_markdown_body_2_0`
- `qt_composed_markdown_1_0`
- `qt_composed_markdown_1_25`
- `qt_composed_markdown_2_0`
- `qt_public_preview_conversation`
- `release_update_native_https`
- `modified_endpoint_fixture_contract`
- `git_pristine_remount_recovery`
- `application_large_review_cache_bytes`
- `application_large_review_resources`
- `application_startup_failure_source`
- `application_device_flow_cli_choice`
- `application_selected_file_safety_public_path`
- `application_attended_recent_replay`
- `release_update_transaction`
- `release_update_native_staging`
- `application_release_maintenance_public_path`
- `application_release_update_public_path`
- `application_profile_second_window_close_public_path`
- `application_personal_discovery_public_path`
- `application_profile_two_account_isolation_public_path`
- `application_profile_window_layout_drain`
- `application_document_tabs_restoration_public_path`
