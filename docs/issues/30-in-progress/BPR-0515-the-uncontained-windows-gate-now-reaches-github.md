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

## The override landed — 2026-09-03

Integrated at `41b6bb8b` with the card open; the Windows measurement that
closes it is `bpr-win`'s.

**One flag, one existing seam.** `--network-denied` is parsed only inside
`main.cpp`'s `BPR_BUILD_TESTING` block and lands in
`github_application_config::NetworkDenied` (an unconditional field, because
the liveness test links the composition without that define and the config
crosses by value; every line that reads it is guarded). When set, the
device-authorization probe composes
`make_native_https_credential_free_transport_for_test` with the adapter's
existing `proxy_policy::fixed_test` pointed at `127.0.0.1:1` — a closed
port, offered as the only candidate, with no direct route and no fallback —
instead of the production twin. No new proxy mode was added: the
application's `github_proxy_mode` cannot carry a macro-guarded enumerator
across libraries that disagree about `BPR_BUILD_TESTING`, and the adapter's
policy already does exactly this and is already proven absent from a shipped
build by name. The composition library now defines
`BPR_NATIVE_HTTPS_TESTING` under `BUILD_TESTING` to see that factory, a
seam widening the boundary gate accepts and the RFC-BPR-0017 inventory keeps
honest: `--network-denied` is now a forbidden string there, and the shipped
image carries it zero times.

**Both dev-tree device rows use it.** `application_device_authorization_unavailable`
and `_release_contract` pass `--network-denied` and expect
`DEVICE_FLOW_STAGE:failed`, `DEVICE_FLOW_REQUESTS:0`,
`DEVICE_FLOW_REGISTERED_SELECTION_OK` on macOS, Linux, and Windows alike;
an unregistered platform keeps `unavailable`. The three-branch
`bpr_device_selection_self_test_expression` — `failed` here, `cancelled`
there — is retired, and the comment names the one mechanism. `_public_path`
is untouched, `WINDOWS_UNCONTAINED` stays on both rows (BPR-0362's drive
letter), and neither inventory needed a required-argument change.

**Load-bearing where it can be seen.** On this host the gate lands on
`failed` with zero requests in .892 s whether or not the flag is
passed, because the harness's sandbox denies the connect regardless; the
worker made the flag a no-op and the gate still passed here, and said so.
The flag's effect is observable on Windows, where nothing else denies, and
that is the measurement this card waits for.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| Every device-authorization row, the two shipped-selection preview rows, both inventories and their contract, the manifest gate, the composed-host audit, and both composition contracts | `ctest --preset dev -R '^(application_device_authorization_(unavailable\|release_contract\|public_path\|registered_course\|browser_handoff\|browser_absent\|panel_course\|panel_absent)\|application_anonymous_public_preview_(unavailable\|release_contract)\|rfc0016_test_inventory\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|test_network_isolation_manifest\|composed_application_hosts\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 17/17 passed | this commit |
| The architecture boundaries hold with the widened seam | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the touched files | exit 0, no output | this commit |
| The shipped image knows neither the flag nor the seam | `cmake --build --preset app-release`; the RFC-0017 inventory's `--all --release-binary` scan with `--network-denied` forbidden; `strings` on both images | `RFC0017_TEST_INVENTORY_OK`; 0 occurrences in the release image, present in the tests-enabled one | this commit |
| The gate with the flag on this host | `better_pull_review --self-test --device-authorization-probe --network-denied --automated-run-budget-ms 20000` offscreen | `DEVICE_FLOW_STAGE:failed`, `DEVICE_FLOW_REQUESTS:0`, `DEVICE_FLOW_REGISTERED_SELECTION_OK` in .892 s | this commit |
| The pins bind the formatted composition | two-pass repin, a third after a header reword the composition contract demanded | `050a8340…98f03`, `bc219d30…cbf366` | `41b6bb8b` |
