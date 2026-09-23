---
schema: "implement-rfc.card.v1"
id: "BPR-0883"
title: "Five durable stores bypass the private-store open helper and drop their validated directory handles"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable sqlite partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0883: Five durable stores bypass the private-store open helper and drop their validated directory handles

## Defect

`open_private_store_database` (`src/adapters/sqlite/src/catalog_database.cpp:4702-4799`)
is used by only three stores: the profile registry, window restoration and
device binding. Five others open with a bare
`sqlite3_open_v2(aPath.string())` — no `SQLITE_OPEN_NOFOLLOW`, no admission, no
0600 narrowing:

- `appearance_settings_database.cpp:1137-1144`
- `workbench_settings_database.cpp:771-778`
- `queue_enrollment_database.cpp:582-589`
- `review_ledger_database.cpp:1132-1139`
- `review_mutation_recovery_store.cpp:1234-1242` and `:1282-1290`

Three consequences, the first of which is a defect this project has already
fixed once:

1. **`path.string()` is the ANSI code page on Windows.** That is exactly the
   failure [[BPR-0813]] AC2 repaired for the other three stores ("would still
   fail at startup"). A profile root outside the code page makes the review
   ledger and the recovery journal unavailable **when a pull request is
   opened** — so the same bug survives in the stores a reviewer touches most.
2. The files are created at SQLite's default mode (0644 & ~umask on POSIX) and
   never narrowed, relying entirely on the 0700 `state/v1` directory.
3. Each `make_*` takes `open_state_directory` as a **local** and drops it at
   return (`review_ledger_store.cpp:240-246`,
   `appearance_settings_store.cpp:84-92`,
   `workbench_settings_store.cpp:144-152`,
   `queue_enrollment_store.cpp:183-191`,
   `review_mutation_recovery_store.cpp:1230-1233`), directly contradicting
   `state_directory.hpp:32-33`: "The handles are held, not merely checked."
   A validated handle that is released before the open is a check, not a
   guarantee.

**There is also no POSIX contract that any of this holds.**
`private_store_open_tests.cpp` is registered Windows-only
(`tests/sqlite_contract/CMakeLists.txt:397`), and no 0600 or
`invalid_permissions` assertion exists in `profile_registry_storage_tests.cpp`,
`window_review_restoration_storage_tests.cpp` or
`device_binding_storage_tests.cpp`. `restrict_private_store_file`
(`catalog_database.cpp:4689-4698`) is path-based — `std::filesystem::permissions`
follows links — and runs *after* SQLite created the file at its default mode,
so the narrowing is worth pinning rather than assuming.

## Scope

- Route all five through `open_private_store_database`, or say per store why
  it must not be.
- Hold the state-directory handle across the open, as the header already
  promises.
- Extend the non-ANSI-root course to every store factory, and register it on
  POSIX as well as Windows.
- Assert `stat` mode 0600 for each `*.sqlite3` under `state/v1`, on POSIX, for
  all eight stores.

## Acceptance

- Every durable store opens through the private helper, or carries a recorded
  exception with its reason.
- A profile root outside the Windows code page opens the review ledger and the
  recovery journal.
- A POSIX contract pins 0600 on every private store file, and fails if one is
  created at the default mode.
