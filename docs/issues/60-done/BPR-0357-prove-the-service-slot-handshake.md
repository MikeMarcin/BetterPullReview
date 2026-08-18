---
id: BPR-0357
title: Prove the SERVICE_SLOT handshake mechanism before anything is built on it
status: Done
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

## Result — 2026-08-17

`macos_service_slot_handshake` is registered in the default graph on macOS and
nowhere else. It links no product library and adds no file under `src/app`, so
none of the three pinned digests or the file list in
`application_review_round_trip_composition_contract.cmake` moved.

The mechanism works on the footing that was actually in doubt. The linker
already ad-hoc signs on arm64, but it sets the linker-signed bit, which is not
what the capsule carries. A `POST_BUILD` step re-signs with the exact
`codesign --force --sign - --timestamp=none` from
`scripts/designated_live_macos_capsule.py:511`, and the built contract reports
`flags=0x2(adhoc)` — the same shape as the capsule, with no hardened runtime and
no entitlements.

What the contract proves in one hermetic run: the slot starts unused; a receive
right can be allocated and published there; the published right survives
`POSIX_SPAWN_START_SUSPENDED` and `exec`; the parent restores its exact prior
snapshot and the slot reads unused again for every later spawn; the child's
first message is the exact fixed greeting; the kernel supplies a full audit
trailer naming the exact child; `SecCodeCopyGuestWithAttributes` keyed by
`kSecGuestAttributeAudit` returns a guest that passes `SecCodeCheckValidity` and
whose `kSecCodeInfoUnique` equals the on-disk image's; and the child accepts only
the exact fixed reply on the send-once right it supplied.

Fail-closed halves are courses, not comments. A child that inherits an empty
slot refuses rather than reaching for argv, environment, or its parent's pid. An
identity checked against a pinned requirement it cannot satisfy is refused. An
occupied slot is detected rather than overwritten, and cleared again so the
refusal leaves no residue.

### The assertions fail when their properties do

| Mutation | Result |
|---|---|
| Snapshot restore skipped after spawn | fails "the restored snapshot leaves SERVICE_SLOT unused for every later spawn" |
| Child returns success on an empty slot instead of refusing | fails "a child that inherits no published right refuses rather than falling back" |
| Child sends a greeting other than the fixed one | fails "the child's first message is the exact fixed greeting" |

### One defect the negative pass found in this contract

The first mutation run hung the runner rather than failing it. Children are
spawned suspended, so an assertion failing between the spawn and the resume left
a stopped child nobody would ever reap, and `ctest` waited on it. Every spawned
child is now tracked and killed on the failure path, and the same mutation now
exits 1 in well under a second. A test that can hang the graph when it fails is
worse than the gap it closes, and this one would have shipped that way.

### Gates

`macos_service_slot_handshake` passed three consecutive runs. The wider set —
`architecture_boundaries`, `architecture_boundary_contract`,
`live_mutation_isolation`, `live_mutation_isolation_contract`,
`test_network_isolation_structure_contract`, `test_network_isolation_manifest`,
`designated_live_macos_capsule_contract` — passed 8/8; the new target raises the
isolation sweep's non-production executable count from 101 to 102 and nothing
pins that number. The full `dev` graph builds, `coding-format-check` passes
across 375 files, `verify_documentation.py` returns `DOCUMENTATION_OK`, and
`git diff --check` is clean.

### Scope

No disk image, no mount, no capsule change, no `src/app` edit, no approval
touched, and no live, credential, package, or publication authority is granted
or implied. [[BPR-0351]] through [[BPR-0354]] own the rest of [[BPR-0262]].
