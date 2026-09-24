---
schema: "implement-rfc.card.v1"
id: "BPR-0883"
title: "Five durable stores bypass the private-store open helper and drop their validated directory handles"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win", "mac"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable sqlite partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005","linux_done_at":"874dc0ae","remaining":"Windows AC2 run","mac_confirmed_at":"0ca739b2","related_cards":["BPR-0806","BPR-0813","BPR-0841","BPR-1044"]}
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

## Linux: all five through the helper, and a contract on every platform (bpr-main-linux, 2026-09-23)

**Linux is done at `874dc0ae`. Windows and macOS still owe one run each.**
AC2 names a Windows code page, and only Windows can observe it. The card
therefore goes back to Ready with `platforms: ["win", "mac"]`, so that those
two queues offer it. Linux owes nothing more.

### What was stale

- The line numbers have moved. The five stores still opened with a bare
  `sqlite3_open_v2(path.string())` at `7eb4aa87`.
- **Consequence 2 never held for a build of this project.** The vendored
  SQLite has been compiled with `SQLITE_DEFAULT_FILE_PERMISSIONS=0600` since
  it was vendored (`third_party/sqlite/CMakeLists.txt`, `1c78e22a`,
  2026-07-31). That is before the first of the five stores existed
  (`f0917d3a`, 2026-08-03). A bare open therefore created each file at
  `0600 & ~umask`, never `0644`. This station's own data root shows it:
  `appearance-settings`, `review-queue` and `workbench-settings`, all created
  by bare opens, are `0600`. What was missing was the second layer, the
  helper's own narrowing, and any contract that the result is `0600`. It
  also means that existing installs are safe. Admission refuses a store file
  with group or other bits, and no existing store file has them.
- The card counted eight stores. Since it was written, two more have been
  added, `release-update` and `update-draft-recovery`, and both open through
  the helper. The epoch catalog, which the card did not count, has its own
  hardened bootstrap. `state/v1` therefore holds eleven store files, and the
  contract covers all eleven.

### What changed (`874dc0ae`)

- **No store is an exception. All five open through
  `open_private_store_database`.** The epoch catalog, which was never one of
  the eight, keeps its own open. It creates its file itself, owner-private
  and exclusive, and opens it through its own VFS, bound to the retained
  directory (`catalog_database::open`). That is stricter than the helper, and
  its file is inside the new contract. The two release stores were not
  changed.
- Each of the four database classes (appearance, workbench, queue and
  ledger) gains an overload that takes the held `state_directory` and a
  store name. It opens through the helper with the store's own flags plus
  `SQLITE_OPEN_NOFOLLOW`, as the other per-profile stores do. The configure,
  establish and validate steps that followed the open are shared by both
  overloads (`adopt`).
- The overload that takes a path stays, because only the schema contracts
  call it. Its comment now says that it applies no private-store policy.
- **The review ledger admits its file before its schema preflight copies
  it.** It then runs the preflight and opens through the helper, so a link
  or a file that is not private is refused before any byte of it is read.
  The preflight also named `-wal`, `-shm` and its clone with
  `path.string()`, which throws on Windows for a root outside the code page.
  Those names are now appended to the path, and the clone is named to SQLite
  in UTF-8.
- The mutation and submission recovery stores open one journal
  (`review-mutation-recovery.sqlite3`) through the same code,
  `open_recovery_journal`. Before, each had its own copy.
- **Every store holds its `state_directory` for as long as its connection is
  open**, as `state_directory.hpp` promises. It is declared before the
  database, so it is released after it. On Windows the retained handles are
  what the helper binds SQLite's held file to. On POSIX, holding them makes
  the promise true, but it does not pin the name. The helper admits, opens
  and restricts by path, and SQLite cannot portably open a file relative to
  a held directory descriptor. On POSIX, other accounts are kept out by the
  privacy of `state/v1` itself (`0700`, owned by the user).
- `private_store_database_failure` maps a refused helper open into the
  database vocabulary. Every one of the five ports already reported
  admission and directory refusals as `unavailable`, and still does.
