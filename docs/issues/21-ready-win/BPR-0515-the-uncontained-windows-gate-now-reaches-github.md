---
id: BPR-0515
title: Enabling Windows made the uncontained device-authorization gate reach GitHub on every dev-tree run
status: Ready
rfc: RFC-BPR-0016
depends_on: [BPR-0311]
---

# Enabling Windows made the uncontained device-authorization gate reach GitHub on every dev-tree run

## Why

`application_device_authorization_unavailable` carries `WINDOWS_UNCONTAINED`:
[[BPR-0362]] exempts it from the AppContainer because a lowbox token cannot
give it what it needs, so on Windows it runs labelled
`bpr_windows_uncontained` with nothing in front of the network. Before
[[BPR-0311]] that cost nothing — the capability was off and the refusing
transport answered locally. After the flip the real flow starts, reaches
`awaiting_authorization` against GitHub, and the probe's budget cancels it
there. `bpr-win` measured the registered Windows expectation as `cancelled`
for exactly that reason and wrote the mechanism into
`tests/end_to_end/CMakeLists.txt`, explicitly not borrowing macOS's
determinism claim: Windows is the one platform whose gate expectation depends
on reaching GitHub, and an offline host lands `failed` and fails the gate.

That posture arrived as a side effect of the flip rather than as anything the
flip chose, so it is adjudicated here on its own.

A correction to a shared reading, recorded so it is not repeated:
`DEVICE_FLOW_REQUESTS:0` counts submissions that reached the refusing
transport, never HTTP requests. Zero proves the capability gate held and is
fully consistent with a real device-code request having gone out. On macOS
and Linux the isolation makes the two readings coincide; on Windows they come
apart.

## Required final state

One of the three, written down with its cost, and the gate's behaviour on an
offline Windows host stated either way:

- the test gains a Windows containment path so the connect is denied locally
  and the expectation becomes deterministic, or
- the registered Windows expectation moves to a selector that is allowed to
  touch the network and is labelled as such, leaving the dev-tree gate
  offline-deterministic, or
- the exemption is accepted, with the exact request that leaves the host
  (endpoint, client identity, what GitHub mints per run) and its rate cost
  recorded, and the offline failure named as expected behaviour.

## Non-goals

- Re-flipping Windows. The selection stands; this is about the gate.
- [[BPR-0285]]'s final-head replay, which runs the platform course rather
  than the dev-tree gate.

## Verification

On the Windows station first: capture what leaves the host during one run of
`ctest --test-dir build/windows-dev -C Debug -R '^application_device_authorization_unavailable$'`
(the request path and host, the client identity presented, whether GitHub
mints a device code, and the rate-limit headers consumed), and run the same
selector with outbound network blocked to record the exact failing line. That
measurement is the input to the owner's choice above; the chosen outcome is
then verified by the selector under the condition it claims.
