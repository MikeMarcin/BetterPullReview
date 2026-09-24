---
schema: "implement-rfc.card.v1"
id: "BPR-1020"
title: "The Windows isolation manifest gate carries eleven unpinned uncontained rows"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win under BPR-0653","found_at":"53d229d7","run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1020--BPR-1022--f3b33a48cfc4.json","related_cards":["BPR-0812","BPR-0833","BPR-0872","BPR-0873"]}
acceptance: [{"criterion":"AC1 test_network_isolation_manifest on the Windows station reports no finding","outcome":"pass","revision":"41bb3229","evidence":{"kind":"command","detail":"bpr-main-win (JaegAW, Windows 11 10.0.26200, MSVC, Qt 6.11.2, build/dev reconfigured at the change): ctest --preset dev -R ^test_network_isolation_manifest$ prints TEST_NETWORK_ISOLATION_OK:configured=412:registrations=386:backend=windows-appcontainer with zero findings. Before, at 38cc26d7, it printed 33 TEST_NETWORK_ISOLATION_FAILED findings, three for each of the eleven rows (build/logs/bpr-1020-main-win/manifest-1.log, verify-1.log).","path":"docs/issues/60-done/BPR-1020-the-windows-isolation-manifest-gate-carries-eleven-unpinned-uncontained-rows.md#verification-at-41bb3229"}},{"criterion":"AC2 Each of the eleven rows is registered contained where it passes contained, or is pinned with its measured first contained failure; no row is pinned without a measured cause","outcome":"pass","revision":"41bb3229","evidence":{"kind":"observation","detail":"Each row was registered contained by a reverted trial hook in bpr_add_network_isolated_test and run inside the AppContainer at 38cc26d7. release_package_audit_contract and release_security_audit_contract pass contained with the same 6 and 9 tests and are now registered contained. The other nine stop at a measured first failure: Git 'unable to get current working directory: Permission denied' (package_runtime_contract, modified_endpoint_fixture_contract; probed directly), CPython realpath [WinError 5] (release_source_assets_contract), 'canonical: Access is denied.' from a reverted terminate hook (sqlite_release_maintenance_files), and the first owner-private root step of five composed courses. All nine are pinned dos-volume-name with those measurements in the comment. Four of them also fail uncontained at late assertions; those are separate defects, recorded on the card with new IDs requested.","path":"docs/issues/60-done/BPR-1020-the-windows-isolation-manifest-gate-carries-eleven-unpinned-uncontained-rows.md#windows-station-2026-09-23-bpr-main-win-two-contained-nine-pinned"}},{"criterion":"AC3 verify_test_network_isolation_tests.py (test_network_isolation_structure_contract) still passes","outcome":"pass","revision":"41bb3229","evidence":{"kind":"command","detail":"ctest --preset dev -R ^test_network_isolation_structure_contract$ on the Windows station: Ran 64 tests, OK (skipped=10), passed in 2.40 s (build/logs/bpr-1020-main-win/verify-1.log).","path":"docs/issues/60-done/BPR-1020-the-windows-isolation-manifest-gate-carries-eleven-unpinned-uncontained-rows.md#verification-at-41bb3229"}}]
---

# BPR-1020: The Windows isolation manifest gate carries eleven unpinned uncontained rows

## Defect

On the Windows station, `test_network_isolation_manifest` fails. Eleven rows are
registered `WINDOWS_UNCONTAINED` but are missing from the pinned Windows set in
`scripts/verify_test_network_isolation.py` (`WINDOWS_UNCONTAINED_TESTS`). Each
row produces three findings: it "does not start with
[...better_pull_review_network_sandbox.exe]", it "lacks the
bpr_network_isolated label", and it "uses bpr_windows_uncontained but is not in
the pinned Windows set".

[[BPR-0653]] found them at `53d229d7`. The list at `38cc26d7` is the same, 33
findings in all:

