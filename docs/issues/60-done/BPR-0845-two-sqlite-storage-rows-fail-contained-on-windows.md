---
schema: "implement-rfc.card.v1"
id: "BPR-0845"
title: "Two SQLite storage rows fail contained on Windows"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-creation-descriptor new defect","found_at":"6c633c3dc34e4e0622338e193ebfe6b692ed5ffe","run":"run-20260916-stations","dispatch":"dsp-003"}
acceptance: [{"criterion":"AC1 both rows pass on the Windows station as registered, or are pinned with a measured reason, and the manifest reports no finding","outcome":"pass","revision":"b52fea2fbe976b759711b91928067c7e4a85e44b","evidence":{"kind":"command","detail":"bpr-win: both rows also failed uncontained (no Windows owner-private root); tests now narrow with make_storage_root_owner_private and pass uncontained; contained they stop at validate_ancestor_chain error 5 and are pinned dos-volume-name with that measurement; pass as registered; TEST_NETWORK_ISOLATION_OK:configured=357 with no finding."}},{"criterion":"AC2 they still pass on macOS","outcome":"pass","revision":"3bd658618abc15628cd634d52bbc5bbf5833cbb3","evidence":{"kind":"command","detail":"Both rows pass on macOS at 3bd65861 (build/r2-logs/dsp3-int10-ctest.log)."}}]
---

# BPR-0845: Two SQLite storage rows fail contained on Windows

## Defect

On the Windows station, two rows registered contained fail inside the
AppContainer:

- `sqlite_queue_enrollment_storage`: `FAILED: queue port creates its state directory`;
- `sqlite_workbench_settings_storage`: `FAILED: explicit defaults factory creates private storage`.

Both fail the same way with [[BPR-0833]]'s adapter change reverted, so the
failures predate it. Because the rows are registered contained, the manifest
gate does not report them.

Reported by `bpr-win` in `task-20260917-win-creation-descriptor`, not carded
there.

## Scope

- Measure each row's first failing call contained, and compare with an
  uncontained run.
- Fix the cause: likely the same inherited-descriptor or canonicalisation
  shape as BPR-0833 and [[BPR-0812]]. Otherwise pin the row with the measured
  reason.

## Acceptance

- Both rows pass on the Windows station as registered, or are pinned with a
  measured reason, and `test_network_isolation_manifest` reports no finding.
- They still pass on macOS.

## Windows station, 2026-09-17 (task-20260917-win-private-parent)

The base is `2699a47e1ccd8e9a820429fc3279899597120292`. Logs are under
`build/logs/task-20260917-win-private-parent/`.

**Measured.** Neither row failed only when contained: at the base, **both also
failed uncontained**, with the same lines:

- `sqlite_queue_enrollment_storage`: its `scratch_directory::make_private()`
  returns `true` on Windows without doing anything. The scratch root under the
  temporary directory keeps its inherited access-control list, and the store
  refuses it.
- `sqlite_workbench_settings_storage`: the explicit-defaults root is made with
  `create_directory` plus `permissions(owner_all)`. On Windows that only
  toggles the read-only attribute and leaves the inherited list, which the
  store refuses.

Pointing `TEMP` at an owner-private directory did not change either result.

**Fix** (tests only; behaviour on POSIX unchanged):

- `tests/sqlite_contract/queue_enrollment_storage_tests.cpp`: on Windows,
  `make_private()` calls `bpr::adapters::sqlite::make_storage_root_owner_private(Path)`.
- `tests/sqlite_contract/workbench_settings_storage_tests.cpp`: on Windows, the
  explicit-defaults root is narrowed with `make_storage_root_owner_private`.

Both then pass uncontained (`queue-enrollment-storage-tests:pass`,
`workbench-settings-storage-tests:pass`).

**Contained, with the fix.** Both still fail at the same lines. A reverted
diagnostic in `open_state_directory` printed `validate_ancestor_chain`
`failure=1 error=5` for both rows. Inside the AppContainer the store's walk of
the root's ancestors is denied: the retained-ancestor-chain step the manifest
documents under `dos-volume-name` for application roots, and the same refusal
[[BPR-0833]] measured for the automated profile.

**Registration.** Both rows are now `WINDOWS_UNCONTAINED` and pinned
`dos-volume-name` in `scripts/verify_test_network_isolation.py`, with the
measurement in the comment.

**Results after a full rebuild:**

| Check | Result |
|---|---|
| `sqlite_queue_enrollment_storage`, as registered | Passed, 2.23 s |
| `sqlite_workbench_settings_storage`, as registered | Passed, 4.03 s |
| `sqlite_profile_storage_root`, `sqlite_private_store_open` | Pass |
| `architecture_boundaries` | Passes |
| `test_network_isolation_structure_contract` | Passes |
| `test_network_isolation_manifest` | Passes: `TEST_NETWORK_ISOLATION_OK:configured=357:registrations=344`, **no finding** |

**Portable sources touched:**

- `tests/sqlite_contract/queue_enrollment_storage_tests.cpp` (a Windows branch
  and one include);
- `tests/sqlite_contract/workbench_settings_storage_tests.cpp` (a Windows block
  and one include);
- `tests/sqlite_contract/CMakeLists.txt` (two `WINDOWS_UNCONTAINED` markers,
  which do nothing elsewhere);
- `scripts/verify_test_network_isolation.py` (pins).

## macOS run; finished (dsp-003)

Main merged `windows-lane` at `b52fea2f` as `3bd65861`. On macOS,
`sqlite_queue_enrollment_storage`, `sqlite_workbench_settings_storage`, both
isolation gates and `architecture_boundaries` pass
(`build/r2-logs/dsp3-int10-ctest.log`).
