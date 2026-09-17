---
schema: "implement-rfc.card.v1"
id: "BPR-0833"
title: "Two Windows rows fail only when contained"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
acceptance: [{"criterion":"AC1 both rows pass on the Windows station as registered, or are pinned with a measured reason, and the manifest reports no new finding","outcome":"pass","revision":"0e37446fc0175643c73623c872631769fa804735 and f91321e3ddb1820386b98bd789f6b16e784c6ab7","evidence":{"kind":"command","detail":"sqlite_profile_storage_root pinned dos-volume-name (contained canonical Access is denied, measured) and passes as registered; both capability rows registered WINDOWS_UNCONTAINED and pinned dos-volume-name with the measured WRITE_DAC refusal (and the ancestor-walk refusal under option (i)); all 18 checked rows pass on Windows; TEST_NETWORK_ISOLATION_OK:configured=357 with no finding. macOS: both rows and both isolation gates pass at 0c58092e (build/r2-logs/dsp3-int11-ctest.log)."}}]
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

## Windows station: option (a), 2026-09-17 (task-20260917-win-creation-descriptor)

The base is `31fff50675a71bc2c5ce55857c72e57dc36933df`. Logs are under
`build/logs/task-20260917-win-creation-descriptor/`.

### Implemented: storage-root creation writes the descriptor at creation (Windows)

In `src/adapters/sqlite/src/catalog_database.cpp`:

- **`create_storage_root_owner_private`** on Windows no longer calls
  `create_directories` and then narrows. Missing folders above the root are
  created as before and inherit. The root itself is created with
  `CreateDirectoryW` and the adapter's `private_security_attributes` (the
  token user, plus the container SID when contained). A root that already
  exists is accepted only if `validate_private_storage_root` admits it, which
  is also the final check after creation. POSIX is unchanged.
- **`real_canonical_directory`,** the existing-ancestor check, uses the
  adapter's own rule on Windows: not a reparse point, and a spelling that
  `canonical_windows_root` accepts. The reason is measured: inside the
  container `std::filesystem::canonical` is denied, which was the first refusal
  after the creation change (`real_canonical_directory` returned false). POSIX
  is unchanged.

Verification after a full rebuild, all passing as registered:

- `application_profile_startup_public_path`,
  `application_profile_management_public_path`,
  `application_profile_multi_window_public_path`,
  `application_windows_profile_storage_startup_contract`,
  `application_device_binding_restart`, `application_public_path`,
  `application_render`;
- `sqlite_profile_storage_root`, `sqlite_storage_root_contract`,
  `sqlite_private_store_open`, the store storage courses;
- `architecture_boundaries` and both isolation gates, with zero findings.

`sqlite_queue_enrollment_storage` and `sqlite_workbench_settings_storage` fail
contained, but identically with this change reverted, so they are reported
separately as pre-existing.

### Blocked: the automated-run seam cannot use a child of the `QTemporaryDir` inside the container

The seam change the card describes was made and measured, then **not
committed**; the patch is kept at
`build/logs/task-20260917-win-creation-descriptor/main-seam-child-of-qtemporarydir.patch`.
It took the private parent as `<QTemporaryDir>/profile`, created by
`create_storage_root_owner_private`.

| Run | Result |
|---|---|
| Uncontained | Works: `AUTOMATED_PROFILE_PRIVATE_OK`, `COMPOSITION_TEST_BOUNDARY:inside` |
| Contained (`application_composition_capabilities_inside_boundary` and `_product_run`) | Still `AUTOMATED_PROFILE_PRIVATE_FAILED` |

Measured with reverted diagnostics inside `create_storage_root_owner_private`:

1. The first contained refusal was `real_canonical_directory`
   (`std::filesystem::canonical` denied). That is fixed above.
2. The next is `CreateDirectoryW(<QTemporaryDir>/profile, private attributes)`
   → **error 5**. Inside the container even `GetNamedSecurityInfoW` on the
   `QTemporaryDir` fails.
3. Uncontained, that directory's descriptor reads
   `O:<user>D:P(A;OICI;FA;;;<user>)(A;OICI;0x120080;;;ME)(A;OICI;0x120080;;;WD)`.
   Qt 6 writes its `0700` as a **protected** owner-only DACL. It names no
   AppContainer SID, so the contained process that created it cannot list it,
   create children in it or read its descriptor.

So no child of a `QTemporaryDir` is usable inside the container, whatever
descriptor the child would get.

**The decision needed.** The card's constraint says the `QTemporaryDir`
still owns removal. Either:

- **(i)** the seam creates its unique private directory directly under
  `QDir::temp()` with `create_storage_root_owner_private` (a fresh random
  name), and a small scope guard (for example `QDir::removeRecursively`)
  owns removal instead of `QTemporaryDir`; or