| Row | Registered in | Added by |
|---|---|---|
| `package_runtime_contract` | `tests/CMakeLists.txt` | `65dd7dd4` (RFC-BPR-0014 release package paths) |
| `release_source_assets_contract` | `tests/CMakeLists.txt` | `1c429749` (separate corresponding-source asset) |
| `release_package_audit_contract` | `tests/CMakeLists.txt` | `3da341d1` ([[BPR-0957]]) |
| `release_security_audit_contract` | `tests/CMakeLists.txt` | `9409ff61` (release hardening) |
| `sqlite_release_maintenance_files` | `tests/sqlite_contract/CMakeLists.txt` | `19a3dfd7` (release maintenance host) |
| `modified_endpoint_fixture_contract` | `tests/git_contract/CMakeLists.txt` | `a9492592` |
| `application_logging` | `tests/end_to_end/CMakeLists.txt` | `19a3dfd7` (Windows registration, was UNIX only) |
| `application_selected_file_safety_public_path` | `tests/end_to_end/CMakeLists.txt` | `a9492592` |
| `application_attended_recent_replay` | `tests/end_to_end/CMakeLists.txt` | `e73831f5` ([[BPR-0932]]) |
| `application_release_maintenance_public_path` | `tests/end_to_end/CMakeLists.txt` | `a34fc07f` |
| `application_release_update_public_path` | `tests/end_to_end/CMakeLists.txt` | `5ed24a8c` ([[BPR-0947]]) |

All eleven were added off Windows. The pinned set is checked only on the
`windows-appcontainer` backend, so the gate cannot see them from macOS or
Linux. This is the same drift [[BPR-0812]] cleared.

## Scope

- For each row, either show that it runs contained and register it contained,
  or measure its first contained failure on the Windows station and pin it
  with that cause. Do not pin a row without a measured cause.
- A row that also fails uncontained is a separate defect. It is recorded here
  and carded, not fixed here.

## Acceptance

- AC1 `test_network_isolation_manifest` on the Windows station reports no
  finding.
- AC2 Each of the eleven rows is registered contained where it passes
  contained, or is pinned with its measured first contained failure; no row is
  pinned without a measured cause.
- AC3 `verify_test_network_isolation_tests.py`
  (`test_network_isolation_structure_contract`) still passes.

## Windows station, 2026-09-23 (bpr-main-win): two contained, nine pinned

Run `bpr-main-win-20260923`. Windows 11 Home 10.0.26200, VS 18 MSVC, Qt 6.11.2,
`build/dev` built at `38cc26d7`. The logs are under
`build/logs/bpr-1020-main-win/` on the station and are not committed.

**Method.** This follows [[BPR-0812]]. A temporary hook in
`bpr_add_network_isolated_test` registered the eleven rows contained, each row
was run inside the AppContainer, and its first failure was read. The hook was
then reverted and the tree reconfigured. Where a row printed nothing, or Git's
error was swallowed, the call was probed directly through
`better_pull_review_network_sandbox.exe`:

- a reverted terminate hook in `release_maintenance_files_tests.cpp`;
- `git init --bare` inside the container with its stderr kept.

Every row was also run uncontained, as registered. None of the eleven shows a
window.

| Row | Uncontained, as registered | Contained, measured | Resolution |
|---|---|---|---|
| `release_package_audit_contract` | Passes, 6 tests | Passes, the same 6 tests | Registered contained |
| `release_security_audit_contract` | Passes, 9 tests | Passes, the same 9 tests | Registered contained |
| `package_runtime_contract` | Passes | `git -C <checkout> rev-parse HEAD` exits 128, "fatal: Unable to read current working directory: Permission denied". Next, `prepare_source_asset` cannot write into its `tempfile` directory (Errno 13) | Pinned `dos-volume-name` |
| `release_source_assets_contract` | Passes | `packaging/package.py`: `args.build.resolve(strict=True)` raises `[WinError 5] Access is denied` in CPython's `realpath` | Pinned `dos-volume-name` |
| `modified_endpoint_fixture_contract` | Passes | `git init --bare` exits 128, "fatal: unable to get current working directory: Permission denied" (probed) | Pinned `dos-volume-name`, as `profile_window_restoration_fixture_contract` |
| `sqlite_release_maintenance_files` | **Fails**, "FAILED: public host exports exactly preview bytes" | Exit 3 with no output. The hook printed `canonical: Access is denied.` for the scratch root, before any course step | Pinned `dos-volume-name`, as `sqlite_profile_runtime_lease` |
| `application_logging` | Passes, 7.82 s | "native private root before composed startup", the first step | Pinned `dos-volume-name`, as the [[BPR-0833]] rows |
| `application_selected_file_safety_public_path` | Passes, 22.31 s | "FAILED: private folder course root", the first step | Pinned `dos-volume-name` |
| `application_attended_recent_replay` | **Fails**, "FAILED: UIforETW retained comparison ready", after `EPOCH_EFFECT_FAILED:revision-prepare:0` and `PROFILE_QUIT_CLEANUP_SOURCE:agent-workspace-recovery:…:repair-required` | "FAILED: wire fixture root is owner-private", the first step | Pinned `dos-volume-name` |
| `application_release_maintenance_public_path` | **Fails**, "FAILED: hostile legacy diagnostics actually reach the selected native log" | "FAILED: private application root", the first step | Pinned `dos-volume-name` |
| `application_release_update_public_path` | **Fails**, "FAILED: all update shutdown flushes succeed", after `PROFILE_QUIT_CLEANUP_SOURCE:agent-workspace-recovery:…:repair-required` | "FAILED: private fixture state root", the first step | Pinned `dos-volume-name` |

