---
schema: "implement-rfc.card.v1"
id: "BPR-0813"
title: "Windows private stores reopen SQLite by path after admission"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-0806 critical review","found_at":"c0f2fa31","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0813: Windows private stores reopen SQLite by path after admission

## Finding

[[BPR-0806]] admits the profile registry, window-review restoration and
device-binding SQLite files on Windows through the adapter's private-object
checks (`detail::admit_private_store_file`, `catalog_database.cpp:4623-4657`).
Those checks cover an owner-only DACL, no reparse point, a single link and a
matching handle path. The independent critical review found no blocker and
two follow-ups:

1. **Admission and open are separate operations.** The check opens with
   `FILE_FLAG_OPEN_REPARSE_POINT`, refuses reparse points and extra links,
   then drops the handle. `sqlite3_open_v2` then reopens the file by path
   (`profile_registry_store.cpp:440`, `window_review_restoration_store.cpp:514`,
   `device_binding_store.cpp:219`) with the default VFS, where
   `SQLITE_OPEN_NOFOLLOW` has no effect: in SQLite 3.53.4 it acts only when
   `sqlite3OsFullPathname` returns `SQLITE_OK_SYMLINK`, which
   `winFullPathname` never does. A reparse point or second link substituted
   between the two operations would be opened. Only the owner and
   administrators can write in `state\v1`, and POSIX has the same
   check-then-open shape, so this is parity rather than a regression. The
   catalog already opens through `secure_windows_vfs` (`open_sqlite`,
   `catalog_database.cpp:1274`).
2. **The path is converted with `path.string()`.** The same three stores pass
   `path.string()` to `sqlite3_open_v2`. On MSVC that string is in the ANSI
   code page, while the catalog uses `u8string()`
   (`catalog_database.cpp:1288`). A profile root containing characters outside
   the code page would still fail at startup on Windows.

## Scope

- Open the three stores through the same secure Windows VFS the catalog uses,
  or bind the admitted handle's identity to the opened file. Either closes the
  gap by construction.
- Pass UTF-8 paths to SQLite on every platform.
- Owned paths: the three store sources, `storage_internal.hpp`, and the
  Windows parts of `catalog_database.cpp`.

## Acceptance

- A Windows contract shows a substituted reparse point or hard link between
  admission and open is refused. It must fail against the current code.
- A Windows contract opens all three stores under a root containing
  characters outside the ANSI code page.
- The macOS and Linux store courses still pass.

## Windows station, 2026-09-17 (task-20260917-win-hardening)

The base is `de58ebc6`. Logs are under `build/logs/task-20260917-win-hardening/`.

**Change: bind the held handle to the name.** The three stores open through
one helper, `detail::open_private_store_database` (`storage_internal.hpp`,
implemented in `catalog_database.cpp`). It runs in this order:

1. It admits the file, as before.
2. It opens the file by its **UTF-8** path on every platform. Windows uses
   `win32-longpath`; POSIX uses the default VFS.
3. It restricts the file to its owner.
4. **On Windows**, it binds the handle SQLite holds
   (`SQLITE_FCNTL_WIN32_GET_HANDLE`) to the admitted name. The handle must be
   a private regular file with one link and no reparse point, its final path
   must be the name, and its identity must equal the identity the name has
   now. Otherwise the connection is closed and the open refused.

