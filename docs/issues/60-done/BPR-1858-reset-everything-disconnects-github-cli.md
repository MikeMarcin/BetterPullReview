---
schema: "implement-rfc.card.v1"
id: "BPR-1858"
title: "Reset everything signs out and stops using GitHub CLI; Reset connections does only that"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"owner-report","found_by":"owner, 2026-11-04: \"reset everything doesn't seem to fully reset everything. My GitHub CLI was still connected\"","reservation":"docs/issues/_reservations/BPR-1858--BPR-1858--f52e1b0f72a8.json","related_cards":["BPR-1602","BPR-1844","BPR-1846","BPR-1850","BPR-1843"],"amends":"BPR-1602:credentials (no reset reads or deletes credentials), by owner decision 04ebe7a6-945d-4bb8-877e-7d83d2274c7e in attended-review session bbad2aafbd0e0e5c7487e5df46e870fe","frontend_design":"loaded: frontend-design:frontend-design before the v001 wording and again before the Reset connections row and dialog; both inside the accepted BPR-1602 ledger grammar","acceptance":["BPR-1858:policy","BPR-1858:composed","BPR-1858:wording","BPR-1858:gates","BPR-1858:owner"]}
acceptance: [{"criterion":"BPR-1858:policy","outcome":"pass","revision":"0a93a11b3","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_profile_data_reset|core_remembered_connection_policy|sqlite_profile_data_reset_files|sqlite_release_maintenance_files|sqlite_remembered_connection_storage|application_remembered_connection_host)$'","detail":"Core: device-bindings is the connections area, removed only by Reset connections and Reset everything; Reset connections removes no other area, needs no acknowledgement and offers no copy; forget_remembered_connections clears every row and retired identity in one revision, keeps the high-water marks, refuses stale work, and a forgotten identity cannot return. Adapter: the two sign-out scopes erase exactly the inspected credentials once before device-bindings goes; the other resets never call the cleanup; a sign-out reset without a vault refuses unavailable and removes nothing; the purge course is unchanged."}},{"criterion":"BPR-1858:composed","outcome":"pass","revision":"0a93a11b3","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_remembered_connection_public_path|application_profile_data_reset_public_path|application_profile_recovery_public_path|application_release_maintenance_public_path)$'","detail":"reset_everything_disconnects_github_cli: after Use GitHub CLI, a start with Reset everything reports SignedOut, is no_profile at the first-run sign-in, remembers no CLI choice, keeps the high-water marks, gave no consent and ran no gh check; the next launch stays signed out; Use GitHub CLI asks consent again. reset_signs_out_a_native_sign_in (Reset connections, then Reset everything): a device sign-in restored at launch is, after the reset, no_profile at Sign in to GitHub with no fixture-vault entry and no device-bindings file, no remembered connection, the same high-water marks, and stays signed out next launch; Reset connections keeps review-queue, workbench and appearance settings, epoch-catalog and window-restoration as the same native files, Reset everything replaces them. Negative control: with the credential cleanup withheld the course fails ('the reset reports that it signed out'; the adapter refused)."}},{"criterion":"BPR-1858:wording","outcome":"pass","revision":"0a93a11b3","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(qt_profile_data_reset_views|qt_settings_window.*)$'","detail":"Five rows with Reset connections… above Reset everything…, each line under 60 characters; Reset connections names the profile, removes the app's sign-ins with their tokens and the GitHub CLI connection, says gh itself stays signed in, keeps gh's sign-in and review data, asks no acknowledgement, Cancel default, dispatches {connections}; Reset everything says it signs out, lists Connections in Removes and no longer keeps the app's sign-ins; notices for signed out, connections retained and nothing signed in, for both resets."}},{"criterion":"BPR-1858:gates","outcome":"pass","revision":"0a93a11b3","evidence":{"kind":"command","command":"ctest --preset dev -R '^(coding_format_check|architecture_boundaries|architecture_boundary_contract|application_composition_contract|application_review_round_trip_composition_contract)$'; python3 scripts/verify_documentation.py --root .","detail":"Format (clang-format 22.1.8), architecture_boundary_contract, application_composition_contract and the resealed round-trip contract pass; DOCUMENTATION_OK. architecture_boundaries fails only on main's pre-existing QToolTip::showText at workbench_window.cpp:5014 (BPR-1850); with that one line masked locally it passes, and the file was restored byte-identical."}},{"criterion":"BPR-1858:owner","outcome":"pass","revision":"0a93a11b3","evidence":{"kind":"observation","detail":"Human Accept, no notes, in session ae0e8cfbc5c7c1c8e2b2c7fd97204844 on frozen build of 0a93a11b3 (frozen-0a93a11b3-v002), consumed through event 11: BPR-1858-reset-everything-wording-v002 decision 574b44a9-3411-4896-8331-5275e2ece927 (fingerprint 3281dfe6) and BPR-1858-reset-connections-v002 decision d551a7b2-2427-4322-942a-fdeb59e774e7 (fingerprint 2ff6cf3e). Earlier v001 in session bbad2aaf: wording Accept (770264c4), native sign-in kept Reject (04ebe7a6), which directed v002."}}]
---

