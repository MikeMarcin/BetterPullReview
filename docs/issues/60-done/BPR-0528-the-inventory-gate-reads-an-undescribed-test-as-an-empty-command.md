---
id: BPR-0528
title: The inventory gate reads a test CTest could not describe as an empty command
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# The inventory gate reads a test CTest could not describe as an empty command

## Why

With [[BPR-0525]]'s two selectors registered, the Windows course got past
its own inventory step and failed on the registered `rfc0016_test_inventory`
selector with `github_device_authorization_refresh_contract: command omits
--suite refresh`. The registration carries the argument; CTest's own
listing shows it. Same script, same tree, same arguments: run directly it
passes; run inside the AppContainer, as its registration runs it, it fails.
`bpr-win` measured the listing both ways: plainly, 238 tests all with a
command; contained, the same 238 with fourteen carrying no `command` field
at all — exactly the fourteen uncontained tests whose first token is the
Python interpreter under `%LOCALAPPDATA%`, which a lowbox token cannot
read. `registered_tests` in both inventory scripts substituted an empty
tuple for an absent command, so "CTest did not describe this test" became
an accusation about a registration the listing never saw. Latent since
[[BPR-0446]]: `native_https_transport_fixture` has been among the fourteen
since it was registered on Windows, and nothing compared its empty tuple
until BPR-0525 registered the first affected selector with a
`REQUIRED_ARGUMENTS` entry.

## Required final state

- Both inventory scripts refuse a listing entry with no `command` by the
  test's name (`ctest-inventory-command-absent:<name>`), so an undescribed
  test is never mistaken for one that carries no arguments.
- `rfc0016_test_inventory` and `rfc0017_test_inventory` run
  `WINDOWS_UNCONTAINED`, with the reason beside them, since they shell
  `ctest --show-only` and open no socket.
- The Windows course passes through the inventory selector with all
  seventeen names.

## Repair — 2026-09-02

Both scripts refuse by name; both registrations carry the keyword and the
reason.

| Claim | Command | Result | Commit |
|---|---|---|---|
| Both inventories pass on macOS with the refusal in place | `python3 scripts/verify_rfc0016_test_inventory.py --build-dir build/dev`; `python3 scripts/verify_rfc0017_test_inventory.py --build-root build/dev` | `RFC0016_TEST_INVENTORY_OK`, `RFC0017_TEST_INVENTORY_OK` | this commit |
| The Windows course passes the inventory selector with all seventeen | `bpr-win`'s ledger command from a checkout of this commit | `RFC0016_COURSE_SELECTORS:17`, `RFC0016_PLATFORM_COURSE_OK`, inventory selector passing — recorded below | `a51ca43d` |

## Target-native result — 2026-09-02, station `bpr-win` at `a51ca43d`

Both halves of the repair are measured on the platform that motivated them.

The keyword first, checked directly so the refusal is named rather than
trusted:

```text
1/2 Test #10: rfc0017_test_inventory ...........   Passed    0.19 sec
2/2 Test #12: rfc0016_test_inventory ...........   Passed    0.18 sec
100% tests passed out of 2
Label Time Summary:
bpr_windows_uncontained    =   0.37 sec*proc (2 tests)
```

Both carry `bpr_windows_uncontained`, both pass, and neither prints
`ctest-inventory-command-absent` — which is the point: with the listing
complete, the new refusal has nothing to report.

Then the course, from a build root deleted whole:

```text
RFC0016_COURSE_SELECTORS:17
RFC0016_PLATFORM_COURSE_OK
```

with `rfc0016_test_inventory` passing inside the matrix. Full markers and the
artifact are on [[BPR-0525]], whose closing row this shares.

### The refusal was not exercised here, and that is the honest reading

This run proves the keyword: the interpreter is readable, CTest describes
every test, and the inventory sees real commands. It does **not** exercise
`ctest-inventory-command-absent`, because nothing now produces an absent
command on this host. The refusal's value is that the next listing failure
says so by name instead of arriving as a false accusation about a
registration — which is exactly how this defect presented, costing four
measurements to see through. That claim is proven by construction and by the
contract on `main`, not by this run, and the row is recorded as the keyword's
evidence rather than the refusal's.

Worth keeping with the card: the substitution it removed had been silently
wrong on this host since [[BPR-0446]]. `native_https_transport_fixture` was
among the fourteen tests whose command the AppContainer emptied, and its empty
tuple was never compared to anything because it declares no required
arguments. The gate was reading undescribed tests as argument-free ones for
months and only said so when [[BPR-0525]] registered the first affected
selector that declares one.

The card is terminal.
