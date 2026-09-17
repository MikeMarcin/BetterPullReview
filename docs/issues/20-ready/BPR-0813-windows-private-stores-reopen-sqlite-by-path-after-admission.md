---
schema: "implement-rfc.card.v1"
id: "BPR-0813"
title: "Windows private stores reopen SQLite by path after admission"
status: "Ready"
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