- No `src/app` file changed, so no composition pin moves.

### The contract: `sqlite_private_store_open` on every platform

`tests/sqlite_contract/private_store_open_tests.cpp` was registered only on
Windows. It is now registered everywhere. Its five Windows courses are
unchanged and stay Windows-only.

- **`non-ansi-root`** (every platform) opens every store factory under a
  root named `bpr-0883-` followed by U+65E5 U+672C U+8A9E and U+1F600. No
  ANSI code page holds U+1F600. The name is written with universal character
  names, so no compiler reads it in its own code page. There are twelve
  factories and eleven files, because the mutation and submission stores
  share one file.
  The course uses each store once, which is when a lazy store creates its
  file. It prints every store's outcome before asserting any, and then checks
  that all eleven files exist.
- **`owner-only`** (POSIX) runs under `umask 0`, so only the stores
  themselves can narrow what SQLite creates. It opens every store, keeps all
  of them open, and requires every `*.sqlite3`, `-wal` and `-shm` file in
  `state/v1` to be a regular file with mode `0600`. It fails on any file
  created at SQLite's stock default mode, `0644`, as the negative control
  shows.

### Verification (Linux: JaegAW, WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2, offscreen)

| Check | Candidate | Result |
| --- | --- | --- |
| `core-only` build (GCC 15.2, `-Werror`) and all of its rows | the change on `c5c172bc`, before the rebase | 158 of 158 pass (12 min 28 s) |
| `dev` build; the 18 `sqlite_*` rows, `test_network_isolation_manifest`, `architecture_boundaries` and `architecture_boundary_contract` | the same | 21 of 21 pass |
| Integration pass: the 164 `application_*` and end-to-end rows, serial, offscreen, `DISPLAY` unset, fresh XDG roots. This is [[BPR-1044]]'s survey set | the same | 161 of 164. The three failures are the ones the `7eb4aa87` baseline already had, with the same markers: [[BPR-1036]], [[BPR-0966]]'s dif-context row, and `application_threaded_reply_contract`. `application_composition_contract` builds a Release application from the working tree, and it passes (523 s). The 33 refusal lines are unchanged ([[BPR-1044]]). |
| After the rebase: `dev` build; the 18 `sqlite_*` rows and the manifest; `live_review_mutation_provider`, `live_review_mutation_restart_recovery`, `live_review_submission_provider`, `live_review_submission_restart_recovery` and `release_update_recovery`; and 15 composed rows that open these stores (the queue, workbench, review-ledger, round-trip and defect composition contracts, `appearance_store_fallback`, and the review-submission, profile-startup, profile-management, release-maintenance, personal-discovery, two-account, window-restart, window-layout and window-review-restoration public paths) | `18fb2ca6` | 39 of 39 pass |
| After the rebase: `core-only` build and the 18 `sqlite_*` rows | `18fb2ca6` | 18 of 18 pass |
| At the pushed commit: `sqlite_private_store_open`, the manifest, and the review-ledger, appearance-fallback and profile-startup courses | `874dc0ae` | 5 of 5 pass: `non-ansi-root:pass`, `owner-only:pass` |
| `format_check.py` with clang-format 22.1.8 on the 16 C++ files touched | the change | passes |
| `tidy.py --paths` with clang-tidy on the 11 `.cpp` files touched | the change | passes. Its 13 warnings are all in code this change did not touch |

`18fb2ca6` and `874dc0ae` are the same change. The second rebase brought
only `src/app/main.cpp` from [[BPR-0902]].

**Negative control: SQLite built at its stock default mode.** A detached
worktree was built at `c5c172bc` as a separate `core-only` tree, with
`CMAKE_C_FLAGS=-USQLITE_DEFAULT_FILE_PERMISSIONS
-DSQLITE_DEFAULT_FILE_PERMISSIONS=0644`. In `compile_commands.json` that
flag follows the target's own `=0600`, so `0644` is the value in effect.
Nothing from the control was committed.

