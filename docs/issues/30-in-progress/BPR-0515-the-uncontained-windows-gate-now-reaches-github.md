---
id: BPR-0515
title: Enabling Windows made the uncontained device-authorization gate reach GitHub on every dev-tree run
status: In Progress
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

## Measured on the Windows station — 2026-09-02

`bpr-win`, read-only at `windows-lane` `4627b6e1`, unelevated (no firewall
rule or packet capture available), sampling the process's TCP table every
20 ms for its whole lifetime and diffing the DNS client cache around the
run. Recorded by the integrator from the reply.

| Claim | Measurement |
|---|---|
| What leaves the host per run | Exactly one outbound connection, to `github.com:443` (DNS cache gained `github.com` → the same address); no `api.github.com` resolution and no second connection. Sampling at 20 ms cannot count HTTP requests inside one TLS session, only connections |
| A device code is minted per run | Yes, by code path rather than inference: the probe dispatches cancel only from `awaiting_authorization`, which is reached only in the device-code completion handler from `requested`, so `cancelled` implies GitHub answered with a code every run |
| Request paths, rate-limit headers, client identity on the wire | Not observable without MITM; from source, `/login/device/code` and `/login/oauth/access_token` with the embedded distributable client, which the enabled artifact is known to contain |
| Egress denied (per-user system proxy at `127.0.0.1:1`, a closed port) | `DEVICE_FLOW_STAGE:failed`, `DEVICE_FLOW_REQUESTS:0`, `DEVICE_FLOW_REGISTERED_SELECTION_OK`; the gate's regex on `cancelled` misses and the selector fails in 4.84 s against 1.13 s unblocked, so something waits before giving up |
| Host restored | Proxy keys back to their prior values under try/finally; no firewall rule; `Test-NetConnection github.com -Port 443` true afterwards; tree untouched |

Two findings beyond the measurement. `application::github_proxy_mode` has
exactly one enumerator, `system`: there is no per-profile, per-process, or
environment proxy override anywhere in the application, so a test cannot be
pointed at a local denial without changing the application. And the only
unelevated way to deny egress is the per-user system proxy, which for about
five seconds pointed every application in that logged-in session at a closed
port — a reason not to script this measurement into a routine on a shared
host.

On containment: [[BPR-0362]]'s reasoning stands — the test needs the
`\GLOBAL??\C:` link, the mount-point manager, the volume device, and the
pipe namespace, which a lowbox token cannot reach — so the AppContainer stays
off the table for it. A denial at the socket sidesteps the token entirely,
and a deterministic local denial lands on exactly `failed`, which is already
the macOS and Linux expectation: a shim would unify all three platforms on
one gate value rather than needing a Windows-specific one.

The station's reading of the three options, for the owner: (a) a proxy or
endpoint override the harness can set per process — small, and the only one
that returns Windows to `failed` beside the other two; (b) an admin-scoped
firewall rule per executable, needing elevation the station lacks and CI
would have to grant; (c) accept the exemption with the network-dependent
expectation and this cost written down. The integrator's recommendation is
(a) confined to the tests-enabled fixture profile, so the tests-disabled
Release carries no proxy override and the RFC-BPR-0017 fixture-seam
inventory keeps proving its absence.

## Decided — 2026-09-02

The product owner chose the first option, in full: "Fixture proxy override."
The test gains a per-process proxy override that only the tests-enabled
fixture profile can set, pointing the connect at a closed loopback port, so
the Windows dev-tree gate is denied locally and its expectation becomes the
`failed` macOS and Linux already record; the tests-disabled Release carries
no override, and the RFC-BPR-0017 fixture-seam inventory keeps proving its
absence. Claimed by the integrator into `30-in-progress/` and split as the
board's rule requires: the override, the gate expectation, and the host
reproduction are generic work done here; the Windows measurement that the
gate now lands on `failed` with zero connections is `bpr-win`'s, dispatched
once the code is on main.
