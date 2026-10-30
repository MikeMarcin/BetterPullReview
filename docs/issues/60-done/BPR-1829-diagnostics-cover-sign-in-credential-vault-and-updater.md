---
schema: "implement-rfc.card.v1"
id: "BPR-1829"
title: "Diagnostics cover sign-in, the credential vault and the updater"
status: "Done"
rfcs: []
depends_on: ["BPR-1828"]
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1829--BPR-1831--37b8c8c13a6b.json","kind":"defect","found_by":"BPR-1828's coverage inventory: device sign-in, the credential vault, account switching and the updater left no diagnostic record, so a failed sign-in or update left no trace in an export","related_cards":["BPR-1828","BPR-1830","BPR-1831","BPR-1722"],"shared_category":"github (github.sign_in), credential (credential.vault), updater (updater.release)"}
acceptance: [{"criterion":"AC1: the coverage inventory is updated, before and after","outcome":"pass","revision":"e31a1cc26","evidence":{"kind":"observation","detail":"Inventory below; before is main 594e8e842 (BPR-1828).","path":"docs/issues/60-done/BPR-1829-diagnostics-cover-sign-in-credential-vault-and-updater.md"}},{"criterion":"AC2: device sign-in, renewal, Sign out, account changes, vault store/load/delete and updater check/download/verify/apply record started/succeeded/failed/cancelled with typed failures","outcome":"pass","revision":"e31a1cc26","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(core_operation_diagnosis|application_credential_vault_diagnostics|application_device_authorization_host|application_device_authorization_refresh|application_github_host|application_release_update)$'","detail":"core_operation_diagnosis: sign-in, vault and updater lines round-trip; shared words are admitted by each family that reports them; another family's failure, a process or a write flag is refused. application_credential_vault_diagnostics: store success, locked, cancelled and a failure-less error (operation_failed); load found (count 1), absent (0), access_denied; delete deleted (1), absent (0), corrupt; refresh slots; cancel passes through; a throwing submission records unavailable and rethrows; every vault failure, release update reason, refresh failure and device stage/reason pair forms a valid line of its family. application_device_authorization_host: start at the device-code request, succeeded after two polls with duration_ms=10000 on the host clock, authorization_denied, device_flow_disabled with count=0, cancelled, a fresh start after a terminal. application_device_authorization_refresh: rotation started then succeeded (duration_ms=2000), refresh_rejected, refresh_absent, release (Sign out) cancelled. application_github_host: account_change count=1 on acceptance; Sign out started, account_change count=0, succeeded; no_session when nothing is signed in. application_release_update: check, download (count=100 bytes), verify, refresh and install handoff each start and succeed; offline; wrong_signer; shutdown cancelled. Each course also runs with no log as a control."}},{"criterion":"AC3: the logger and the anonymized export admit exactly the line and nothing private","outcome":"pass","revision":"e31a1cc26","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(quill_logging|quill_logging_content_policy|quill_logging_backpressure|application_release_maintenance|application_operation_diagnostics)$'","detail":"quill_logging: the real logger writes github.sign_in (category github), credential.vault (category credential) and updater.release (category updater) records with the exact line; it drops a sign-in line on the updater operation, an updater failure on a vault line and a sign-in line carrying login=PRIVATE; the metadata preview classifies operations 12 to 14 without their lines. application_release_maintenance test_operation_export: under both policies each new line exports exactly under its category; one carrying a login, a token-shaped failure word or a URL keeps the record with its detail omitted and no PRIVATE text; a credential.vault record filed under category github is refused. Host tests assert that no user code, device code, access or refresh token, verification address, login, display name, node, account number, hostname, release tag or asset name appears in any line. Negative controls (each emission removed in turn, then restored): application_device_authorization_host fails at 'a device-code request records the flow's start', application_github_host at 'an accepted account records one account change', application_release_update at 'a check records its start and success', and quill_logging with the new operations dropped from admission at 'an updater failure on a vault line is refused'."}},{"criterion":"AC4: emitted from the application layer and composed","outcome":"pass","revision":"e31a1cc26","evidence":{"kind":"command","command":"cmake --build build/dev --target app_better_pull_review; ctest --test-dir build/dev -R '^application_review_round_trip_composition_contract$'","detail":"device_authorization_application_host, github_application_host, release_update_application_host and application::with_credential_vault_diagnostics record; no widget or adapter emits them. github_application.cpp wraps the live vault (OwnedVault, after the unavailable fallback) and calls set_diagnostic_log on the GitHub and device hosts; profile_desktop hands the updater the first open profile's log on its account poll. app_better_pull_review builds; application_review_round_trip_composition_contract passes after resealing the full application digest (1af5d890 to 1853b951) and the production app manifest digest (a2498a95 to 1c052a7b); the mutation-branch pin is unchanged. application_device_authorization_refresh_course and application_release_maintenance_public_path pass (the fixture vault is not wrapped, so its store counts are unchanged). core_boundary_contract, architecture_boundaries and architecture_boundary_contract pass; architecture_boundaries first refused a core comment naming the application vault failure type, which was reworded."}},{"criterion":"AC5: gates","outcome":"pass","revision":"e31a1cc26","evidence":{"kind":"command","command":"python3 _coding-standard/scripts/format.py --dry-run <changed files>; python3 scripts/verify_documentation.py --root .","detail":"clang-format 22.1.8 over every changed C++ file reports no violation; DOCUMENTATION_OK; docs/development/build-and-test-reference.md describes the three operations and what their records do not prove. tests/end_to_end/composition_contract_tests.py --cmake cmake ran 8, OK (Release app build included)."}}]
---