- **(ii)** the seam keeps `QTemporaryDir`, and the contained rows stay failing
  or are pinned, which reopens the choice (a) was meant to close.

Nothing in `src/app/main.cpp` was changed.

**Portable sources touched:** `src/adapters/sqlite/src/catalog_database.cpp`,
Windows branches only. `src/app/main.cpp` is untouched.

## Integrator decision (dsp-003): (i), the seam creates its own private directory

Inside the container, a `QTemporaryDir` gets Qt's protected owner-only DACL
with no container SID. The contained process then cannot use that directory
or create a child in it, so option (a) cannot keep `QTemporaryDir` as the
parent. The integrator chooses **(i)**:

- The seam creates a uniquely named private directory directly under
  `QDir::temp()`, through the adapter's `create_storage_root_owner_private`.
- Creation must be exclusive. The helper validates a directory that already
  exists, so the seam must refuse or retry a name that exists, rather than
  adopt one another process created. Use bounded retries with a fresh random
  name.
- A scope guard owns removal. It removes only the directory this run created,
  on every exit path, after `desktop` is destroyed. A run killed by a signal
  may leave the directory behind, as `QTemporaryDir` did.
- The gate and the `AUTOMATED_PROFILE_PRIVATE_OK` / `_FAILED` markers stay,
  and POSIX behaviour is unchanged apart from where the directory is created.

(ii) is rejected, for the containment reason recorded under option (a).

## Windows station: option (i), 2026-09-17 (task-20260917-win-private-parent). Blocked, not committed

The base is `2699a47e1ccd8e9a820429fc3279899597120292`.

**Built as decided.**

- A new adapter entry point, `create_new_storage_root_owner_private`
  (`storage_root.hpp`), returns `created`, `already_exists` or `failed`.
  - On Windows it refuses a name that `CreateDirectoryW` reports as existing.
  - On POSIX the chain reports whether it created the root.
  - `create_storage_root_owner_private` shares the same implementation.
- The seam in `src/app/main.cpp`:
  - It creates `bpr-automated-profile-<random hex>` directly under the
    canonical `QDir::tempPath()`, with up to 16 fresh names.
  - A scope guard, declared where the `QTemporaryDir` was (before `desktop`),
    removes only the directory this run created.
  - The gate and both markers are unchanged.

It builds, and it works uncontained.

**Contained, it still fails.** Both capability rows print
`AUTOMATED_PROFILE_PRIVATE_FAILED`. Measured with reverted diagnostics:

1. `CreateDirectoryW` now **succeeds** inside the container. The root's
   descriptor, read inside the container, is exactly the private shape:
   `O:<user>D:P(A;OICI;FA;;;<user>)(A;OICI;FA;;;S-1-15-2-...)`.
2. `validate_private_storage_root` then refuses it. `validate_ancestor_chain`
   returns `failure=1` with Windows error 5.

Inside the AppContainer the adapter's walk of the root's ancestors, from the
volume down through `C:\Users\mikem\AppData\Local\Packages\...\AC\Temp`, is
denied. This is the retained-ancestor-chain step the manifest documents
under `dos-volume-name` for every application root. [[BPR-0845]]'s two
storage rows stop at exactly the same call.

So no choice of parent directory lets a contained run pass while the stores
require the ancestor chain to be walkable. The two capability rows can pass
contained only if the ancestor-chain rule changes for a contained process,
which is a product security decision.

**The decision needed.** Either:

- **(1)** change the storage-root rule for contained processes, for example
  accepting ancestors the container cannot open, or starting the walk below
  the first ancestor it can; or
- **(2)** pin both capability rows `WINDOWS_UNCONTAINED` with this measured
  cause, which is the choice (ii) option (a) was meant to avoid.

**Nothing from step 1 is committed.** The complete change is kept as
`build/logs/task-20260917-win-private-parent/bpr0833-option-i-seam-and-exclusive-creation.patch`
(259 lines; `main.cpp`, `storage_root.hpp`, `catalog_database.cpp`), and the
working tree was reverted. The contained runs left
`bpr-automated-profile-*` directories under the container temp: the
created-but-refused roots, plus earlier `QTemporaryDir` roots the container
could not delete. They were removed by hand.

One more fix is needed whatever is decided: a root the call created but
validation then refused is currently returned as `failed`, so the guard never
records it and it leaks. If (1) is chosen, `created` should also be reported
for a root created but refused, or the adapter should remove what it created.

## Integrator decision (dsp-003): pin the capability rows

Option (i) was built and measured on Windows. The seam's private root was
created inside the container with exactly the owner-private descriptor,
including the container SID. The stores' `validate_ancestor_chain` then
refused it with error 5, because the AppContainer cannot open the ancestors
above the temporary directory. That is the documented `dos-volume-name`
application step, the same one [[BPR-0812]], [[BPR-0834]] and [[BPR-0845]]
pinned. No choice of parent passes contained while the stores require that
walk.

