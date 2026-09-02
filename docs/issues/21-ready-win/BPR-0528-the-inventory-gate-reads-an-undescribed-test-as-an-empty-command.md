---
id: BPR-0528
title: The inventory gate reads a test CTest could not describe as an empty command
status: Ready
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
| The Windows course passes the inventory selector with all seventeen | `bpr-win`'s ledger command from a checkout of this commit | **Pending** — target-native, the closing row shared with [[BPR-0525]] | Pending |
