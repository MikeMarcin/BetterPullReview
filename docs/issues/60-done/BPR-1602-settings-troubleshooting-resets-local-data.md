---
schema: "implement-rfc.card.v1"
id: "BPR-1602"
title: "Settings › Storage & privacy › Troubleshooting resets this profile's local data"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"owner-request","origin":"Owner request relayed by the coordinator, 2026-10-18","governing_rfc":"RFC-BPR-0014","related_rfcs":["RFC-BPR-0048","RFC-BPR-0084"],"related_cards":["BPR-0953","BPR-0954","BPR-0955","BPR-0956","BPR-1217","BPR-1555","BPR-1597"],"reservation":"docs/issues/_reservations/BPR-1602--BPR-1602--48cdf9547216.json","acceptance":["BPR-1602:design","BPR-1602:scopes","BPR-1602:credentials","BPR-1602:lifecycle","BPR-1602:recovery","BPR-1602:copy","BPR-1602:gates","BPR-1602:owner"]}
acceptance: [{"criterion":"BPR-1602:design","outcome":"pass","revision":"234bdee3e","evidence":{"kind":"observation","detail":"Owner selected A (four rows) in session ea1ed383, decision 47b87fa1, with the note asking for less wording on the page; coordinator relayed the Reset settings scope."}},{"criterion":"BPR-1602:scopes","outcome":"pass","revision":"234bdee3e","evidence":{"kind":"command","detail":"sqlite_profile_data_reset_files and core_profile_data_reset pass: each scope removes exactly its files, every other file byte-identical; negative control (review-queue misfiled as settings) fails."}},{"criterion":"BPR-1602:credentials","outcome":"pass","revision":"234bdee3e","evidence":{"kind":"command","detail":"Counting credential port receives no call for any reset; application_profile_data_reset_public_path keeps remembered-connections, device-bindings and profile-registry as the same files."}},{"criterion":"BPR-1602:lifecycle","outcome":"pass","revision":"234bdee3e","evidence":{"kind":"command","detail":"application_profile_data_reset_public_path: Settings, Clear caches and Review data resets close, remove exactly their stores, keep logs and reopen with a notice; unacknowledged reset does nothing; negative control (startup reset skipped) fails. Interrupted reset finished by recovery; refusals remove nothing."}},{"criterion":"BPR-1602:recovery","outcome":"pass","revision":"234bdee3e","evidence":{"kind":"command","detail":"application_profile_recovery_public_path on the shipped entry point and the frozen executable: healthy profile offered nothing; damaged epoch-catalog offered the dialog; Quit removes nothing; Clear caches opens the PR; --reset-profile-data with Reset everything works; copy without scope refused."}},{"criterion":"BPR-1602:copy","outcome":"pass","revision":"234bdee3e","evidence":{"kind":"command","detail":"Adapter course copies exactly the removed review stores into a new private folder and refuses an existing folder, removing nothing."}},{"criterion":"BPR-1602:gates","outcome":"pass","revision":"744c28c06","evidence":{"kind":"observation","detail":"21 focused rows pass on merged main cd80d1ab3 incl. architecture boundaries and both composition contracts (rerun at landing); clang-format 22 on 20 touched files; clang-tidy clean on new sources; documentation gate passes."}},{"criterion":"BPR-1602:owner","outcome":"pass","revision":"234bdee3e","evidence":{"kind":"observation","detail":"Human Accept a492cfed-db6b-4188-8946-697156c28ecf for frozen-234bdee3e-v001 in session 534aa242, consumed through event 9."}}]
---
# BPR-1602: Settings › Storage & privacy › Troubleshooting resets this profile's local data

## Owner request

2026-10-18, through the coordinator: "We need a button in Settings to clear
our data, just in case something happens and the db gets into a bad state; I
want users to have an easier path to self-serve and unblock themselves."

The owner proposed four options (clear caches; reset review state; reset
settings; reset everything), a strong confirmation for the destructive ones
with an offer to keep drafts and notes first, a safe open-database lifecycle
(core decides, host performs, Qt presents; never a half-deleted database), and
a recovery path when the profile cannot open at all (a startup dialog or a
`--reset-profile-data` flag named in the error).

## Boundaries

- Reuse the RFC-BPR-0014 maintenance host, its exact native inventory,
  `.bpr-maintenance-v1` journal and startup recovery
  ([release maintenance](../../architecture/release-maintenance.md)). No
  second deletion path and no recursive delete.
