---
id: BPR-0452
title: The loopback authority table never learned about the device-flow fixtures
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# The loopback authority table never learned about the device-flow fixtures

`test_network_isolation_manifest` fails at `21e5024`:

```
TEST_NETWORK_ISOLATION_FAILED:native_https_device_authorization_fixture has loopback count 1, expected 0
TEST_NETWORK_ISOLATION_FAILED:github_device_authorization_refresh_contract has loopback count 1, expected 0
```

Found by a full `dev` matrix run during [[BPR-0418]], which is unrelated work.
It is recorded as its own card rather than repaired there, because a change to
a network-authority allowlist is a security decision and not a byproduct of a
Qt slice.

## Why it is failing

`MACOS_LOOPBACK_AUTHORITIES` in `scripts/verify_test_network_isolation.py:153`
is the checked-in record of which tests are permitted to bind loopback, and how
many ports each may take. Anything not in it is expected to take zero. The gate
is doing exactly its job: it fails closed when a test acquires loopback
authority nobody wrote down.

Two tests acquired it and were never written down:

- `native_https_device_authorization_fixture`, declared with
  `LOOPBACK_PORT_COUNT 1` at `tests/github_contract/CMakeLists.txt:242` by
  [[BPR-0278]] (`78e7784`);
- `github_device_authorization_refresh_contract`, same count at
  `tests/github_contract/CMakeLists.txt:268` by [[BPR-0450]] (`a0e547e`).

The table's last edit is `20c6472` ([[BPR-0449]]), which is older than
`a0e547e`. So this has been red since BPR-0450 closed on 2026-08-25 and was not
caught, because a card's focused selectors do not include the isolation
manifest and the last full matrix predates both registrations.

## What this card must decide, not just do

Adding two lines to the table is the likely answer and must not be the starting
assumption. Both fixtures drive
`tests/github_contract/verify_native_https_device_authorization.py`, which is
the same shape as the already-recorded `native_https_transport_fixture` (2
ports). Before recording them, confirm from the script and the fixture that
each takes **exactly one** loopback port, contacts no external endpoint, and
carries no credential — and say so on this card. A count recorded larger than
the fixture needs is a standing permission nobody is using.

## Required final state

- `test_network_isolation_manifest` passes, and the two entries are recorded
  with the counts the fixtures actually take.
- The full network-isolation group passes, including the structure contract and
  the unrelated-endpoint probe.
- This card states what was verified about each fixture's authority, not merely
  that the gate went green.

## Also worth fixing, since it is the reason this was late

Nothing failed the day BPR-0450 landed, because no card's focused selector runs
`test_network_isolation_manifest`. Consider whether registering a loopback test
should require the table entry in the same slice — a CMake-side check, or a
line in the board's verification-scope rule naming the manifest as required
whenever `LOOPBACK_PORT_COUNT` appears in a diff.

## Verification

```sh
ctest --preset dev --output-on-failure -R 'network_isolation'
python3 scripts/verify_test_network_isolation.py --help
```

## Done — 2026-08-25

Both entries are recorded with count 1, which is what each fixture actually
takes rather than what the CMake declaration asserts.

### What was verified about the authority, before recording it

Both tests drive the same script,
`tests/github_contract/verify_native_https_device_authorization.py`:

- **Exactly one port.** `configured_loopback_port` reads
  `BPR_TEST_LOOPBACK_PORTS` and raises `invalid-loopback-port` unless the list
  contains exactly one value in `1..65535` (line 162). There is exactly one
  `bind(...)` call in the file, at `127.0.0.1` (line 407).
- **Loopback only.** A peer whose address is not `127.0.0.1` is recorded as the
  violation `non-loopback-peer` (line 447) rather than served.
- **No outbound request.** The file imports no HTTP client — no `requests`, no
  `urllib`, no `http.client`. Its one `github.com` string is `VERIFICATION_URI`,
  which appears only as a `verification_uri` value inside a JSON body the
  fixture itself serves (line 370). It is never contacted.

So one port each is the true count, not a rounded-up permission.

### The table stays hand-maintained, deliberately

The card asked whether the entry could be required automatically. It should not
be *derived*: the CMake `LOOPBACK_PORT_COUNT` is the claim and this table is the
independent record of what was authorized. A table generated from the thing it
checks checks nothing. That reasoning is now a comment above the table so the
next person does not have to rediscover it.

What was worth fixing is the reminder. The board's verification-scope rule now
names `test_network_isolation_manifest` as required whenever a diff touches
`LOOPBACK_PORT_COUNT`, alongside the composition-selector trap [[BPR-0453]]
found the same day.

### Verification

| claim | command | proves it | result |
|---|---|---|---|
| the manifest is green | `ctest --preset dev -R 'network_isolation'` | manifest, structure contract, external, child, loopback, unrelated, application | 7/7 pass |
| the repair is load-bearing | same, with both entries removed | the manifest fails again naming both tests | **fails without the fix** |
| documentation | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | pass |
