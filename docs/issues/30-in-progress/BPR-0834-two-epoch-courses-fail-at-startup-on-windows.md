---
schema: "implement-rfc.card.v1"
id: "BPR-0834"
title: "Two epoch courses fail at startup on Windows before any step"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0834: Two epoch courses fail at startup on Windows before any step

## Defect

On Windows, uncontained,

- `application_epoch_ordinary_update_recreated` and
- `application_epoch_portable_occurrence_two_catalogues`

both print `STARTUP_FAILED` before any course step, identically with the
unmodified course script at HEAD. Narrowing their fixture root to the
owner-private shape did not help (that change was reverted). Because neither
passes uncontained, [[BPR-0812]] could not measure their contained cause or
pin them; the manifest still reports their six findings.

Reported by `bpr-win` in `task-20260917-win-hardening` step 2, not carded
there.

## Scope

- Find which startup refusal each prints (the `startup_failure` value, as
  the station did for [[BPR-0832]]) and fix it.
- Then measure the contained run and pin or contain the rows, which closes
  BPR-0812's remaining findings.

## Acceptance

- Both courses pass uncontained on the Windows station, and each is either
  contained and passing or pinned with a measured reason.
- They still pass on macOS.

## Windows station, 2026-09-17 (task-20260917-win-startup-roots)

The base is `559377700d52f37d6c094cd646200c67ab20eeb5`. Logs are under
`build/logs/task-20260917-win-startup-roots/`.

**The refusal.** A reverted diagnostic at `src/app/main.cpp`'s startup
completion printed `startup_failure=5` (`registry`) for both courses.

**The cause.** CPython's `Path.mkdir(mode=0o700)` on Windows writes a
protected DACL with explicit SYSTEM, Administrators and OWNER RIGHTS entries.
The profile registry refuses that root. The earlier narrowing attempt under
[[BPR-0812]] used `icacls /inheritance:r /grant:r`, which leaves those
explicit entries in place, so the root was never owner-private. The
distinction was measured by running the application by hand on three roots:

| Root | Result |
|---|---|
| As created by `mkdir(mode=0o700)` | `STARTUP_FAILED`, `startup_failure=5` |
| Narrowed without removing the explicit entries | `STARTUP_FAILED`, `startup_failure=5` |
| Narrowed with `/remove:g *S-1-5-18 *S-1-5-32-544 *S-1-3-4` | Starts; `PUBLIC_PATH_OK`, `PUBLIC_PREVIEW_ENABLED_OK` |

**The fix** is in the two course scripts.

- **`make_root_private(root)`.** POSIX keeps `mkdir(mode=0o700)`. Windows
  creates the directory without a mode, then narrows it with `icacls` to one
  inheritable full-access grant for the user. Inside an AppContainer it adds a
  second grant for the process's own container SID, which it reads from the
  token. SYSTEM, Administrators and OWNER RIGHTS are removed. This is the shape
  `make_storage_root_owner_private` writes.
- **Two further Windows failures,** each found in turn after the startup
  refusal cleared:
  - The courses use `contained_temporary_directory`. Contained, a `tempfile`
    root is unusable ([[BPR-0350]]).
  - That parent inherits `%TEMP%`'s foreign grants on this station, which
    made the composed read print `PUBLIC_PATH_FAILED`. So the parent is
    narrowed with the same helper.
- **Read-only pack files.** `epoch_ordinary_update_public_path.py` removes
  its catalogue between passes with an `onexc` handler that clears the
  read-only attribute Git sets on pack files. Windows otherwise refuses
  `unlink` with `WinError 5`.

**Results.**

| Check | Result |
|---|---|
| `application_epoch_portable_occurrence_two_catalogues`, uncontained | **Passed**, 90.65 s: `EPOCH_PORTABLE_TWO_CATALOGUES_OK:3` |
| `application_epoch_ordinary_update_recreated`, uncontained | **Passed**, 31.21 s: `EPOCH_ORDINARY_RECREATED_OK:1` |
| Both rows contained, through a reverted registration hook with the final scripts | Both **fail** with `STARTUP_FAILED` and the reverted diagnostic's `startup_failure=5` (`registry`). The root and its parent carry the user and container grants, so the fixture no longer causes the refusal; this is the application-level containment step. |
| Registration | Both stay `WINDOWS_UNCONTAINED` and are now pinned `dos-volume-name` in `scripts/verify_test_network_isolation.py`, with the measurement in the comment |
| `test_network_isolation_manifest` | **Passes**: `TEST_NETWORK_ISOLATION_OK:configured=357:registrations=344:backend=windows-appcontainer`. **Zero findings.** |
| `test_network_isolation_structure_contract` | Passes |

**Portable sources touched:**
- `tests/end_to_end/epoch_portable_occurrence_public_path.py`
- `tests/end_to_end/epoch_ordinary_update_public_path.py`
- `scripts/verify_test_network_isolation.py` (pins only)

On POSIX the scripts still create roots with `mkdir(mode=0o700)` and narrow
nothing. They now use `contained_temporary_directory`, which on POSIX is
`tempfile`, and the catalogue removal's `onexc` handler runs only when a
removal fails. The macOS run is the integrator's.

## macOS run; finished (dsp-003)

Main merged `windows-lane` at `aedf471f` as `013d2036`. **macOS at main `013d2036`** after the merge (`build/r2-logs/dsp3-int5-ctest.log`):
`application_epoch_ordinary_update_recreated` (70.19 s) and
`application_epoch_portable_occurrence_two_catalogues` (231.85 s) both pass,
with `test_network_isolation_manifest` and
`test_network_isolation_structure_contract`. On POSIX the scripts still
create roots with `mkdir(mode=0o700)`, and `contained_temporary_directory`
is `tempfile`.