# BPR-1858: Reset everything signs out and stops using GitHub CLI; Reset connections does only that

## Report

The owner, 2026-11-04: "reset everything doesn't seem to fully reset
everything. My GitHub CLI was still connected".

## Cause

[[BPR-1602]] made every reset keep `remembered-connections.sqlite3` and
`device-bindings.sqlite3` and never call the vault ("Credentials, their device
bindings and remembered connections … are never part of a reset"). The GitHub
CLI choice is a `github_cli` row in the remembered connections. At the next
start `remember_startup` found it, `restore_cli` added the profile to the run's
CLI consent and activated it, and the profile was connected through gh again
with no question. A sign-in made in the app was restored the same way from its
binding and vault entry.

## Owner decisions

- Session `bbad2aafbd0e0e5c7487e5df46e870fe` (2026-11-04), candidate
  `BPR-1858-reset-wording-v001`: **Accept** (decision
  `770264c4-fceb-4b22-b275-6318aea2481e`). The v001 confirmation said
  accurately that Reset everything disconnects GitHub CLI without signing gh
  out.
- Same session, candidate `BPR-1858-native-sign-in-kept-v001` ("Reset
  everything keeps a sign-in made in the app itself"): **Reject** (decision
  `04ebe7a6-945d-4bb8-877e-7d83d2274c7e`), note "Reset everything, add a
  separate reset connections option on that page too."

This card therefore **amends BPR-1602:credentials**. Reset everything also
signs out the app's own sign-ins. Troubleshooting gains **Reset connections**,
which only forgets connections: the app's sign-ins and the GitHub CLI choice.
It keeps review data, settings, saved sources and caches. Vault deletion is
limited to this profile's own entries, exactly those its device bindings name.

## Boundary

- The app never signs gh out. gh's sign-in belongs to gh and to every other
  tool that uses it; no reset runs a gh command or changes gh configuration.
- After Reset connections or Reset everything the profile opens at the
  first-run **Sign in to GitHub**. It connects through gh again only when the
  person presses **Use GitHub CLI** and answers its read consent again.
- Only this profile's own vault entries are deleted, never another profile's
  and never gh's. Profile purge stays the full removal. It excludes shared
  updater and registry state and is not an uninstall.

## Design as built

- **Core decides.**
  - `profile_data_reset::connections` and `profile_data_reset_signs_out`
    (true for connections and everything) are in
    `core/profile_data_reset.hpp`.
  - `device-bindings` is the new `connections` area; only those two resets
    remove it. Reset connections removes no other area. It is not destructive,
    so it asks no acknowledgement and offers no copy.
  - `forget_remembered_connections` (in `core/remembered_connection.hpp`)
    removes every row and retired identity in one revision and keeps the
    identity high-water marks, so a forgotten identity cannot return over the
    review data Reset connections keeps.
- **The maintenance adapter performs**, with journal scope 15 for Reset
  connections.
  - A scope that signs out is handled like the purge's credential step:
    capture inspects this profile's credential inventory and the journal
    records it. Replay deletes both slots of every entry the bindings name
    through `release_profile_credentials`, then removes the binding store.
  - Startup recovery finishes an interrupted reset the same way.
  - Without a vault, such a reset over existing bindings refuses
    (`unavailable`) and removes nothing.
- **The startup step.**
  - `run_profile_data_reset` takes the vault, then saves the forget-all result
    into the remembered connections it keeps.
  - The startup gives it the profile's platform vault on a worker it owns and
    drains. A course gives it the fixture vault (`make_github_fixture_reset_vault`),
    never the keychain.
  - The outcome records `SignedOut`, or `ConnectionsRetained` when the
    remembered connections could not be saved.
- **Qt presents.** Troubleshooting lists Clear caches, Reset settings, Reset
  review data, **Reset connections** ("Signs out and stops using GitHub CLI.
  Keeps the rest.") and Reset everything ("All of the above, as on first
  launch."). The lead is "Each reset reopens this profile’s windows. Reset
  connections and Reset everything sign you out."
  - **Reset connections**
    - Lead: "This profile signs out of GitHub and stops using GitHub CLI, then
      reopens at Sign in to GitHub. It doesn’t sign you out of GitHub CLI
      itself."
    - Removes: the app's sign-ins and their saved tokens, this profile's
      GitHub CLI connection, which account each server uses.
    - Keeps: GitHub CLI's own sign-in, notes, drafts and read marks, settings
      and saved sources, caches, logs.
  - **Reset everything**
    - Lead: "…starts again as on first launch: it signs out and stops using
      GitHub CLI. It doesn’t sign you out of GitHub CLI itself, and nothing on
      GitHub changes."
    - Removes gains "Connections: sign-ins made in this app and their saved
      tokens, this profile’s GitHub CLI connection".
    - Keeps no longer lists the app's sign-ins.
  - The notice says the profile was reset (or connections reset) and signed
    out, that gh itself is still signed in, and how to connect again.

## Audit: what Reset everything leaves behind

| Item | Where | After this card |
|---|---|---|
| GitHub CLI choice and account | `remembered-connections` rows | Forgotten (also by Reset connections) |
| Sign-in made in the app (device grant) | this profile's vault entries, `device-bindings`, its row | Deleted through the credential cleanup (also by Reset connections) |
| CLI consent for the run | in memory, per runtime | Gone: the profile reopens on a new runtime |
| gh avatar helper ([[BPR-1846]]) | per-launch private `GH_CONFIG_DIR`, in-memory hold-off; pictures in `markdown-images` | Nothing persistent; the cache store is removed |
| Notification state ([[BPR-1850]]) | `pull-request-activity` (settings) | Removed |
| GHES and other saved servers, saved repositories, observer rules, Home queue | `review-queue` (saved sources) | Removed |
| CLI-discovered server profiles | in-memory profile catalog | Rebuilt only on Use GitHub CLI |
| Recently viewed and the Home Active review ([[BPR-1843]]) | `window-restoration` (settings) | Removed |
| Filters ([[BPR-1844]]) | `view-filters` (settings) | Removed |
| Agent setup | `agent-setup` (settings) | Removed |
| Caches | `repositories/`, `incoming/`, `locks/`, `quarantine/`, `mcp-snapshot/`, `epoch-catalog`, `markdown-images`, `review-overviews` | Removed |
| Activity, review data, Insights | `review-ledger`, `review-tours`, `review-cycles`, `publication-git-v1/`, `review-mutation-recovery`, `insights` | Removed |
| Identity high-water marks | `remembered-connections` (file kept) | Kept on purpose, so a forgotten identity cannot alias a new one |
| gh's own sign-in | gh's configuration and keychain | Never touched |
| Diagnostic logs | `logs/` | Kept; they have their own Clear |
| Agent workspaces | `agent-workspace-ownership-v1/` | Kept |
| Isolation scaffolding | `templates/`, `hooks/` | Kept; rebuilt by the app, holds no choice |
| Shared registry and updater state | `profile-registry`, `release-update`, `update-draft-recovery`, `profiles/`, `updates/` | Kept: shared by every profile; purge excludes them too; not an uninstall |
| Other profiles, exported files, the person's checkouts | outside this profile's root | Kept |

## Open

- A connection made again after a sign-out reset gets a fresh host identity
  for its server, as after Forget. That is existing behaviour. The
  single-host fixture cannot serve it, so the composed CLI course stops at the
  consent question on reconnect.
- Removing the platform vault entries is proved through the fixture vault
  (the same port and refusals). The real keychain deletion path is the purge's
  existing `release_profile_credentials`, unchanged here. No course deletes a
  real keychain item.

## Acceptance

- **BPR-1858:policy** Core: only Reset connections and Reset everything sign
  out and remove `device-bindings`, and Reset connections removes nothing else.
  Forget-all clears every row and retired identity in one revision, keeps the
  high-water marks and refuses stale work. Adapter: the sign-out scopes delete
  exactly the inspected credentials once, the others never call the cleanup,
  and a sign-out without a vault removes nothing.
- **BPR-1858:composed** On the real composition:
  - GitHub CLI: connect, then Reset everything. The app is not connected,
    Home is the first-run sign-in, no CLI choice is remembered, gh was asked
    nothing, the next launch does not reconnect, and Use GitHub CLI asks
    consent again.
  - App sign-in: sign in with a device grant, then Reset connections, and
    separately Reset everything. Each reports it signed out, opens at Sign in
    to GitHub with no vault entry and no device binding left, remembers no
    connection, keeps the high-water marks and stays signed out on the next
    launch. Reset connections keeps the saved-sources, settings, cache and
    window stores as the same files; Reset everything removes them.
  - Negative control: without the credential cleanup, the sign-out course
    fails.
- **BPR-1858:wording** Both confirmations, the notices and the Troubleshooting
  lines say what signs out and that gh itself stays signed in.
- **BPR-1858:gates** Focused tests, clang-format 22, documentation gate,
  composition contracts.
- **BPR-1858:owner** The owner accepts the revised Reset everything wording
  and the new Reset connections row and dialog.

## Verification (candidate `0a93a11b3`, on main `33e1e088a`)

Focused rows pass on the full dev build:
- core: `core_profile_data_reset`, `core_remembered_connection_policy`
- adapter: `sqlite_profile_data_reset_files`, `sqlite_release_maintenance_files`,
  `sqlite_remembered_connection_storage`
- application: `application_remembered_connection_host`
- Qt: `qt_profile_data_reset_views`, `qt_settings_window` at all four scales
- composed: `application_remembered_connection_public_path`,
  `application_profile_data_reset_public_path`,
  `application_profile_recovery_public_path`,
  `application_release_maintenance_public_path`
- contracts: `application_composition_contract`,
  `application_review_round_trip_composition_contract` (resealed),
  `architecture_boundary_contract`, `coding_format_check`
- the documentation gate

`architecture_boundaries` fails only on main's pre-existing BPR-1850
`QToolTip::showText`; with that one line masked locally it passes.

The recovery course compares remembered rows and high-water marks from a copy
of the store and its WAL, because the reset now reads that store.

## Owner review v001 (handed back)

Frozen build `/Users/jaeger/BPR-attended/BPR-1858-reset-v001.vw6i9430` from
`12c2d137a`, session `bbad2aafbd0e0e5c7487e5df46e870fe` (state
`/Users/jaeger/.codex/attended-review/bpr1858-v001-Bi416D/state`), consumed
through event 31. Its host is stopped.
- `BPR-1858-reset-wording-v001`: Accept.
- `BPR-1858-native-sign-in-kept-v001`: Reject, note "Reset everything, add a
  separate reset connections option on that page too."

The v001 frozen root is superseded and can be removed with
`scripts/freeze_attended_build.py remove`.

## Owner review v002 (asked)

Frozen build `/Users/jaeger/BPR-attended/BPR-1858-reset-v002._dtbc1sj` from
`0a93a11b3` (executable sha256 `3a88e773ff57…`), launched with
`python3 /Users/jaeger/BPR-attended/BPR-1858-reset-v002._dtbc1sj/launch.py`;
its `--check` passed at freeze. In the fixture build a reset signs out a test
sign-in, never the keychain or gh.

Session `ae0e8cfbc5c7c1c8e2b2c7fd97204844` (fresh, short), state
`/Users/jaeger/.codex/attended-review/bpr1858-v002-QmGl6F/state`, hosted over
Tailscale Serve, both candidates ready and protected at cursor 6. One
Accept/Reject question each:
- `BPR-1858-reset-everything-wording-v002` (revision `frozen-0a93a11b3-v002`):
  the Reset everything confirmation as revised.
- `BPR-1858-reset-connections-v002` (same revision): the new Reset
  connections row and its confirmation.

## Owner verdict and landing (2026-11-05)

The owner accepted both candidates, with no notes, and handed the session
back (consumed through event 11):
- `BPR-1858-reset-everything-wording-v002`: decision `574b44a9-3411-4896-8331-5275e2ece927`.
- `BPR-1858-reset-connections-v002`: decision `d551a7b2-2427-4322-942a-fdeb59e774e7`.

Closeout:
- The v002 host (serve PID 93523, Tailscale helper PID 93534) was stopped
  through the helper's `stop`, and both PIDs were confirmed gone.
- Both frozen roots (`BPR-1858-reset-v001.vw6i9430`,
  `BPR-1858-reset-v002._dtbc1sj`) were removed with
  `scripts/freeze_attended_build.py remove`.
- Main `a2e2ca244` was merged. The only conflict was the round-trip
  contract's two digest pins, which were resealed over the merged
  `github_application.cpp` and production manifest with both histories kept.
- The focused reset rows, `coding_format_check` and the documentation gate
  were rerun on the merged tree.
