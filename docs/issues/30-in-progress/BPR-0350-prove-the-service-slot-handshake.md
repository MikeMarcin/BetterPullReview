---
id: BPR-0350
title: Prove the SERVICE_SLOT handshake mechanism before anything is built on it
status: In Progress
rfc: RFC-BPR-0008
depends_on: []
---

# Prove the SERVICE_SLOT handshake mechanism before anything is built on it

[[BPR-0262]] fact_3 and fact_4 rest on a Mach mechanism that appears nowhere in
this tree. `SERVICE_SLOT`, `mach_ports_register`, `audit_token`, and
`bootstrap_*` have zero occurrences outside the operator packet, so every later
slice would be building on an assumption.

The card named the assumption honestly: the capsule is signed
`codesign --force --sign - --timestamp=none`, with no hardened runtime and no
entitlements, and "nothing in the tree establishes whether Mach port
registration and audit-trailer validation also [work] under that signing."

This card establishes it, as a checked-in contract rather than a one-off probe.

## What the mechanism is

`SERVICE_SLOT` is `2` in `<mach/mach_init.h>` — an index into the task's
registered-port array, not a launchd service name. Stage one registers a send
right at that index, the suspended child recovers it with `mach_ports_lookup`,
and the parent restores its exact prior snapshot immediately afterwards. The
child's first message carries a kernel audit trailer, from which the parent
identifies the peer with Security.framework — never from a pid.

## Required final state

- A macOS-only contract proves, in one hermetic run with no network and no live
  service: the slot is initially empty; registration succeeds; the right
  survives `POSIX_SPAWN_START_SUSPENDED` plus `exec`; the child recovers it;
  the parent's restored snapshot leaves the slot empty again; the child's
  message carries an audit trailer whose token names the child exactly; and
  `SecCodeCopyGuestWithAttributes` keyed by `kSecGuestAttributeAudit` returns a
  guest whose `kSecCodeInfoUnique` equals the on-disk binary's.
- The fixture is signed the same ad-hoc way the capsule is, so the contract
  answers the question actually asked rather than a friendlier one.
- Fail-closed halves are proven, not assumed: an occupied slot is refused, and
  a peer whose code identity does not match is refused.
- The contract runs in the default graph on macOS and registers nothing on
  other platforms. It grants no live, credential, package, or publication
  authority and touches no approval.

## Non-goals

- No disk image, no mount, no capsule change. Fact_1 and fact_2 are separate.
- No change to `src/app`, `main.cpp`, or the approval subject. Those are
  [[BPR-0351]] and [[BPR-0352]], and changing `src/app` would drift three pinned
  digests and a file list in
  `tests/end_to_end/application_review_round_trip_composition_contract.cmake`
  for no reason.
- Do not wire the mechanism into the product. This card proves it exists.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure -R '^macos_service_slot_handshake$'
```

Each assertion must fail if the property it names is removed.
