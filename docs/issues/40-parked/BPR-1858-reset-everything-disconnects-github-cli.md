---
schema: "implement-rfc.card.v1"
id: "BPR-1858"
title: "Reset everything leaves the app no longer connected through GitHub CLI"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"owner-report","found_by":"owner, 2026-11-04: \"reset everything doesn't seem to fully reset everything. My GitHub CLI was still connected\"","reservation":"docs/issues/_reservations/BPR-1858--BPR-1858--f52e1b0f72a8.json","related_cards":["BPR-1602","BPR-1844","BPR-1846","BPR-1850","BPR-1843"],"frontend_design":"loaded: frontend-design:frontend-design before changing the Reset everything confirmation, its notice and the Troubleshooting lines; wording only, inside the accepted BPR-1602 ledger","acceptance":["BPR-1858:policy","BPR-1858:composed","BPR-1858:wording","BPR-1858:gates","BPR-1858:owner"]}
acceptance: [{"criterion":"BPR-1858:policy","outcome":"pass","revision":"12c2d137a","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_remembered_connection_policy|core_profile_data_reset|application_remembered_connection_host|sqlite_remembered_connection_storage)$'","detail":"reset_forgets_github_cli: an enabled and a signed-out CLI row are forgotten in one revision with their retired identities; a native row on another server stays selected; high-water marks unchanged; a forgotten identity cannot return; stale revision refused; a state with no CLI row keeps its rows. Only Reset everything forgets the CLI connection."}},{"criterion":"BPR-1858:composed","outcome":"pass","revision":"12c2d137a","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_remembered_connection_public_path|application_profile_data_reset_public_path|application_profile_recovery_public_path)$'","detail":"reset_everything_disconnects_github_cli on the real composition: Use GitHub CLI connects and is remembered; a start with Reset everything reports GithubCliDisconnected, is no_profile with Home at the first-run sign-in, remembers no CLI choice, keeps the high-water marks, gave no consent and ran no gh inventory check; the next launch stays signed out with no gh check; Use GitHub CLI asks read consent again. Negative controls: without the forget the report check fails, and with that check removed 'after Reset everything the app is not connected' fails (the owner's report). Recovery course: --reset-profile-data everything keeps the remembered rows and high-water marks of a store with no CLI row."}},{"criterion":"BPR-1858:wording","outcome":"pass","revision":"12c2d137a","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^qt_profile_data_reset_views$'","detail":"Removes names 'This profile’s GitHub CLI connection'; the lead says it doesn’t sign you out of GitHub CLI itself and no longer says you stay signed in; Keeps names gh's own sign-in and sign-ins made in this app; the notice names a forgotten CLI connection, one that could not be forgotten, and promises nothing when there was none."}},{"criterion":"BPR-1858:gates","outcome":"pass","revision":"12c2d137a","evidence":{"kind":"command","command":"ctest --preset dev -R '^(coding_format_check|architecture_boundary_contract|application_composition_contract|application_review_round_trip_composition_contract)$'; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"Format (clang-format 22) passes; composition contract passes; round-trip contract resealed and passes; DOCUMENTATION_OK; documentation tests OK. architecture_boundaries fails on main 33e1e088a, unchanged here: QToolTip::showText in src/adapters/qt/src/workbench_window.cpp:5014 from BPR-1850's activity info button (BPR-1354 tooltip rule)."}},{"criterion":"BPR-1858:owner","outcome":"pending","revision":"12c2d137a","evidence":{"kind":"observation","detail":"Owner Accept or Reject of the reworded Reset everything confirmation on frozen build BPR-1858-reset-v001; not yet asked."}}]
parked_on: "human-review"
---

# BPR-1858: Reset everything leaves the app no longer connected through GitHub CLI

## Report

The owner, 2026-11-04: "reset everything doesn't seem to fully reset
everything. My GitHub CLI was still connected".

## Cause

[[BPR-1602]] made every reset keep `remembered-connections.sqlite3` as a whole
file ("Credentials, their device bindings and remembered connections … are
never part of a reset"). That store is where the GitHub CLI choice lives: a
`github_cli` row for the server. At the next start `remember_startup` found
the row, `restore_cli` added the profile to the run's CLI consent and
activated it, and the reopened profile was connected through gh again with no
question. The confirmation said so ("You stay signed in"), but the owner
expected Reset everything to start again as on first launch.

## Boundary

- The app never signs gh out. gh's sign-in belongs to gh and to every other
  tool that uses it; the reset runs no gh command and changes no gh
  configuration.
- After Reset everything the profile is not connected through gh. It opens at
  the first-run **Sign in to GitHub**, and connects through gh again only when
  the person presses **Use GitHub CLI** and answers its read consent again.
- Profile purge stays the full removal; it excludes shared updater and
  registry state and is not an uninstall.

## Design as built

- Core decides. `forget_github_cli_connections` (in
  `core/remembered_connection.hpp`) applies Forget to every server whose row is
  a CLI connection, enabled or signed out: the row and that server's retired
  identities go; native rows, other servers and every identity high-water mark
  stay, so a forgotten identity cannot return as a new authority.
  `profile_data_reset_forgets_github_cli` says only Reset everything does it.
- The host performs. `run_profile_data_reset`, in the startup's lease-holding
  step and before any store opens, removes Reset everything's files and then,
  when `remembered-connections.sqlite3` exists and holds a CLI row, saves the
  core result through the remembered-connection store. The outcome records
  `GithubCliDisconnected`, or `GithubCliRetained` when the save failed.
- At the next start there is no CLI row to restore. Because the store has a
  recorded decision, the absent row also suppresses an older device grant for
  that server, exactly as Forget in Accounts & hosts does.
- Qt presents. The Reset everything confirmation now says what it does:
  lead "This profile starts again as on first launch and stops using GitHub
  CLI. It doesn’t sign you out of GitHub CLI itself, and nothing on GitHub
  changes."; Removes adds "This profile’s GitHub CLI connection" and names
  servers, filters, notifications and agent setup under Settings; Keeps names
  "GitHub CLI’s own sign-in, which other tools use" and "Sign-ins made in
  this app (sign out in Accounts & hosts)". The notice after reopening says
  the profile no longer uses GitHub CLI and that gh is still signed in, or
  that the connection could not be forgotten. The Troubleshooting lead and
  row no longer promise every reset keeps you signed in.

## Audit: what Reset everything leaves behind

| Item | Where | After this card |
|---|---|---|
| GitHub CLI choice and account | `remembered-connections` CLI rows | Forgotten (this card) |
| CLI consent for the run | in memory, per runtime | Gone: the profile reopens on a new runtime |
| gh avatar helper ([[BPR-1846]]) | per-launch private `GH_CONFIG_DIR`, in-memory hold-off; pictures in `markdown-images` | Nothing persistent to keep; the cache store is removed |
| Notification state ([[BPR-1850]]) | `pull-request-activity` (settings) | Removed (already) |
| GHES and other saved servers, saved repositories, observer rules, Home queue | `review-queue` (saved sources) | Removed (already) |
| CLI-discovered server profiles | in-memory profile catalog | Rebuilt only on Use GitHub CLI |
| Recently viewed and the Home Active review ([[BPR-1843]]) | `window-restoration` (settings) | Removed (already) |
| Filters ([[BPR-1844]]) | `view-filters` (settings) | Removed (already) |
| Agent setup | `agent-setup` (settings) | Removed (already) |
| Caches | `repositories/`, `incoming/`, `locks/`, `quarantine/`, `mcp-snapshot/`, `epoch-catalog`, `markdown-images`, `review-overviews` | Removed (already) |
| Activity, review data, Insights | `review-ledger`, `review-tours`, `review-cycles`, `publication-git-v1/`, `review-mutation-recovery`, `insights` | Removed (already) |
| Sign-in made in the app (native device grant) | vault item, `device-bindings`, its `remembered-connections` row | **Kept, by design** ([[BPR-1602]]:credentials: no reset calls the vault). Said in Keeps. Open question below |
| gh's own sign-in | gh's configuration and keychain | Never touched, by design |
| Diagnostic logs | `logs/` | Kept; they have their own Clear |
| Agent workspaces | `agent-workspace-ownership-v1/` | Kept, by design |
| Git isolation scaffolding | `templates/`, `hooks/` | Kept; rebuilt by the app, holds no choice |
| Shared registry and updater state | `profile-registry`, `release-update`, `update-draft-recovery`, `profiles/`, `updates/` | Kept: shared by every profile; purge excludes them too; not an uninstall |
| Other profiles, exported files, the person's Git checkouts | outside this profile's root | Kept |

## Open

- **Native sign-in.** Reset everything still keeps a sign-in made in the app
  (the device grant in the vault), so a person who signed in that way reopens
  signed in, not at Sign in to GitHub. Removing it means calling the vault from
  a reset, which [[BPR-1602]] ruled out and the owner accepted. The dialog now
  says it is kept and where to sign it out. Whether Reset everything should
  also sign the app's own sign-in out is the owner's decision.
- A GitHub CLI connection made again after the reset gets a fresh host
  identity for its server, as after Forget; that is the existing Forget
  behaviour, not new here.

## Acceptance

- **BPR-1858:policy** Core forgets every CLI row (enabled or signed out) and
  its server's retired identities in one revision, keeps native rows and the
  high-water marks, refuses stale work, and only Reset everything asks for it.
- **BPR-1858:composed** On the real composition: connect through gh, quit,
  start with Reset everything, and the app is not connected, Home is the
  first-run Sign in to GitHub, no CLI choice is remembered, gh was asked
  nothing and no consent was given; the next launch does not reconnect; Use
  GitHub CLI asks read consent again. Negative control: without the forget
  the reopened app is connected.
- **BPR-1858:wording** The confirmation, notice and Troubleshooting lines say
  the reset disconnects GitHub CLI and does not sign gh out.
- **BPR-1858:gates** Focused tests, clang-format 22, documentation gate.
- **BPR-1858:owner** The owner accepts the reworded confirmation.

## Verification (candidate `12c2d137a`, on main `33e1e088a`)

- Policy: `core_remembered_connection_policy` (`reset_forgets_github_cli`) and
  `core_profile_data_reset`.
- Composed: `application_remembered_connection_public_path`
  (`reset_everything_disconnects_github_cli`) connects through the fixture's
  gh, starts again with Reset everything and finds the app at the first-run
  sign-in with no CLI choice, no consent and no gh call; the next launch stays
  signed out; Use GitHub CLI asks read consent again. Two negative controls
  (forget skipped) fail, the second with "after Reset everything the app is
  not connected", which is the owner's report. The course stops at the
  consent question on the reconnect because Forget mints a fresh host
  identity that the single-host fixture does not serve.
- `application_profile_recovery_public_path` now compares remembered rows and
  high-water marks from a copy of the store and its WAL instead of the main
  file's bytes, which a WAL write never changed and the reset's read may
  checkpoint.
- `application_profile_data_reset_public_path`, `sqlite_profile_data_reset_files`,
  `qt_profile_data_reset_views`, `application_composition_contract`,
  `application_review_round_trip_composition_contract` (resealed),
  `architecture_boundary_contract`, `coding_format_check` and the
  documentation gate pass. `architecture_boundaries` fails on main already
  (BPR-1850's `QToolTip::showText`), untouched here.

## Owner review (asked)

Frozen build `/Users/jaeger/BPR-attended/BPR-1858-reset-v001.vw6i9430` from
`12c2d137a` (executable sha256 `e7757e4302ae…`), launched with
`python3 /Users/jaeger/BPR-attended/BPR-1858-reset-v001.vw6i9430/launch.py`;
its `--check` passed at freeze. One Accept or Reject question, candidate `BPR-1858-reset-wording-v001`,
revision `frozen-12c2d137a-v001`: in Settings › Storage & privacy ›
Troubleshooting, open **Reset everything…** — does its confirmation say
accurately what it does, that it disconnects this profile from GitHub CLI
without signing you out of GitHub CLI itself? On Accept: record it, move the
card to `60-done`, remove the frozen root with
`scripts/freeze_attended_build.py remove`.

Second question, candidate `BPR-1858-native-sign-in-kept-v001`, revision
`12c2d137a`, no build: keep that Reset everything keeps a sign-in made in the
app itself (browser device sign-in), signed out in Accounts & hosts; Reject
with a note if Reset everything should sign that out too.

Session `bbad2aafbd0e0e5c7487e5df46e870fe` (fresh, short), state
`/Users/jaeger/.codex/attended-review/bpr1858-v001-Bi416D/state`, opened
2026-11-04 with both candidates ready and protected at cursor 6, hosted over
Tailscale Serve. Status: awaiting the owner.