- Never read or delete credentials: no vault call, and `device-bindings` and
  `remembered-connections` stay. Signing out stays in Accounts & hosts; the
  existing two-step profile purge (which does remove credential bindings)
  stays as it is under Profile maintenance.
- Never touch other profiles, the shared registry or updater state, agent
  workspaces, diagnostic logs (they have their own Clear), exported files, or
  any Git checkout outside the app's own caches.
- Whole files only: a store is removed with its sidecars or not at all, so a
  damaged database is never opened to be reset.

## Owner decision (design)

Session `ea1ed38316a18d2afe3b1fe067c699d7`, decision
`47b87fa1-986d-4927-ac58-07d11d81c8af` (2026-10-18): **A, four rows**, with the
note "A is fantastic just a little less verbosity on the settings page. the
confirmation dialog looks great." Through the coordinator the same day: trim
each row to a short label and one short line; keep the confirmation exactly as
designed; Reset settings **keeps** saved repositories, servers and observer
rules and restores only the queue display choices inside `review-queue`;
Reset everything is the one that clears them, said in its Removes list.

## Design as built

- Settings › Storage & privacy ends with **Troubleshooting**: one lead line
  ("Each reset keeps you signed in and reopens this profile's windows.") and
  four rows, each a name, one short line and a button.
- Each button asks the accepted confirmation: title naming the profile, lead,
  Removes and Keeps side by side, Cancel the default and Escape. Reset review
  data and Reset everything add "Save a copy of the removed data in Downloads
  first" (on) and the acknowledgement, which enables the red button.
- A confirmed reset closes the profile's windows, runs at the profile's next
  start under its lease after maintenance recovery and before any store opens,
  and reopens the profile on Home with a status-bar notice.
- A profile that cannot open offers Clear caches and try again (default),
  Reset everything… and Quit, and starts again in the same process.
  `--reset-profile-data` opens that dialog first;
  `--reset-profile-data-scope` and `--reset-profile-data-copy` run a reset
  directly.

## Acceptance

- **BPR-1602:design** The owner picks the design to build.
- **BPR-1602:scopes** On a populated profile root, each reset removes exactly
  its files and sidecars and leaves every other file byte-identical; Reset
  everything removes their union (negative control: a file outside the scope
  is unchanged).
- **BPR-1602:credentials** No reset calls the credential vault; device
  bindings and remembered connections are unchanged; the profile is still
  signed in after reopening.
- **BPR-1602:lifecycle** A reset retires the profile's windows and closes its
  stores before inventory, journals before deleting, and reopens the profile
  on success; a reset interrupted after the journal is finished by startup
  recovery; an unknown entry refuses and deletes nothing.
- **BPR-1602:recovery** A profile whose store is damaged opens to the recovery
  dialog instead of exiting silently; each recovery action leaves the profile
  open; `--reset-profile-data` opens the same dialog; a healthy profile shows
  no dialog.
- **BPR-1602:copy** The optional copy holds exactly the removed store files in
  a new folder and never overwrites one.
- **BPR-1602:gates** Focused tests, clang-format 22, the documentation gate and
  the composition contracts.
- **BPR-1602:owner** The owner accepts the frozen build in Light and Dark.

## Design round v001 (2026-10-18)

Designed with the `frontend-design` skill in the existing Settings row
grammar; the one new element is the confirmation's Removes / Keeps ledger.
Mockups (Light and Dark, 14 images) ask one question, A or B, in a fresh
attended session `ea1ed38316a18d2afe3b1fe067c699d7` (state
`~/.mask/attended-review-sessions/bpr-1602-design-v001-20261018`), candidate
`BPR-1602-reset-design-v001`, revision `mockups-2026-10-18-v001`.

Proposed scopes, by whole store file with its sidecars:

