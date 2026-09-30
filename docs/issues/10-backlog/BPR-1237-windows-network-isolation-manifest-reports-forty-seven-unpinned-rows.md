---
schema: "implement-rfc.card.v1"
id: "BPR-1237"
title: "Windows: test_network_isolation_manifest reports 47 rows that are uncontained or unlabelled without a pin"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json","related_cards":["BPR-1231"]}
---

# BPR-1237: Windows: test_network_isolation_manifest reports 47 rows that are uncontained or unlabelled without a pin

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`test_network_isolation_manifest` fails on Windows, at `46725457` and at every
revision this station ran since `22d18416`. It reports 142 findings over 47
rows. Each row either uses `bpr_windows_uncontained` without being in
`WINDOWS_UNCONTAINED_TESTS` in `scripts/verify_test_network_isolation.py`, or is
registered without the Windows launcher (`… does not start with
[…better_pull_review_network_sandbox.exe]`) and lacks the
`bpr_network_isolated` label. The rows:

- logging: `diagnostic_context_age`, `diagnostic_controls`,
  `logging_crash_recovery`, `logging_session_retention`,
  `quill_logging_content_policy`, `application_logging_cancel_startup`,
  `application_logging_capability-failure`, `application_logging_full`,
  `application_logging_unavailable`, `application_diagnostic_controls_public_path`;
- tours: `application_review_tour_cold_import_public_path`,
  `application_review_tour_composition_public_path`,
  `application_review_tour_composition_scale_125`, `_scale_200`, the nine
  `application_review_tour_publication_*` rows, `git_haves_trimmed_pack_tour-source`,
  `sqlite_tour_owner_channel`;
- private review: `application_private_review_file`, `_host`, `_native`,
  `_retry`, `sqlite_private_review_storage`;
- staged review: `staged_review_restart`, `staged_review_session`,
  `application_review_submission_composition_scale_125`, `_scale_200`;
- others: `application_active_changed_scaling`,
  `application_attended_recent_replay`,
  `application_document_tabs_restoration_public_path`,
  `application_first_tab_comparison`, `application_large_review_timing_control`,
  `application_offline_inventory`, `application_profile_window_layout_drain`,
  `application_rate_limit_render_recovery`,
  `application_recent_account_isolation`,
  `application_release_maintenance_public_path`,
  `application_release_update_public_path`, `sqlite_cached_review_overview`,
  `sqlite_release_maintenance_files`.

The rows added and measured by this station (`532cffaf`, `6533bd11`) are not
among them.

## Scope

- For each row, decide from a Windows measurement: pin it, with its measured
  cause, or register it through the launcher and label it. Some are already
  registered with `WINDOWS_UNCONTAINED` in CMake and only need the pin and the
  measurement. Others are raw `add_test` rows the gate expects to go through
  `bpr_add_network_isolated_test`.
- Pinning without a measurement is not acceptance. Each pin's comment names
  what was measured.

## Acceptance

- `test_network_isolation_manifest` passes on Windows.
- It still passes on macOS and Linux.
