---
schema: "implement-rfc.card.v1"
id: "BPR-1108"
title: "Windows: revert BPR-1020's four pins for rows that also failed uncontained, and leave them unpinned as BPR-0812 did"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin":"owner verdict on attended candidate BPR-1020-uncontained-pins-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision dcc2b167-d51d-4b47-9ccf-68b33f616640, 2026-09-24","related_cards":["BPR-1020","BPR-0812","BPR-1030","BPR-1031","BPR-0653"],"pinned_at":"41bb3229","reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json"}
acceptance: [{"criterion":"AC1 The four pins are gone from WINDOWS_UNCONTAINED_TESTS, and nothing else in the pinned set changes except rows moved to contained under AC2","outcome":"pass","revision":"94be7dc7","evidence":{"kind":"command","detail":"git diff of scripts/verify_test_network_isolation.py removes exactly sqlite_release_maintenance_files, application_attended_recent_replay, application_release_maintenance_public_path and application_release_update_public_path (170 to 166 names) with their BPR-1020 comment lines, adds a BPR-1108 note, and changes nothing else; no row moved to contained because none passes contained; no registration changed.","path":"docs/issues/60-done/BPR-1108-windows-revert-bpr-1020s-four-uncontained-pins.md#the-change"}},{"criterion":"AC2 Each of the four rows has a recorded measurement on the Windows station, uncontained and contained; a row that passes contained is registered contained, and the others stay unpinned with the cause recorded","outcome":"pass","revision":"94be7dc7","evidence":{"kind":"command","detail":"bpr-main-win build/dev: uncontained as registered all four pass 2 of 2 (release-maintenance-files-tests:pass; ATTENDED_RECENT_REPLAY_OK; release-maintenance-public-path:pass; release-update-public-path:pass). Contained through a reverted trial hook in bpr_add_network_isolated_test (ctest -N -V showed the sandbox launcher and bpr_network_isolated): sqlite_release_maintenance_files exit 3 with 'canonical: Access is denied.' printed by a reverted terminate hook for its AC\\Temp scratch root; the three application rows FAILED at their first owner-private root step ('wire fixture root is owner-private', 'private application root', 'private fixture state root'). None passes contained, so all four stay unpinned with the cause recorded in the card's table.","path":"docs/issues/60-done/BPR-1108-windows-revert-bpr-1020s-four-uncontained-pins.md#each-row-measured"}},{"criterion":"AC3 test_network_isolation_manifest on Windows reports exactly the findings of the rows left unpinned, three each, and no others; test_network_isolation_structure_contract passes; the same gate on macOS and Linux is unchanged","outcome":"pass","revision":"94be7dc7","evidence":{"kind":"command","detail":"After the trial was reverted and build/dev reconfigured: ctest --preset dev -V -R ^(test_network_isolation_manifest|test_network_isolation_structure_contract)$ prints exactly 12 TEST_NETWORK_ISOLATION_FAILED lines, 3 for each of the four rows and none for any other row (before: TEST_NETWORK_ISOLATION_OK:configured=419:registrations=396:backend=windows-appcontainer); the structure contract ran 65 tests, OK (skipped=10). macOS and Linux unchanged by construction: WINDOWS_UNCONTAINED_TESTS is consulted only on the windows-appcontainer backend, and the backend-independent source-count bound holds (149 against 166); not run from this station.","path":"docs/issues/60-done/BPR-1108-windows-revert-bpr-1020s-four-uncontained-pins.md#the-gate-before-and-after"}}]
---

# BPR-1108: Windows: revert BPR-1020's four pins for rows that also failed uncontained

## Why

[[BPR-1020]] (`41bb3229`) cleared the Windows manifest gate's 33 findings. It
contained two release audit rows and pinned nine rows as `dos-volume-name` in
`WINDOWS_UNCONTAINED_TESTS` (`scripts/verify_test_network_isolation.py`).
Four of those nine also failed when run uncontained at `38cc26d7`, each late in
its course:

- `sqlite_release_maintenance_files`
- `application_attended_recent_replay`
- `application_release_maintenance_public_path`
- `application_release_update_public_path`

[[BPR-0812]]'s precedent left such rows unpinned. Asked whether to keep the
four pins or revert them, the owner answered on 2026-09-24 in attended session
`cbd48b91000b00f1e72e1ad62c7c384c` (candidate
`BPR-1020-uncontained-pins-v001`, revision `41bb3229`, decision `dcc2b167`,
19:12:47Z), **select**, verbatim:

> Revert the four pins and leave those rows unpinned, as BPR-0812 did

The note is empty.

## Scope

- Remove the four entries from `WINDOWS_UNCONTAINED_TESTS`. Leave the other
  five pins and the two contained audit rows as BPR-1020 left them.
- Run `test_network_isolation_manifest` on the Windows station. The gate is
  then expected to report three findings for each of the four rows. They are
  this card's, in the way BPR-0812's two epoch rows were BPR-0812's until they
  were resolved, and not a regression.
- Since the pins were taken, the four rows' uncontained failures were worked as
  [[BPR-1030]] and [[BPR-1031]] (both Done). For each row, re-measure it
  uncontained and, by a reverted trial hook, contained:
  - if it now passes contained, register it contained, as BPR-1020 did for the
    two audit rows;
  - if it passes uncontained but not contained, record the contained cause and
    leave the row unpinned, as the owner chose;
  - if it fails uncontained, record why and leave it unpinned.
- Record each row's outcome here, with the gate's findings before and after.

## Acceptance

- AC1 The four pins are gone from `WINDOWS_UNCONTAINED_TESTS`, and nothing
  else in the pinned set changes except rows moved to contained under AC2.
