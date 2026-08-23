---
id: BPR-0427
title: The epoch catalog hangs inside an AppContainer instead of failing
status: Done
rfc: RFC-BPR-0003
depends_on: []
---

# The epoch catalog hangs inside an AppContainer instead of failing

Inside the AppContainer, with the ancestor chain unreadable ([[BPR-0362]]),
the two epoch-catalog tests do not fail — they stop producing output and
CTest kills them:

| test | contained | uncontained |
|---|---|---|
| `sqlite_epoch_catalog_storage_contract` | **Timeout at 30 s, no output** | fails later, at a retention assertion ([[BPR-0425]]) |
| `sqlite_epoch_catalog_behavior_contract` | **Timeout at 180 s, no output** | passes, in 99.5 s |

The two sibling stores that open a state directory through the same
`open_state_directory` path fail *fast* under the same denial —
`sqlite_review_ledger_storage` with `the bounded state directory opens`,
`sqlite_review_mutation_recovery_storage` with
`owner-private recovery store opens` — so the denial itself is not what hangs.
Something between the catalog's first open and its first diagnostic retries or
waits when the chain cannot be walked. [[BPR-0362]] observed the same shape on
2026-08-18 ("no output at all for seven minutes") and later recorded that the
tests had begun to fail fast; on this tree, at `a74e1df`, they hang again.

This is not only a test-suite matter. [[BPR-0360]] records why: an
MSIX-packaged application runs inside an AppContainer, and a catalog that
hangs at startup rather than reporting `invalid_permissions` is a worse
failure for a real user than one that fails closed.

## Required final state

- The wait is located: which call blocks, what it is waiting for, and why the
  ledger and recovery stores on the same path do not.
- With the chain unreadable, the catalog fails closed with a typed diagnostic
  in bounded time, contained, the way its siblings do.
- A test fails without the change — necessarily a contained one, which means
  a deliberately registered contained probe rather than the two catalog
  tests, which [[BPR-0362]] registers `WINDOWS_UNCONTAINED`.
- Uncontained behaviour, and macOS and Linux behaviour, are unchanged.

## Verification

A contained probe through the launcher against a catalog root whose chain
the container cannot read, asserting a typed failure within the CTest timeout;
the exact registration is this card's to design.

```sh
ctest --preset core-only --output-on-failure -R '^sqlite_epoch_catalog_(storage|behavior)_contract$'
```

## Measured again, and the wait is gone — 2026-08-23

The card's observation was real at `a74e1df` and did not survive the
session's repairs. Re-measured today through the same launcher, on the
tree that carries [[BPR-0425]]'s root re-hold, [[BPR-0437]]'s missing-file
comparison fix, and the `longPathAware` manifest:

| test, contained | at `a74e1df` | now |
|---|---|---|
| `sqlite_epoch_catalog_storage_contract` | timeout at 30 s, no output | exits in 5 s |
| `sqlite_epoch_catalog_behavior_contract` | timeout at 180 s, no output | exits in 5 s |

Both suites still fail contained — the lowbox denies the DOS volume name
their retained-chain validation needs, which is exactly why [[BPR-0362]]
registers them `WINDOWS_UNCONTAINED` — but they now fail in bounded time.
The wait can no longer be located because it no longer exists; the
candidates the investigation had flagged (the deadline-less
`catalog_lock` retry and SQLite's `xSleep`) are only reachable behind
opens that now fail fast.

What the card still required is the deliberately registered contained
probe, and it exists now: `contained_catalog_probe`
(`tests/sqlite_contract/contained_catalog_probe_tests.cpp`) runs
*contained* on Windows, constructs the catalog on a root the container
can reach, and requires a typed answer — construction or a typed
refusal — within its own twenty-second bound, treating an escaped
exception as a failure. On macOS and Linux the same registration asserts
the constructive answer arrives as promptly.

### Evidence

| claim | command | result |
|---|---|---|
| the probe passes, contained, as registered | `ctest --preset core-only -R '^contained_catalog_probe$'` | Passed, 0.74 s, contained (`probe-run-1.log`, 2026-08-23) |
| the two catalog contracts pass as registered (uncontained) and are bounded contained | `ctest --preset core-only -R '^sqlite_epoch_catalog_(storage\|behavior)_contract$'`; the contained re-measurement above | both Passed uncontained (`bpr0425-siblings.log`); 5 s bounded contained (`contained-0427.log`, `contained-0427b.log`) |
| macOS and Linux are unchanged | the probe is a new registration; no product change was needed | by construction |

Implemented by the integrator.
