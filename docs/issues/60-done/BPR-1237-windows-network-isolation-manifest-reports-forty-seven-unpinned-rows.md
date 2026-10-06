---
schema: "implement-rfc.card.v1"
id: "BPR-1237"
title: "Windows: test_network_isolation_manifest reports 47 rows that are uncontained or unlabelled without a pin"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json","related_cards":["BPR-1231"]}
acceptance: [{"criterion":"test_network_isolation_manifest passes on Windows","outcome":"pass","revision":"3134abc5cefad2f3d7578cf572aa29f125b61024","evidence":{"kind":"command","path":"scripts/verify_test_network_isolation.py","detail":"bpr-main-win build/windows-dev Debug, MSVC 14.51: test_network_isolation_manifest passes (20.92 s) and test_network_isolation_structure_contract passes (9.95 s) at current main; every WINDOWS_UNCONTAINED row carries a measured pin (dbe6966ea/e517d66d0 for this card's 46 plus BPR-0967's 10; later BPR-1185, BPR-1082, BPR-1083 pins measured on this station)."}},{"criterion":"It still passes on macOS and Linux","outcome":"pass","revision":"c1cd6b585","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1280-command-return-saves-the-note-being-written.md","detail":"After the pin change e517d66d0: macOS integrator build/dev-tour-cli passes test_network_isolation_manifest at c1cd6b585 (BPR-1280 boundary record); Linux bpr-main-linux passes it at e517d66d0 (4/4 with the structure contract and the two re-contained rows) and at 15bb8c921 (BPR-1185 Linux note)."}}]
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

## Every row measured and pinned; the manifest passes on Windows (bpr-main-win, 2026-10-02)

At `dbe6966ea` the manifest listed 58 rows registered `bpr_windows_uncontained`
with no Windows pin. 47 are this card's, and 11 are BPR-0967's
`application_large_review_*` rows. Windows 11 Home 10.0.26200, MSVC 14.51,
Qt 6.11.2, `build/windows-dev` Debug.

**Contained.** A station probe, not committed, ran each row through
`better_pull_review_network_sandbox.exe`. It used the row's own command,
`ENVIRONMENT`, `ENVIRONMENT_MODIFICATION`, `WORKING_DIRECTORY`, `TIMEOUT` and
pass/fail expressions from `ctest --show-only=json-v1`. 56 failed and two
passed. Each failure stops at the row's private data root (its
canonicalisation or owner-private narrowing), at the profile registry's
ancestor-chain walk, or at a store opened under such a root:

- the large-review benchmark's courses: "large-review-benchmark-failed: private
  data root parent";
- the tour and other composed courses: `e2e::private_data_root()`, which the
  tour courses leave silently and the others report as "private data root";
- "owner-private root", "wire fixture root is owner-private", "native private
  root before composed startup" and similar narrowing steps, the four logging
  courses among them;
- `diagnostic_context_age` and `diagnostic_controls`: "create_directory: Access
  is denied" inside their `QTemporaryDir`;
- `git_haves_trimmed_pack_tour-source` and `sqlite_release_maintenance_files`:
  "canonical: Access is denied";
- the private-review seeds: `STARTUP_PROFILE_REFUSAL:registry_unavailable`;
  `application_offline_inventory` and `_recent_account_isolation`: "desktop
  admitted"; the auth seed's exit 3;
- store opens: "production staged journal opens", "accepted overview saves",
  "public factory opens migrated ledger", and the private-review host's
  unchecked `std::expected`.

**Uncontained, as registered**, all 58 pass, with `ctest -j4` over the set
taking 2,772.57 s. One row, `application_private_review_retry`, failed under
`-j4` only at cleanup: `WinError 32` on its held `review-ledger.sqlite3`. It
passed alone in 21.97 s.

**The change, in one commit so the manifest is never red in between:**

- 56 pins in `WINDOWS_UNCONTAINED_TESTS`, all `dos-volume-name`, the term this
  file already uses for root canonicalisation and narrowing refused inside the
  container (BPR-0833, BPR-0874, BPR-1031). Each group's comment names the
  measured step.
- `sqlite_tour_owner_channel` passes contained (`tour-owner-channel-tests:pass`),
  so its `WINDOWS_UNCONTAINED` keyword is removed and it runs in the container.
  So does BPR-0967's `application_large_review_history_session_race`.
- `WINDOWS_PENDING_REGISTRATIONS` is now empty, because every audited row is
  reconciled. Its test plants a pending registration of its own in place of
  borrowing one.

After the change, on Windows: `test_network_isolation_manifest` passes
(7.97 s), `test_network_isolation_structure_contract` passes, and both moved
rows pass contained under CTest. The macOS and Linux manifests count the same
registrations and need a rerun.

### Linux at `e517d66d0` (bpr-main-linux, 2026-10-02)

WSL2, GCC 15.2, `build/dev` Debug. The build has 0 `warning:` lines. `ctest
--preset dev --output-on-failure -R
"^test_network_isolation_(manifest|structure_contract)$|^sqlite_tour_owner_channel$|^application_large_review_history_session_race$"`
passes **4 of 4**:

- `test_network_isolation_structure_contract`, 2.59 s
- `test_network_isolation_manifest`, 1.34 s
- `sqlite_tour_owner_channel`, 5.77 s
- `application_large_review_history_session_race`, 0.98 s

The two rows contained again on Windows pass under the Linux network sandbox,
and the Windows pin-set change leaves the Linux manifest and structure gates
green. No remote mutation.

A correction to `e517d66d0`'s commit message. It counted 11 large-review
pins for BPR-0967 and 45 for this card. The pins are 10 and 46:
`application_large_review_history_session_race` moved into the container
instead of being pinned, and `application_large_review_timing_control` is
this card's. The section above and the file's comments are right.
`sqlite_tour_owner_channel`, moved into the container there, returns to a
measured pin with BPR-1168's named pipe (see BPR-1168's Windows note).

## Verified on current main and closed (bpr-main-win, 2026-10-05)

At `3134abc5c`, on `build/windows-dev` (Debug, MSVC 14.51):
`test_network_isolation_manifest` passes (20.92 s) and
`test_network_isolation_structure_contract` passes (9.95 s). Since this card's
pins, every row registered `WINDOWS_UNCONTAINED` has been pinned with its
measurement before it reached `main`. The latest are the [[BPR-1185]],
[[BPR-1082]] and [[BPR-1083]] rows, measured on this station at `1f2a8452c`
and `4d2694a54`.

Both other platforms pass after the pin change (`e517d66d0`):

- **macOS**: the integrator's `build/dev-tour-cli` passes
  `test_network_isolation_manifest` at `c1cd6b585` ([[BPR-1280]]'s boundary
  record).
- **Linux**: `bpr-main-linux` passes it at `e517d66d0` (above) and again at
  `15bb8c921` ([[BPR-1185]]'s Linux note).

No code changed for this close.