| Scope | Files |
|---|---|
| Caches | `repositories/`, `incoming/`, `locks/`, `epoch-catalog.sqlite3`, `markdown-images.sqlite3`, `review-overviews.sqlite3`, `mcp-snapshot/` |
| Settings | `appearance-settings`, `workbench-settings`, `diagnostic-preferences`, `window-restoration`; Home's queue display choices restored inside `review-queue` |
| Review data | `review-ledger`, `review-tours`, `review-cycles` with `publication-git-v1/`, `review-mutation-recovery` |
| Everything | all three and `review-queue` (saved repositories and servers, observer rules, Home's queue) |
| Never | `device-bindings`, `remembered-connections`, the vault, `logs/`, `profile-registry`, `release-update`, `update-draft-recovery`, `profiles/`, `updates/`, agent workspaces, exported files |

Review data and Everything need an explicit acknowledgement and offer a copy
of the removed store files in Downloads, on by default. The startup recovery
dialog offers Clear caches and try again, Reset everything… or Quit, and
`--reset-profile-data` opens it before the profile loads.

### Damaged-store probe (main `e9b1cee35`, tests-on dev build)

A fixture profile (`--github-fixture github-com --epoch-fixture-root`) opened
its pull request, then each store's header was overwritten in a copy of that
root (its WAL and SHM removed) and the app started again offscreen:

- Opened normally, the damage silent at start: appearance-settings,
  workbench-settings, diagnostic-preferences, review-queue, review-ledger,
  review-tours, review-cycles, review-overviews, window-restoration,
  review-mutation-recovery, remembered-connections.
- Exited 1 with no window and no message: `epoch-catalog`
  (`STARTUP_FAILURE_SOURCE:profile-runtime-construction`) and the shared
  `profile-registry` (`STARTUP_PROFILE_REFUSAL:invalid_registry`).

So the Settings resets are the main path, and the startup dialog's Clear
caches covers the observed exit; the shared registry is outside these resets.

## Verification (candidate `234bdee3e`, main `cd80d1ab3` merged)

- **scopes:** `sqlite_profile_data_reset_files` resets a populated private
  root for each scope and compares every file byte for byte: exactly the
  scope's files go, the rest (sign-in stores, logs, shared stores, agent and
  Git isolation, sibling profiles, unknown files) are identical. Negative
  control: classifying `review-queue` as settings fails the course.
  `core_profile_data_reset` covers the policy table, admission and names.
- **credentials:** the same course gives the adapter a credential port that
  counts calls; no reset calls it. `application_profile_data_reset_public_path`
  keeps `remembered-connections`, `device-bindings` and `profile-registry` as
  the same files through every reset.
- **lifecycle:** `application_profile_data_reset_public_path` confirms Reset
  settings, Clear caches and Reset review data in the real Settings; each
  closes the windows, removes its stores, keeps every other store and the
  logs, and reopens on a new runtime with its notice; an unacknowledged Reset
  everything does nothing. Negative control: skipping the startup reset step
  fails it. The adapter course stops a deletion part-way (an unwritable pack
  folder): the journal stays, another reset refuses, and recovery finishes
  exactly the scope; a folder where a store belongs and a link inside a cache
  folder each refuse and remove nothing.
- **recovery:** `application_profile_recovery_public_path` runs the shipped
  entry point: a healthy profile is offered nothing; a damaged
  `epoch-catalog` fails start-up and is offered the dialog; Quit removes
  nothing; Clear caches and try again opens the pull request; with
  `--reset-profile-data`, Reset everything replaces review data and keeps
  remembered connections; `--reset-profile-data-copy` alone is refused. The
  same course passes against the frozen executable.
- **copy:** the adapter course copies exactly the removed review stores into
  a new private folder and refuses an existing one, removing nothing.
- **gates:** 21 focused rows pass on the merged tree, including both
  architecture boundary tests, `application_composition_contract`,
  `application_review_round_trip_composition_contract` (resealed),
  `qt_profile_data_reset_views`, `qt_settings_window`,
  `qt_workbench_window_scale_100`, the release-maintenance, diagnostic,
  profile management, empty-desktop and multi-window public paths.
  clang-format 22 passes on all 20 touched C++ files; clang-tidy (LLVM 22,
  warnings as errors) is clean on the new sources, the adapter and
  `profile_desktop.cpp`; the documentation gate passes.

## Final review

Frozen build `/Users/jaeger/bpr-1602-reset-v001.6t1zlk_c` from `234bdee3e`
(executable sha256 `f3032408eff8…`), throwaway profile
`/Users/jaeger/bpr-1602-reset-v001-profile.d`. Session
`534aa242d644f5ab110f86fa14257fd6` (state
`~/.mask/attended-review-sessions/bpr-1602-final-v001-20261018`), candidate
`BPR-1602-reset-built-v001`, revision `frozen-234bdee3e-v001`, one Accept or
Reject. On Accept: finish the card, merge main, land after BPR-1597 (already on
main), remove the frozen root and profile folder.

## Owner verdict and landing (2026-10-18)

Owner **Accept**, no note, decision `a492cfed-db6b-4188-8946-697156c28ecf`
for `BPR-1602-reset-built-v001` (revision `frozen-234bdee3e-v001`) in session
`534aa242…`; handback consumed through event 9. Main had not moved since the
merge at `cd80d1ab3`, so no `src/app` change needed a reseal; both composition
contracts pass again on the landing tree (`744c28c06`).