Relaxing the ancestor-chain rule for contained processes would be a product
security change, and nothing in this card's scope asks for one. So the
integrator chooses **(2)**. Both capability rows are pinned
`WINDOWS_UNCONTAINED` with this measured cause, as the other rows were.

- The adapter half of option (a), `aa3c6ef1`, stays on main: storage roots
  are created with the private descriptor on Windows. The product data-root
  courses pass with it.
- The option (i) seam patch is not applied, and `src/app/main.cpp` is
  unchanged.
- The pinned rows remain visible to the manifest gate as enumerated
  exceptions.

## Windows station: pinned, 2026-09-17 (task-20260917-win-pin-capability-rows)

The base is `83349437954610dce544e3eebd966ba24f96eb51`, which has the adapter
half `aa3c6ef1` and `main.cpp` unchanged. Logs are under
`build/logs/task-20260917-win-pin-capability-rows/`.

### The first failing call on this code, measured again

With a reverted diagnostic at each refusal of
`make_storage_root_owner_private`, the call `main.cpp` makes at this base, both
rows fail contained at the same point, with the marker set and with it unset:
**`open_existing_object(root, directory, WRITE_DAC)` returns error 5.** The
root is the `QTemporaryDir`, which Qt creates with a protected owner-only DACL
that names no container SID. The run then prints
`AUTOMATED_PROFILE_PRIVATE_FAILED`.

That is not the call the pin request names. The request's
`validate_ancestor_chain` refusal (error 5, after a private root created with
the container SID) was measured under the option (i) patch, which is not
applied. The pin comment records both, in order:

- the refusal this code reaches first;
- the documented application step (the stores' ancestor-chain walk) that the
  row reaches once that first refusal is removed.

Both rows are pinned `dos-volume-name`, as the request directs. If the
vocabulary should instead name the WRITE_DAC refusal separately, that is a
one-word change in the comment and the value.

### Change

- `tests/end_to_end/CMakeLists.txt`: both capability rows are registered
  `WINDOWS_UNCONTAINED`. This changes nothing outside Windows.
- `scripts/verify_test_network_isolation.py`: both rows are pinned
  `dos-volume-name`, with the measurement above in the comment.

### Results, as registered, after a full rebuild at the base

| Check | Result |
|---|---|
| `application_composition_capabilities_inside_boundary` | **Passed**, 1.62 s: `AUTOMATED_PROFILE_PRIVATE_OK`, `COMPOSITION_TEST_BOUNDARY:inside` |
| `application_composition_capabilities_product_run` | **Passed**, 1.61 s: `AUTOMATED_PROFILE_PRIVATE_OK`, `COMPOSITION_TEST_BOUNDARY:outside` |
| `test_network_isolation_manifest` | **Passed**: `TEST_NETWORK_ISOLATION_OK:configured=357:registrations=344`, no finding |
| `test_network_isolation_structure_contract` | Passed |
| `architecture_boundaries` | Passed |
| BPR-0820's other 13 rows and `runtime_process_surface` | All passed: `application_public_path`, `_ghes_cli_unavailable`, the four device-authorization rows, the four anonymous-preview rows, `application_render`, `runtime_audit_contract` and `runtime_process_surface` (18 of 18 in the run) |

### Leftovers

The two diagnostic contained runs of this task each left one
`bpr-automated-profile-*` directory under the container temporary directory.
A contained run cannot delete the owner-only `QTemporaryDir` it created. Both
were removed by hand. No `bpr-automated-profile-*` directory remains, and
none remained from earlier tasks at the start of this one.

The card is left In Progress for the integrator.

**Portable sources touched:** `tests/end_to_end/CMakeLists.txt` (two
`WINDOWS_UNCONTAINED` markers) and `scripts/verify_test_network_isolation.py`
(pins).

## Merged; finished (dsp-003)

Main merged `windows-lane` at `f91321e3` as `1c4b7be6`. The first contained
refusal on the code that is actually merged is
`make_storage_root_owner_private`'s `WRITE_DAC` open of the `QTemporaryDir`
(error 5): Qt's owner-only DACL excludes the container. The ancestor-walk
refusal comes next only under the unapplied option (i) patch. The pin comment
records both, in that order. The integrator's decision is unchanged: no
product security change, so pin.

On macOS at `0c58092e`, both capability rows,
`test_network_isolation_manifest`, `test_network_isolation_structure_contract`
and `architecture_boundaries` pass (`build/r2-logs/dsp3-int11-ctest.log`).

**Latent, recorded here rather than carded.** On Windows, a contained
automated run that fails with `AUTOMATED_PROFILE_PRIVATE_FAILED` leaves its
`QTemporaryDir` behind, because the contained process cannot remove it. With
both rows now uncontained, no registered row does this.
