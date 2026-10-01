---
schema: "implement-rfc.card.v1"
id: "BPR-1257"
title: "Linux: sqlite_epoch_catalog_storage_contract deadlocks on its own flock of state/v1"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux triage of the BPR-0905 Linux full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1255--BPR-1258--f86522aa8587.json","related_cards":["BPR-0053","BPR-0905"]}
acceptance: [{"criterion":"The row passes on Linux, alone and in a parallel run","outcome":"pass","revision":"6617931a1","evidence":{"kind":"command","detail":"bpr-main-linux, WSL2, GCC 15.2, build/dev: ctest --preset dev -R '^sqlite_epoch_catalog_storage_contract$' passed in 0.30 s (was a timeout at 30 s, and a hang of over 400 s run directly); ctest --preset dev -R '^sqlite_' -j10 passed 24 of 24. Negative control without the historical release: FAILED: every catalog and session on the root is released before it reopens, in 0.10 s."}},{"criterion":"The card names the cause and the layer fixed","outcome":"pass","revision":"6617931a1","evidence":{"kind":"observation","detail":"The card's Cause and fix section: the test layer. The historical session from 5ec14834d was not released before the reopen. On Linux catalog_database_platform_state keeps the version descriptor and its flock for the database's lifetime, so the reopen's blocking flock waited on its own process. macOS drops the lock when open returns. Fixed in the test, with a non-blocking Linux probe that fails by name."}},{"criterion":"macOS and Windows keep their path, or the change is run there","outcome":"pass","revision":"6617931a1","evidence":{"kind":"freeform","detail":"Test-only change. On macOS and Windows it only releases a session earlier, before the reopen. The probe and its includes are compiled only under __linux__. Not run on macOS or Windows; the integrator can request a run."}}]
---

# BPR-1257: Linux: sqlite_epoch_catalog_storage_contract deadlocks on its own flock of state/v1

Found by bpr-main-linux on 2026-10-01: WSL2 Linux 6.18, GCC 15.2, `build/dev`
Debug, at `db021262a`. bpr-main-win's BPR-0905 full suite reported the row as
`***Timeout` at 30 s under -j10.

## Observation

This is not contention:

- Alone under ctest, at load average 0.5, it times out at 30 s.
- Run directly with `timeout 400`, it is still running at 400 s, at 0.1% CPU,
  in `locks_lock_inode_wait`.
- CTestCostData records an average of 0 s for the row, which fits a row that has
  not passed here for some time.

While it hangs, inside `bootstrap_authority_and_storage_contract` (root
`provider-hostname-url-json-login-body-token-canary`), `/proc/locks` shows the
test process holding `FLOCK WRITE` on the `state/v1` directory inode and also
waiting for it:

```
11: FLOCK  ADVISORY  WRITE 287059 00:4a:535515 0 EOF
11: -> FLOCK  ADVISORY  WRITE 287059 00:4a:535515 0 EOF
```

`epoch-catalog.sqlite3` is open twice (fds 6 and 9). So a second store opens
on the root while the first is still alive. A store holds `flock(LOCK_EX)` on
`state/v1` for its whole lifetime, through the blocking loop in
`src/adapters/sqlite/src/catalog_database.cpp` (about line 4663). A second
open file description of the same inode in the same process then waits
forever.

## Scope

- Find which object keeps the first store alive when the second opens, and
  whether that is the test's error or the product's.
- Fix it at the right layer, and pin it so a reintroduced self-wait fails
  quickly by name, not at the timeout.

## Acceptance

- The row passes on Linux, alone and in a parallel run.
- The card names the cause and the layer fixed.
- macOS and Windows keep their path, or the change is run there.

## Cause and fix (bpr-main-linux, 2026-10-01)

**Cause: the test.** `bootstrap_authority_and_storage_contract` opens a
`historical` session (added by `5ec14834d` on 2026-09-25 for cached review
reading). Before it reopens the root, it releases `secondRepository`,
`secondPr`, `same`, `first` and `port`, but not `historical`. On Linux,
`open_catalog_database` moves the root, state and version directory
descriptors into `catalog_database_platform_state`
(`src/adapters/sqlite/src/catalog_database.cpp`, about line 4744). The version
descriptor carries the `flock(LOCK_EX)` taken at about line 4663, so the lock
lives as long as the database. A live session therefore keeps `state/v1`
locked. The reopen's `make_epoch_catalog` opens the directory again, which is
a second open file description, and its blocking `flock` waits on this same
process forever.

**Why macOS passes.** The macOS branch returns `catalog_database{ database,
finalPath }` and keeps no directory descriptors. Its `flock` ends when
`open_catalog_database` returns, so the leftover session holds no lock there.
The comment above `windows_sibling_store_root_rehold_contract` says that "a
store there [POSIX] holds `flock(LOCK_EX)` on `state/v1` for its whole
lifetime". That is true on Linux and not on macOS. This card leaves the
platform difference as it is and only records it. Whether macOS should hold the
lock for the store's lifetime is a separate question.

**Fix (`6617931a1`, test only).**

- The course releases `historical` with the other sessions.
- A Linux-only probe, `catalog_root_released`, takes a non-blocking
  `flock(LOCK_EX | LOCK_NB)` on `state/v1` just before the reopen. A session
  left alive now fails the course by name, `every catalog and session on the
  root is released before it reopens`, in about 0.1 s instead of hanging. No
  product code changed.

| Check | Result |
|---|---|
| Before: direct run, `timeout 400` | Hung 400 s in `locks_lock_inode_wait`; `/proc/locks` held and waiting on the same `state/v1` inode |
| After: direct run | `sqlite-epoch-catalog-storage-tests:pass`, 0.30 s |
| Negative control: the `historical` release removed | `FAILED: every catalog and session on the root is released before it reopens`, 0.10 s, no root left behind |
| `ctest --preset dev -R '^sqlite_epoch_catalog_storage_contract$'` | Passed, 0.30 s |
| `ctest --preset dev -R '^sqlite_' -j10` | 24 of 24 pass |
| `/usr/bin/clang-format` 22.1.8 `--dry-run --Werror` on the file | Clean |
| `tidy.py --paths` on the file | No finding on a changed line. Two existing `readability-identifier-naming` findings at lines 909 and 915 (`retained_version`, `replacement_catalog`) are not this card's. |

macOS and Windows were not run. Their path changes only by releasing a session
earlier in the course. The probe is compiled only on Linux.