# BPR-1829: Diagnostics cover sign-in, the credential vault and the updater

[[BPR-1828]] gave host operations one diagnostic line
(`bpr/core/operation_diagnosis.hpp`) and listed what still left no record. A
failed device sign-in, a locked keychain and a failed update were among them:
nothing reached the export. This card puts them on the same line.

## Acceptance

- **AC1, inventory:** the rows this card covers move from none to their
  records, and what is still uncovered is named.
- **AC2, events:** device sign-in and token renewal, Sign out and account
  changes, credential vault store, load and delete (account slot and refresh
  slot), and the updater's check, download, verify and apply record their
  start and their end as `succeeded`, `failed` or `cancelled` with a typed
  failure.
- **AC3, privacy:** only enums, counts, durations and exit codes. No token,
  device or user code, login, account, hostname, URL, path, release tag or
  asset name reaches a record or an export.
- **AC4, layering:** emitted from the application layer and composed in
  `src/app`; composition digests resealed.
- **AC5, gates:** clang-format 22, focused tests, documentation gate.

## Design

- **Same line, three more families.** `core::operation_diagnosis_family`
  gains `sign_in`, `credential_vault` and `updater`, logged as
  `github.sign_in`, `credential.vault` and `updater.release` (appended to
  `application::diagnostic_operation_names`, so categories `github`,
  `credential` and `updater`). Actions and failures are appended to the
  existing enums. Each family has its own closed failure set: the device
  flow's terminal stage or `device_authorization_reason_code`, the renewal's
  `device_authorization_refresh_failure`, `github_credential_vault_failure`,
  and `core::release_update_reason` (policy reasons collapse to
  `policy_refused`). Words two families share (`cancelled`, `unavailable`,
  `timed_out`, `malformed_response`, `invalid_request`,
  `certificate_rejected`, `redirect_rejected`) are spelled once. A process,
  exit code or write flag on these lines is refused, as before. The logger's
  admission and the export's parse are the BPR-1828 ones, extended by the
  three operations.