A test-build checkpoint,
`detail::testing::set_private_store_open_checkpoint`, runs after admission and
after open (`BPR_SQLITE_TESTING` only, like the adapter's existing hooks).

**Contract.** `sqlite_private_store_open` (`tests/sqlite_contract/private_store_open_tests.cpp`,
Windows only) runs through the public stores. It has three courses:

- **`non-ansi-root`.** All three stores open and create their files under
  `bpr-0813-プロフィール-😀-…`.
- **`symbolic-link`.** At the admitted checkpoint the registry file is moved
  aside and a file symbolic link to another private registry is put in its
  place. At the opened checkpoint the link is removed and the original name
  restored, so the path-based restriction sees exactly what was admitted.
  Loading must fail. Output: `restored=1:loaded=0`.
- **`hard-link`.** The same with a hard link. It cannot be undone while SQLite
  holds the file, because the Win32 VFS does not share delete, so the name
  still has two links when the path-based restriction looks. Output:
  `restored=0:loaded=0`. It shows the refusal but not the gap.

**Against current behaviour** (reverted trial): the binding was removed and
HEAD's `path.string()` open restored.

- `symbolic-link` **fails**: `restored=1:loaded=1` and `FAILED: a store opened
  through a substituted symbolic-link is refused`. The store served the
  substituted database.
- `non-ansi-root` **fails with exit 3**: the narrow conversion throws and
  nothing catches it.
- `hard-link` passes, as expected; HEAD's restriction already refuses a second
  link.

**Registration.** The contract is pinned `WINDOWS_UNCONTAINED` with cause
`dos-volume-name`. Contained, it stops at `canonical: Access is denied.` of
the temporary directory. It passes uncontained (0.14 s).

**Results after rebuild.** These passed:

- `sqlite_private_store_open`, `sqlite_device_binding_storage`,
  `sqlite_profile_registry_storage`, `sqlite_window_review_restoration_storage`,
  `sqlite_profile_activation`, `sqlite_epoch_catalog_storage_contract`;
- `application_device_binding_restart` (38.56 s),
  `application_profile_startup_public_path`,
  `application_profile_management_public_path`,
  `application_profile_multi_window_public_path` (33.74 s),
  `application_windows_profile_storage_startup_contract`;
- `architecture_boundaries` and `test_network_isolation_structure_contract`.

`test_network_isolation_manifest` reports only [[BPR-0812]]'s two parked epoch
rows.

**Found, not changed.** `sqlite_profile_storage_root` is registered contained
but fails contained on this host: exit 3, because its scratch root calls
`std::filesystem::canonical` first. It passes uncontained. Its sources do not
touch the changed open path.

**Still open: macOS and Linux store courses.** The helper changes the POSIX
open as well:

- The path is UTF-8 (`u8string`), which is the same bytes as before.
- The restriction now runs before each store's read limits or read scope is
  installed.
- A failed open now closes its connection. Previously the device-binding store
  left it open.

**Portable sources touched:**
- `src/adapters/sqlite/src/catalog_database.cpp`: the helper and the testing
  checkpoint.
- `src/adapters/sqlite/src/storage_internal.hpp`: the helper declaration.
- `src/adapters/sqlite/include/bpr/adapters/sqlite/testing.hpp`: the testing
  checkpoint.
- `profile_registry_store.cpp`, `window_review_restoration_store.cpp` and
  `device_binding_store.cpp`.
- `tests/sqlite_contract/CMakeLists.txt`: a Windows-only block.
- `scripts/verify_test_network_isolation.py`: pin only.

## macOS store courses and critical review (dsp-003)

Main merged `windows-lane` at `711d3ddc` as `902cda35`.

**macOS at main `8a03a5be` passes all of these** (`build/r2-logs/dsp3-int1-ctest.log`):

- `sqlite_device_binding_storage`, `sqlite_profile_registry_storage` and
  `sqlite_window_review_restoration_storage`;
- `sqlite_profile_activation`, `sqlite_profile_runtime_lease`,
  `sqlite_profile_storage_root` and `sqlite_epoch_catalog_storage_contract`;
- `application_device_binding_restart` (29.35 s);
- `application_profile_startup_public_path`,
  `application_profile_management_public_path` and
  `application_profile_multi_window_public_path`.

`sqlite_private_store_open` is Windows-only.

**Independent critical review** of `711d3ddc` found **no blocker**. It read
the source, SQLite 3.53.4's amalgamation and the recorded Windows evidence,
and could not run Windows code.

What it confirmed:

- The held handle is bound on volume serial plus 128-bit file ID, and
  reparse, delete-pending and a link count other than 1 are refused on that
  handle itself.
- `path_matches_handle` compares against the same `\\?\` name that
  admission uses.
- `win32-longpath` differs from `win32` only in maximum path length, so
  locking and journalling of existing stores are unchanged.
- Restriction must precede the bind for a newly created file, and does.
- The checkpoint exists only in `BUILD_TESTING` builds.
- The non-ANSI course opens all three public stores.
- On POSIX the path bytes are identical, and every failure still maps to
  `unavailable`.

Its non-blocking findings:

1. `open_private_store_database`'s `bad_alloc` catch does not close an
   already-open connection (`catalog_database.cpp` ~4771). Only reachable
   when memory runs out.
2. The device-binding store now latches `Failed` when admission fails
   (`device_binding_store.cpp` ~215). Before, an admission failure returned
   `unavailable` without latching, so a later `ensure_open` retried. The host
   treats `corrupt` and `unavailable` alike, so no authority changes, but
   retry behaviour changed on every platform.
3. The symbolic-link course needs Developer Mode or the symlink privilege,
   and fails rather than skips without it.
4. SQLite still opens side files (`-journal`) by path, and a reparse point
   pre-placed in `state\v1` would be followed. The code behaved the same
   before this change, and exploiting it needs write access to the owner's
   state directory.
5. The contract binary prints `pass` for an unrecognised argument (ctest
   passes none).
6. `u8string()` can throw on a lone-surrogate root name, and the helper
   catches only `bad_alloc`.

These are follow-up hardening, not criteria of this card.

**Still open:** the Linux store courses.

## Linux store courses; finished (dsp-003)

`bpr-linux` ran the 11 store courses at `84ac5e0c` from a stock full build
(1249/1249, no warnings), and main merged the run as `f4758aa9`. All 11 pass
([the dsp-003 Linux confirmation](../../verification/dsp3-linux-confirmation.md), step 3). The critical review's hardening items were done in
[[BPR-0841]].