**Why four rows that fail uncontained are still pinned.** [[BPR-0812]] left
its two epoch rows unpinned because they failed uncontained at startup,
before any step, so their contained cause could not be told apart. These four
are different. Each gets well into its course uncontained and fails at a late
assertion. Contained, each stops at its first step, on a refusal already pinned
elsewhere. The pin records only why the row cannot run inside the AppContainer.
It does not claim the row passes.

**Found, not fixed here: four rows fail on Windows even uncontained.** None is
in the pinned set's scope, and IDs have been requested from the coordinator for
them:

- `sqlite_release_maintenance_files` fails at "public host exports exactly
  preview bytes", and `application_release_maintenance_public_path` fails at
  "hostile legacy diagnostics actually reach the selected native log". Both are
  in the release maintenance export and diagnostics path.
- `application_attended_recent_replay` and
  `application_release_update_public_path` report
  `agent-workspace-recovery:<data-root>\agent-workspace-ownership-v1:repair-required`
  at quit. That is [[BPR-0875]]'s mechanism, in C++ courses whose
  `QTemporaryDir` data root is not under `bpr_private_data_root`. The replay
  course also fails its epoch preparation.

Each failing run also left its temporary root behind in `%TEMP%`. Those, and
the contained trial's temporaries, were removed by hand.

**Stale neighbour.** [[BPR-0891]] (Ready) says `application_logging` is
registered on UNIX only. `19a3dfd7` removed that guard, and the row passes on
Windows uncontained. That card's premise no longer holds; it is reported to the
coordinator and not changed here.

### Verification at `41bb3229`

The change was authored and measured as `9080406d` and rebased onto
`0b57fe2e` without conflict. The rebase brought only documentation, so the
sources, tests and scripts are the ones measured.

- `test_network_isolation_manifest`:
  `TEST_NETWORK_ISOLATION_OK:configured=412:registrations=386:backend=windows-appcontainer`,
  zero findings (before: 33 findings, 3 for each of the eleven rows).
- `test_network_isolation_structure_contract`
  (`verify_test_network_isolation_tests.py`): 64 tests, OK (10 skipped).
- `release_package_audit_contract` and `release_security_audit_contract` pass
  under their real registration, now behind
  `better_pull_review_network_sandbox.exe` (6 and 9 tests).
- The nine pinned rows keep their registration unchanged. Run as registered at
  `38cc26d7`, five pass and four fail uncontained, as above.
- `git diff --check` is clean.

## Post-close owner decision, 2026-09-24: revert the four pins (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-1020-uncontained-pins-v001`, revision
`41bb3229`, fingerprint `b070e2e1…`, decision
`dcc2b167-d51d-4b47-9ccf-68b33f616640`, sequence 581, 2026-09-24T19:12:47Z,
`actor: human`: **select**, verbatim:

> Revert the four pins and leave those rows unpinned, as BPR-0812 did

The note is empty. The owner overrode the reasoning in "Why four rows that
fail uncontained are still pinned". `sqlite_release_maintenance_files`,
`application_attended_recent_replay`,
`application_release_maintenance_public_path` and
`application_release_update_public_path` come out of the pinned set. This card
stays Done: its AC1 was true at `41bb3229` and is not rewritten. The revert is
new work, [[BPR-1108]] (Ready, Windows). Once it lands, the Windows gate carries
those rows' findings until each is contained or recorded there.