- **Sign-in.** `device_authorization_application_host` records
  `device_sign_in` when the core enters `requested` and when it reaches a
  terminal stage, counting token polls and timing on the host's own monotonic
  clock; and `token_renewal` when it reads the refresh slot and when the
  rotated grant is enrolled, the attempt is cancelled or released (Sign out),
  or it asks for re-enrollment. `github_application_host` records `sign_out`
  around the deactivate intent (`no_session` when nothing was signed in) and
  one `account_change` whenever the accepted account differs from the last one
  it recorded (count 1 when one is selected afterwards). Both hosts take the
  log through `set_diagnostic_log`.
- **Vault.** `application::with_credential_vault_diagnostics` wraps the
  composed vault port, like BPR-1828's mutation wrapper: each submission
  records its start, its one terminal records the end, a submission that
  throws records `unavailable` and rethrows, and calls, answers and
  cancellation pass through unchanged. A load or delete counts 1 when the slot
  held an entry.
- **Updater.** `release_update_application_host` records each effect job when
  it is posted and when its terminal is delivered on the decision lane: the
  operation's action, the release update reason, the duration, and a
  download's received bytes. `release_update_service` and
  `release_update_desktop` forward a log; `profile_desktop` hands it the first
  open profile's log on its existing account poll, because the updater is
  desktop-wide and logs are per profile.
- **What a record proves.** A vault record says what the composed vault port
  answered. It does not say which platform store answered (Keychain, DPAPI or
  Secret Service), that an entry survives a restart, or that the store's own
  access control held; the M8/M9 native vault gaps stay where they were. An
  `update_apply` success is the installer handoff, not an installed update
  (only the healthy-start receipt proves that), and an `update_verify` success
  is what the verification port answered, not an independent signature or
  notarization check. A `sign_out` success means the core ended the session
  and issued its invalidation; the credential's erasure is the separate
  `vault_delete` record.
- **Volume.** A sign-in is two records plus vault traffic; the updater checks
  at most hourly. Far inside [[BPR-1722]]'s budget.

## Coverage inventory

Before is main at 594e8e842 ([[BPR-1828]]); after is this card. Rows
BPR-1828 already covered are unchanged and omitted.

| Operation | I/O | Before | After |
|---|---|---|---|
| Device sign-in (code, polls, terminal) | GitHub | none | `github.sign_in` `device_sign_in` start and end |
| Token renewal (rotation, launch renewal) | GitHub, keychain | none | `github.sign_in` `token_renewal` start and end |
| Sign out | process | none | `github.sign_in` `sign_out` start and end |
| Account gained, lost or replaced | process | none (viewer reads only) | `github.sign_in` `account_change` |
| Credential vault, account and refresh slots | keychain | none | `credential.vault` store, load, delete start and end |
| Updater check, download, verify, refresh, install handoff | network, files | none | `updater.release` start and end per job |
| Pasted-token sign-in validation | GitHub | `github.viewer` | unchanged (plus its vault records) |
| MCP helper, tours, Review Cycle | process, files, GitHub | none | none ([[BPR-1830]]) |
| SQLite, clone scan, search, images, avatars, notifications | various | none | none ([[BPR-1831]]) |

## Left uncovered

- The fixture composition (`BPR_BUILD_TESTING` with a fixture) borrows the
  fixture's vault by reference and is not wrapped, so fixture courses show no
  `credential.vault` records; the wrapper's own tests cover them. Device
  sign-in, Sign out and account change records do appear in fixture courses.
- The maintenance controls' vault (profile purge) and the `bpr tour` remote
  CLI's own vault and device host are composed separately and record nothing.
- The updater's refusal of a job past its four-job bound (`busy`) and policy
  refusals decided in core before any job runs record nothing: no effect ran.
  The release-update store (settings load and save) and the archive staging
  writer are not recorded.
- A renewal abandoned by shutdown, rather than released, records no end.
- The device-binding store (remembered sign-in metadata in SQLite) is left to
  [[BPR-1831]]'s SQLite coverage.