- AC2 Each of the four rows has a recorded measurement on the Windows station,
  uncontained and contained. A row that passes contained is registered
  contained, and the others stay unpinned with the cause recorded.
- AC3 `test_network_isolation_manifest` on Windows reports exactly the findings
  of the rows left unpinned, three each, and no others;
  `test_network_isolation_structure_contract` passes; the same gate on macOS
  and Linux is unchanged.

## Boundaries

Test registration and the isolation script only. No product change, and no
weakening of the gate or of another row's containment.

## Windows station, 2026-09-24 (bpr-main-win): reverted; all four pass uncontained and stop contained at their first step

Run `bpr-main-win-20260923`. Setup:

- Windows 11 Home 10.0.26200, VS 18 MSVC, Qt 6.11.2;
- `build/dev`, rebuilt at `107a9386` (the claim, on main `657ca3bb`);
- every row offscreen, or without a window.

Logs are under `build/logs/bpr-1108-main-win/` on the station and are not
committed.

### The change

In `scripts/verify_test_network_isolation.py`, `WINDOWS_UNCONTAINED_TESTS`
loses:

- `sqlite_release_maintenance_files`
- `application_attended_recent_replay`
- `application_release_maintenance_public_path`
- `application_release_update_public_path`

Their BPR-1020 comment lines go too. A note in their place records the
owner's revert. Nothing else in the set changes:

- the other five BPR-1020 pins stay;
- the two contained audit rows stay contained;
- no registration changes.

The pinned set goes from 170 to 166 names. The source-count bound
(`WINDOWS_UNCONTAINED` appears 149 times) still holds on every backend.

### Each row, measured

- **Uncontained:** as registered, run twice with `ctest --preset dev`.
- **Contained:** through a reverted trial hook in
  `bpr_add_network_isolated_test` (`cmake/BprTestIsolation.cmake`). The hook
  cleared `WINDOWS_UNCONTAINED` for the four names. `ctest -N -V` confirmed
  the rows ran behind `better_pull_review_network_sandbox.exe`, labelled
  `bpr_network_isolated`.
- `sqlite_release_maintenance_files` printed nothing contained. For it, a
  reverted terminate hook in `release_maintenance_files_tests.cpp` printed
  the exception.
- Both hooks were reverted, and `build/dev` was reconfigured and rebuilt.
  `ctest -N -V` shows the four rows uncontained again.

| Row | Uncontained, as registered | Contained, trial hook | Outcome |
|---|---|---|---|
| `sqlite_release_maintenance_files` | **Passes**, 2 of 2 (0.67 s, 0.66 s): `release-maintenance-files-tests:pass` | Exit 3, no output. The terminate hook printed `canonical: Access is denied.` for the scratch root under the container's `AC\Temp`: `std::filesystem::canonical` in the course's `scratch` constructor, before any step. This is the refusal `sqlite_profile_runtime_lease` measured. | Unpinned; contained cause recorded |
| `application_attended_recent_replay` | **Passes**, 2 of 2 (26.17 s, 25.91 s): `ATTENDED_RECENT_REPLAY_OK` | `FAILED: wire fixture root is owner-private`, its first step (the BPR-0833 narrowing) | Unpinned; contained cause recorded |
| `application_release_maintenance_public_path` | **Passes**, 2 of 2 (14.03 s, 14.05 s): `release-maintenance-public-path:pass …` | `FAILED: private application root`, its first step | Unpinned; contained cause recorded |
| `application_release_update_public_path` | **Passes**, 2 of 2 (7.15 s, 7.09 s): `release-update-public-path:pass` | `FAILED: private fixture state root`, its first step | Unpinned; contained cause recorded |

- **Uncontained, the late failures are gone.** BPR-1020 measured all four
  failing late uncontained at `38cc26d7`. Now they pass, which is
  [[BPR-1030]]'s and [[BPR-1031]]'s work.
- **Contained, each stops where BPR-1020 measured**, on the owner-private
  root narrowing under `AC\Temp` that BPR-0833 found refused.
- **None passes contained, so none is registered contained.** All four stay
  unpinned, as the owner chose.

### The gate, before and after

- Before (`107a9386`): `TEST_NETWORK_ISOLATION_OK:configured=419:registrations=396:backend=windows-appcontainer`.
  There were no findings.
- After, with the trial reverted: `test_network_isolation_manifest` reports
  exactly 12 `TEST_NETWORK_ISOLATION_FAILED` findings, three for each of the
  four rows, and no other:
  - "uses bpr_windows_uncontained but is not in the pinned Windows set";
  - "does not start with […better_pull_review_network_sandbox.exe]";
  - "lacks the bpr_network_isolated label".
- These twelve are this card's outcome and are not a regression. Until a row
  runs contained, the Windows manifest row is red on them, as BPR-0812's
  epoch rows were.
- `test_network_isolation_structure_contract` passes: 65 tests, OK, 10
  skipped.
- **macOS and Linux are unchanged by construction**, not by a run from this
  station. `WINDOWS_UNCONTAINED_TESTS` is consulted only on the
  `windows-appcontainer` backend, and the one backend-independent use, the
  source-count upper bound, holds (149 against 166).

### Clean-up

- The uncontained runs left nothing in `%TEMP%`: listings before and after
  are identical.
- The two contained runs left eight empty roots under the container's
  `AC\Temp`, two per row. They were removed by exact name.

`git diff --check` is clean. The only committed change is the isolation
script. It was authored as `26c832bc`, and the push gate's rebase made it
`94be7dc7` on main, with an identical diff. Its tree is the one measured:
`107a9386` plus this diff.
