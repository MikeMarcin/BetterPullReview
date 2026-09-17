---
schema: "implement-rfc.card.v1"
id: "BPR-0841"
title: "Harden the private store open helper after BPR-0813's critical review"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win", "mac", "linux"]
metadata: {"found_by":"dsp-003 critical review of BPR-0813 (711d3ddc)","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
acceptance: [{"criterion":"AC1 items 1, 2, 5 and 6 fixed with a course or reverted trial each; items 3 and 4 fixed or declined with a reason","outcome":"pass","revision":"bf930d12c74c959e49b81ffdf2a1297f01abcb17","evidence":{"kind":"artifact","detail":"bpr-win: item 1 exception-releases-file renamed=1 (trial renamed=0); item 2 admission-retry refused=1:retried=1 (trial retried=0); item 5 unknown argument exits 1 (trial printed pass); item 6 lone-surrogate-root loaded=0 (trial aborted); item 4 named skip on ERROR_PRIVILEGE_NOT_HELD; item 3 declined by the integrator with the threat-model reason recorded on this card."}},{"criterion":"AC2 the Windows and macOS store courses listed on BPR-0813 still pass","outcome":"pass","revision":"2f3a6720 (Windows); d514e76e (macOS)","evidence":{"kind":"command","detail":"Windows: sqlite_private_store_open (6 courses), the three store storage contracts, profile activation, epoch catalog storage, device binding restart, profile startup/management/multi-window, Windows storage startup contract and isolation gates pass. macOS: 16/16 at d514e76e (build/r2-logs/dsp3-int7-ctest.log)."}}]
---

# BPR-0841: Harden the private store open helper after BPR-0813's critical review

## Origin

An independent critical review of [[BPR-0813]] (`711d3ddc`, merged into main
at `902cda35`) found **no blocker**. It read the source, SQLite 3.53.4's
amalgamation and the recorded Windows evidence. These are its non-blocking
findings, recorded on BPR-0813.

## Scope

1. **Leak on `bad_alloc`.** `detail::open_private_store_database`
   (`src/adapters/sqlite/src/catalog_database.cpp` ~4771) catches `bad_alloc`
   without closing a connection it has already opened. On Windows that keeps
   the store file held. Close on every failure path, for example with an RAII
   guard, as the helper's contract says.
2. **Retry latch.** `device_binding_store.cpp` (~215) now sets `Failed` when
   admission fails, so later `ensure_open` calls return `corrupt` without
   retrying. Before BPR-0813 an admission failure returned `unavailable` and
   did not latch. Restore the earlier retry behaviour for admission failures,
   or record why latching is correct.
3. **Side files (Windows).** SQLite opens `<name>-journal` by path, without
   refusing a reparse point. A reparse point pre-placed in `state\v1` would be
   followed. The code behaved the same before BPR-0813, and exploiting it
   needs write access to the owner's state directory. Decide whether the
   catalog's secure VFS route should cover the private stores' side files too.
4. **Host-dependent course (Windows).** The `symbolic-link` course needs
   Developer Mode or the symlink privilege, and without it fails rather than
   skipping with a named reason.
5. **Harness.** `private_store_open_tests` prints `pass` for an unrecognised
   argument.
6. **Path encoding (Windows).** `path.u8string()` can throw
   `std::system_error` for a lone-surrogate root name, and the helper catches
   only `bad_alloc`. Map it to an ordinary failure.

## Acceptance

- Items 1, 2, 5 and 6 are fixed, each with a course or a reverted trial that
  shows it matters. Items 3 and 4 are fixed or explicitly declined with a
  reason.
- The Windows and macOS store courses listed on BPR-0813 still pass.

## Windows station, 2026-09-17 (task-20260917-win-private-roots)

The base is `6c44b6eaedfc7a752f0ab78255c2403bc4f406e4`. Logs are under
`build/logs/task-20260917-win-private-roots/`. Each reverted trial below was
rebuilt and run against its own course in `sqlite_private_store_open`
(`tests/sqlite_contract/private_store_open_tests.cpp`, Windows only).

| Item | Change | Course | Reverted trial |
|---|---|---|---|
| 1. Leak on an exception | `open_private_store_database` owns the connection in a local RAII owner from the moment SQLite returns it. Every failure path, an exception included, closes it; success releases ownership with `std::exchange`. | `exception-releases-file`: the test checkpoint throws `std::bad_alloc` after SQLite opened the file. The store object stays alive, and renaming the store file must succeed. Result: `renamed=1`. | Owner does not close: `renamed=0`, `FAILED: the refused open left the store file unheld` |
| 2. Retry latch | `device_binding_store.cpp` admits the file before the helper; a refused admission returns `unavailable` without setting `Failed`, as before [[BPR-0813]]. Only a failure after admission latches. | `admission-retry`: seed the store; widen the file's DACL to grant Everyone read; load fails; restore the exact owner-private DACL; the **same** store loads. Result: `refused=1:retried=1`. | No separate admission: `retried=0`, `FAILED: the same device-binding store opens once its file is admissible again` |
| 5. Harness | An unrecognised course, or more than one argument, prints `FAILED: unknown course...` and exits 1. | `private_store_open.exe bogus` → exit 1 | Check disabled: `bogus` printed `private-store-open-tests:pass`, exit 0 |
| 6. Path encoding | The helper also catches `std::system_error` and maps it to `invalid_path`. | `lone-surrogate-root`: a root named `bpr-0841-surrogate-` followed by a lone U+D800. Result: `loaded=0`, and the process continues. | Mapping removed: the course dies with `FAILED: No mapping for the Unicode character exists in the target multi-byte code page.` (the exception escapes the store) |

**Item 4, symbolic-link course: fixed, not host-demonstrable.** The course
records `GetLastError()` when `CreateSymbolicLinkW` fails. On
`ERROR_PRIVILEGE_NOT_HELD` it restores the store file and prints
`private-store-open:symbolic-link:skipped:no-symbolic-link-privilege`, a named
skip instead of a failure. Any other failure still fails. This station has
Developer Mode, so the skip path could not be exercised without changing host
policy, which was not done.

**Item 3, side files through the secure VFS: not changed; a design decision is
needed.** SQLite opens `<name>-journal` (and `-wal` or `-shm` in WAL mode) by
path. The catalog's `secure_windows_vfs` is catalog-specific:

- it recognises exactly the catalog's leaf names;
- it holds one parent identity and one security context;
- it registers a uniquely named VFS for the catalog database's lifetime.

Covering the three stores means one of:

- **(a)** generalising that VFS into a per-store instance, whose lifetime each
  store owns next to its connection;
- **(b)** a second, smaller store VFS that only refuses reparse points and
  extra links on side-file opens;
- **(c)** declining, because exploiting it needs write access to the owner's
  own `state\v1`, which already defeats the store.

That choice, and whether the stores should take on VFS registration at all,
belongs to the owner.

**The BPR-0813 store courses still pass** after a full rebuild:

- `sqlite_private_store_open` (all six courses), `sqlite_device_binding_storage`,
  `sqlite_profile_registry_storage`, `sqlite_window_review_restoration_storage`,
  `sqlite_profile_activation`, `sqlite_epoch_catalog_storage_contract`;
- `application_device_binding_restart`,
  `application_profile_startup_public_path`,
  `application_profile_management_public_path`,
  `application_profile_multi_window_public_path`,
  `application_windows_profile_storage_startup_contract`;
- `architecture_boundaries`, `test_network_isolation_manifest` (no finding) and
  `test_network_isolation_structure_contract`.

**Portable sources touched, for the macOS run:**

- `src/adapters/sqlite/src/catalog_database.cpp`: the helper's RAII owner and
  the `std::system_error` mapping. POSIX takes the same paths.
- `src/adapters/sqlite/src/device_binding_store.cpp`: the admission before the
  helper. On POSIX this means an admission refusal no longer latches.
- `tests/sqlite_contract/private_store_open_tests.cpp` is Windows only.

## Integrator decision (dsp-003): item 3 declined

Item 3 (SQLite opens `-journal` side files by path without refusing a reparse
point) is **declined**. Placing a reparse point in `state\v1` needs write
access to the owner-private state directory. That is the owner, or someone
with the owner's rights, and they can already replace the store files
themselves, so following such a link crosses no privilege boundary. The
code behaved the same before [[BPR-0813]]. A per-store secure VFS, or a second
smaller VFS, would add a lifetime and registration surface to every store for
no protection against an actor who cannot already write the stores. If a
later threat model admits an attacker with write access to `state\v1` who
cannot write the store files, this decision should be revisited.

## macOS run; finished (dsp-003)

Main merged `windows-lane` at `2f3a6720` as `d514e76e`. On macOS at that head,
the store courses listed on [[BPR-0813]] pass, along with
`sqlite_profile_runtime_lease`, `sqlite_profile_storage_root`, both isolation
gates, `architecture_boundaries` and the round-trip contract: 16 of 16
(`build/r2-logs/dsp3-int7-ctest.log`). Items 1 and 2 are the portable changes:
the connection owner in `catalog_database.cpp`, and admission before the helper
in `device_binding_store.cpp`.
