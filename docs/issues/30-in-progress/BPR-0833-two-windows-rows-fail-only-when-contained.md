---
schema: "implement-rfc.card.v1"
id: "BPR-0833"
title: "Two Windows rows fail only when contained"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0833: Two Windows rows fail only when contained

## Defect

Two rows pass uncontained on Windows and fail inside the AppContainer:

- `application_composition_capabilities_inside_boundary` prints
  `STARTUP_FAILED` contained. Uncontained, the same probe prints
  `COMPOSITION_TEST_BOUNDARY:inside`, a recording browser and no CLI read.
  No person's app was running.
- `sqlite_profile_storage_root` is registered contained but fails contained:
  exit 3, because its scratch root calls `std::filesystem::canonical` first
  (the same `Access is denied` shape [[BPR-0812]] measured). It passes
  uncontained. Its sources do not touch [[BPR-0813]]'s open path.

Reported by `bpr-win` in `task-20260917-win-hardening` (steps 1 and 4),
not carded there.

## Scope

- Measure each contained failure's first failing call.
- [[BPR-0820]] changes how `application_composition_capabilities_inside_boundary`
  composes its profile (a private, owner-private automated root). Rerun it on
  a `windows-lane` that contains BPR-0820 before diagnosing it.
- Fix the cause, or pin the row with its measured reason in
  `scripts/verify_test_network_isolation.py` as BPR-0812 did. A pin needs a
  passing uncontained run and the measured contained cause.

## Acceptance

- Both rows pass on the Windows station as registered, or are pinned with a
  measured reason, and `test_network_isolation_manifest` reports no new
  finding.

## Windows station, 2026-09-17 (task-20260917-win-private-roots)

The base is `6c44b6eaedfc7a752f0ab78255c2403bc4f406e4`, which contains
[[BPR-0820]]. Logs are under `build/logs/task-20260917-win-private-roots/`.

### `sqlite_profile_storage_root`: pinned

| Run | Result |
|---|---|
| Uncontained | `profile-storage-root-tests:pass`, exit 0 |
| Contained | Exit 3 |
| Contained, with a reverted `std::set_terminate` hook | `canonical: Access is denied.: "...\Packages\bprautomatedtest\AC\Temp\bpr-0694-roots-..."`. The first call, `std::filesystem::canonical` of the scratch root, throws before any course step. |

The row is now registered `WINDOWS_UNCONTAINED` and pinned `dos-volume-name`
in `scripts/verify_test_network_isolation.py`. As registered it passes
(0.05 s). `test_network_isolation_manifest` passes with no finding
(`TEST_NETWORK_ISOLATION_OK:configured=357:registrations=344`), and
`test_network_isolation_structure_contract` passes.

### `application_composition_capabilities_inside_boundary`: measured, parked on a design decision

With BPR-0820's private automated profile, the row now prints
`AUTOMATED_PROFILE_PRIVATE_FAILED` contained, where it used to print
`STARTUP_FAILED`. Run uncontained with the same arguments, it prints
`AUTOMATED_PROFILE_PRIVATE_OK` and `COMPOSITION_TEST_BOUNDARY:inside`, and
exits 0. `application_composition_capabilities_product_run` behaves the same
way (contained: `AUTOMATED_PROFILE_PRIVATE_FAILED`; uncontained:
`COMPOSITION_TEST_BOUNDARY:outside`, system browser).

**The first failing call**, measured with reverted diagnostics in
`src/app/main.cpp` and in each `return false` of
`make_storage_root_owner_private`:

- `QTemporaryDir` is valid inside the container, and Qt's
  `canonicalFilePath()` resolves
  (`C:/Users/mikem/AppData/Local/Packages/bprautomatedtest/AC/Temp/bpr-automated-profile-...`).
- In `bpr::adapters::sqlite::make_storage_root_owner_private`,
  `canonical_windows_root` (step 1) and `private_security::load` (step 2)
  succeed.
- **Step 3 fails:** `open_existing_object(root, directory, WRITE_DAC, false)`
  returns error 5 (`ERROR_ACCESS_DENIED`).

The directory `QTemporaryDir` creates inside the container inherits the
container temporary directory's access-control list, which grants the
package Modify but not `WRITE_DAC`. A lowbox token gets no implicit owner
`WRITE_DAC` from the restricted check, so the container cannot narrow a
directory it did not create with the private descriptor.

**The decision needed.** Neither fix is local:

- **(a) Product fix.** Create the automated profile directory with the private
  descriptor at creation (a `CreateDirectoryW` with the adapter's private
  security attributes, including the container SID), so no later `WRITE_DAC`
  is needed. That changes BPR-0820's integrator-owned seam in `main.cpp` and
  adds, or changes the Windows behaviour of, a SQLite adapter entry point:
  `create_storage_root_owner_private` on Windows today calls
  `create_directories` and then narrows.
- **(b) Pin.** Pin both capability rows `WINDOWS_UNCONTAINED` under a new,
  documented cause (for example `inherited-descriptor`). Those two rows are the
  containment evidence for the boundary answer, so pinning them trades that
  evidence away on Windows.

Nothing was changed for these rows.

**Portable sources touched on this card:** `tests/sqlite_contract/CMakeLists.txt`
(a `WINDOWS_UNCONTAINED` marker, a no-op elsewhere) and
`scripts/verify_test_network_isolation.py` (pin).

## Integrator decision (dsp-003): option (a)

The integrator chooses **(a)**: create the automated profile's private
directory with the adapter's owner-private descriptor at creation, so a
contained run never needs `WRITE_DAC` on a directory it did not create. The
two capability rows are the containment evidence for BPR-0802's boundary
answer, and giving that up on Windows would weaken the only designated
outside-direction row. (a) stays inside the SQLite adapter's existing ownership
and adds no dependency edge.

Constraints for the implementation:

- On Windows, the adapter's storage-root creation creates a directory with
  its private security attributes, including the container SID when
  contained, rather than `create_directories` followed by narrowing. The
  POSIX behaviour is unchanged.
- The automated-run seam in `src/app/main.cpp` gets its private parent from
  that creation rule, for example as a uniquely named child of the
  `QTemporaryDir`. The `QTemporaryDir` still owns removal. BPR-0820's gate and
  marker stay.
- The product's own data-root creation keeps working on Windows. Run the
  profile startup and storage courses listed on [[BPR-0813]].
- `src/app/main.cpp` is pinned by the round-trip contract. List it for the
  Mac run; main reseals after review.
