---
schema: "implement-rfc.card.v1"
id: "BPR-1257"
title: "Linux: sqlite_epoch_catalog_storage_contract deadlocks on its own flock of state/v1"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux triage of the BPR-0905 Linux full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1255--BPR-1258--f86522aa8587.json","related_cards":["BPR-0053","BPR-0905"]}
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
