# BPR-0820: how each dev-preset application row gets its data root

Checked 2026-09-17 by dispatch dsp-003 of run-20260916-stations, on main
`0fa601af` with BPR-0820's change in the working tree, the tree that change was
committed from. Every `add_test` or `bpr_add_network_isolated_test` block under
`tests/**/CMakeLists.txt` that references `app_better_pull_review` is listed,
with how it avoids the default application data root. The rows were sorted by a
script reading the registrations. Driver scripts and `.cmake` drivers were sorted
by the root flags they pass, and each driver's launches were also read by hand.
Rows marked "exits before composition" pass only flags that return before a
profile is composed.

A second, durable guard sits in the application itself. A tests-enabled
`--self-test` or `--render-evidence` run with no fixture and no
`--test-application-data-root` gets a private owner-private temporary profile.
Any automated run that still reaches composition without a root prints
`AUTOMATED_PROFILE_PRIVATE_FAILED` and exits before a lease is taken or a store
opened (`src/app/main.cpp`, `automated_run_uses_private_profile` and the gate
before the desktop starts).

Control: with `--private-data-root` removed from the `runtime_process_surface`
registration, the same sort flags exactly that row as reaching the default root.
The control ran on a copy of the file, never against the default root.

Result: 55 rows, none reaching the default root.

```text
ok  rfc0017_release_absence_negative_control: static release inventory, no launch
ok  test_network_isolation_application: exits before composition
ok  application_public_path: main.cpp automated-run private root
ok  application_public_path_ghes_cli_unavailable: main.cpp automated-run private root
ok  application_device_authorization_public_path: main.cpp automated-run private root
ok  application_device_authorization_unavailable: main.cpp automated-run private root
ok  application_device_authorization_registered_course: fixture root
ok  application_device_authorization_refresh_course: fixture root
ok  application_device_authorization_orphan_course: fixture root
ok  application_device_authorization_browser_handoff: fixture root
ok  application_device_authorization_browser_absent: fixture root
ok  application_composition_capabilities_inside_boundary: main.cpp automated-run private root
ok  application_composition_capabilities_product_run: main.cpp automated-run private root
ok  application_device_authorization_panel_course: fixture root
ok  application_device_authorization_panel_absent: fixture root
ok  application_device_authorization_ghes_refusal: main.cpp automated-run private root
ok  application_device_authorization_release_contract: main.cpp automated-run private root
ok  application_anonymous_public_preview_unavailable: main.cpp automated-run private root
ok  application_anonymous_public_preview_release_contract: main.cpp automated-run private root
ok  application_anonymous_public_preview_enabled_probe: main.cpp automated-run private root
ok  application_anonymous_public_preview_live_candidate_wiring: main.cpp automated-run private root
ok  application_native_notification_attended_contract: driver application_native_notification_attended_contract.cmake: fixture/explicit root
ok  application_icon: exits before composition
ok  application_native_appearance_probe_contract: driver native_appearance_probe_contract.cmake: fixture/explicit root
ok  application_github_public_path_github_com: fixture root
ok  application_github_public_path_uiforetw_74: fixture root
ok  application_uiforetw_selected_diff_contract: driver application_uiforetw_selected_diff_contract.cmake: fixture/explicit root
ok  application_diff_workbench_public_path: fixture root
ok  application_workbench_composition_contract: driver application_workbench_composition_contract.cmake: fixture/explicit root
ok  application_review_ledger_composition_contract: driver application_review_ledger_composition_contract.cmake: fixture/explicit root
ok  application_review_round_trip_composition_contract: driver application_review_round_trip_composition_contract.cmake: fixture/explicit root
ok  application_defect_composition_contract: driver application_defect_composition_contract.cmake: fixture/explicit root
ok  application_overview_since_review_contract: driver application_overview_since_review_contract.cmake: fixture/explicit root
ok  application_review_behaviour_contract: driver application_review_behaviour_contract.cmake: fixture/explicit root
ok  application_saved_server_contract: driver application_saved_server_contract.cmake: fixture/explicit root
ok  application_threaded_reply_contract: driver application_threaded_reply_contract.cmake: fixture/explicit root
ok  application_appearance_theme_customization_contract: driver application_appearance_theme_customization_contract.cmake: fixture/explicit root
ok  application_fixture_verdict_contract: driver fixture_verdict_contract.py: fixture/explicit root
ok  application_attended_launch_contract: driver attended_launch_contract.py: fixture/explicit root
ok  application_diff_workbench_shutdown_public_path: fixture root
ok  application_automated_run_budget: fixture root
ok  application_github_public_path_ghes_3_18: fixture root
ok  application_anonymous_public_preview_ghes_refusal: fixture root
ok  application_anonymous_public_preview_public_path: fixture root
ok  application_epoch_portable_occurrence_two_catalogues: driver epoch_portable_occurrence_public_path.py: fixture/explicit root
ok  application_epoch_persistence_${fixture_test_name}: driver epoch_persistence_contract.cmake: fixture/explicit root
ok  application_epoch_maintenance_public_path: driver epoch_maintenance_public_path_contract.cmake: fixture/explicit root
ok  application_appearance_gesture_contract: driver appearance_gesture_contract.cmake: fixture/explicit root
ok  application_appearance_store_fallback_contract: driver appearance_store_fallback_contract.cmake: fixture/explicit root
ok  binary_dependency_surface: static binary inspection, no launch
ok  runtime_process_surface: script private root (--private-data-root)
ok  composed_application_hosts: static symbol inspection, no launch
ok  application_render: main.cpp automated-run private root
ok  application_profile_activation_public_path: driver runs the app only with refused launch arguments or --application-icon-probe (exits before composition); its owner processes are a test executable with an explicit root
ok  application_epoch_ordinary_update_recreated: driver epoch_ordinary_update_public_path.py: fixture/explicit root
ROWS_REACHING_DEFAULT_ROOT_OR_UNCLASSIFIED=0
```