| Product code | Contract | Result |
| --- | --- | --- |
| `c5c172bc` (the five bare opens), with the new contract | `owner-only` | **Fails**: `every store file is 0600, but not:` followed by `appearance-settings`, `review-ledger`, `review-mutation-recovery`, `review-queue` and `workbench-settings`, each at `644` with its `-wal` and `-shm`. The three stores that already used the helper, the two release stores and the catalog are all `600`. |
| The fix applied to the same worktree and rebuilt | `owner-only` | Passes: all 11 files and 22 sidecars are `600` |

So the contract detects a store created at SQLite's default mode, and after
the fix the helper's own narrowing pins `0600` even where the compiled
default would not.

The station records are under `build/logs/bpr-main-linux-20260923/` and
are not committed.

### Where each criterion stands

- **AC1** (every durable store opens through the private helper, or carries
  a recorded exception): met in `874dc0ae`, on every platform, because the
  same code runs everywhere. The five stores open through the helper. The
  epoch catalog's own open is recorded above as the one exception, with its
  reason. Linux has run it.
- **AC2** (a root outside the Windows code page opens the review ledger and
  the recovery journal): the code is in place and the course is registered.
  It is **pending Windows**. On Linux the course passes, but a byte-string
  name cannot fail, so the run proves nothing about this criterion.
- **AC3** (a POSIX contract pins `0600` and fails on a file created at the
  default mode): met on Linux, together with the negative control above.
  macOS runs the same contract and owes one confirmation run.

### What this card still needs

- **Windows (`bpr-main-win`)**, required for AC2. First, the Windows build
  of `874dc0ae` or later: the restructured contract and the five stores
  have not been compiled by MSVC. Then
  `ctest --preset windows-dev -R "^sqlite_private_store_open$" -V`,
  uncontained as registered. Expected: twelve
  `private-store-open:non-ansi-root:<store>:opened=1` lines,
  `private-store-open:non-ansi-root:pass`, the five Windows courses as
  before, and `private-store-open-tests:pass`. Also run the six storage
  contracts, `sqlite_(appearance_settings|workbench_settings|queue_enrollment|review_ledger|review_mutation_recovery|review_submission_recovery)_storage`,
  and `test_network_isolation_manifest`. For a negative control on Windows,
  the same contract against `c5c172bc`'s stores should fail at the first
  bypassing store, `appearance-settings`, because `path.string()` cannot
  spell the root. That is this card's original claim, which Linux cannot
  show.
- **macOS (`bpr-main-mac`)**: the same `sqlite_private_store_open` row at
  `874dc0ae` or later. Expected: `non-ansi-root:pass`, then `owner-only`
  lines for 11 files and 22 sidecars, all `600`, then `owner-only:pass`.
  Also run the same storage contracts.

## The macOS half (bpr-main-mac, Mac batch 5, 2026-09-23)

Run on `0ca739b2`, which contains `874dc0ae`. Apple Silicon, AppleClang,
Qt 6.11.2, `build/dev` Debug, reconfigured and rebuilt first (exit 0, no
`warning:` line).

`ctest --preset dev -R '^sqlite_private_store_open$' -V` passed in 0.48 s,
exit 0:

- `private-store-open:non-ansi-root:<store>:opened=1` for all twelve stores,
  then `private-store-open:non-ansi-root:pass`. As on Linux, a byte-string
  name cannot fail here, so this says nothing about AC2.
- `private-store-open:owner-only:<store>:opened=1` for all twelve, then 33
  `owner-only` file lines, every one `600`: the 11 databases and their 22
  `-shm` and `-wal` sidecars. Then `private-store-open:owner-only:pass` and
  `private-store-open-tests:pass`.

The six storage contracts
(`sqlite_(appearance_settings|workbench_settings|queue_enrollment|review_ledger|review_mutation_recovery|review_submission_recovery)_storage`)
and `test_network_isolation_manifest` passed in one serial run, 7/7. The
one-minute load was 23.6 at the start.

**AC3 on macOS: confirmed.** The contract pins `0600` for every file here,
as it does on Linux. Only the Windows run for AC2 remains.
