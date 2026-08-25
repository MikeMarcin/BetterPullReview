---
id: BPR-0281
title: Compose the production device-authorization path
status: Done
rfc: RFC-BPR-0016
depends_on: [BPR-0279, BPR-0280]
---

# Compose the production device-authorization path

## Required final state

- The tests-disabled shipped application composes the visible device choice
  and explicit per-platform capability boundary without a client identifier;
  it returns `device_flow_unavailable` before request construction. The tests-
  enabled recorded course composes the private verification client and reaches
  the existing profile enrollment/vault path.
- Production macOS, Linux, and Windows all remain visibly
  `device_flow_unavailable` with zero requests in this slice. BPR-0282 owns the
  complete macOS native/security proof while unavailable; BPR-0318 separately
  owns a distributable identity, the macOS enablement, and the final rerun.
- BPR-0310 and BPR-0311 respectively own the same-slice Linux and Windows
  production enablement plus complete target-native rerun. Build target or card
  folder movement alone never enables the capability.
- Unregistered builds and every GHES profile keep the visible choice but return
  typed `device_flow_unavailable` before constructing or sending a request.
- Manual token enrollment remains functional before and after device success.
- Fixture factories and testing helpers are absent from Release; the public-path
  contract proves exact composition rather than linked symbols or helper calls.

## Inherited from done BPR-0279 — 2026-08-24

The host slice deliberately shipped ports without two production adapters
this card now owns supplying: the interruptible production timer adapter
behind `device_authorization_timer_port` (the host owns the schedule and its
bounds; the real timer needs runtime machinery only composition can verify),
and the bridge that hands the host its transport through the production
`make_native_https_credential_free_transport` factory — the adapter depends
on the application layer, so the bridge lives here, not in `src/application`.
The refresh wire adapter is separately [[BPR-0450]].

## Narrowed by done BPR-0450 — 2026-08-25

The production `device_authorization_transport_port` implementation is no
longer this card's. [[BPR-0450]] needed the host's refresh lifecycle to run
against a real adapter, and the host calls acquire and exchange on the way
there, so a refresh-only implementation of that port could not exist:
`github_device_authorization_transport` ships complete in
`src/adapters/github/`, constructed by
`make_github_device_authorization_transport` from a
`make_native_https_credential_free_transport` transport, a client identifier,
and an `effect_executor`.

What remains here is unchanged in substance: constructing that transport in
composition and handing it to the host, the production interruptible timer
adapter behind `device_authorization_timer_port`, the visible device choice,
and `device_flow_unavailable` on every platform until [[BPR-0318]].

## Implemented and closed — 2026-08-25

### The capability is a compile-time constant, and it defaults to refusing

`device_authorization_capability` is one bool on the host, supplied by
composition, and it carries no client identifier — the host does not need the
value to know whether one exists, and a value that never enters the host cannot
leak from it. It defaults to **unregistered**, deliberately: every shipped build
is unregistered until [[BPR-0318]], and a capability defaulting to available
would make forgetting to compose one look like enabling the flow. That default
cost a one-line update to two test harnesses, which is the right price.

When it is false the host returns before `Transport->acquire_device_code` is
reached, so "zero requests sent" is structural rather than a count that happens
to be zero. The refusal is posted to the decision lane rather than applied
inline, because it runs inside the transition that produced the request, and it
re-checks the flow identity so a superseded flow cannot be refused on a newer
one's behalf.

`shipped_device_authorization_capability()` is the single place that decides.
macOS, Linux, and Windows all read `{ .Registered = false }` from it. Moving a
card between folders does not enable a capability and neither does building on
a different operating system; [[BPR-0310]], [[BPR-0311]], and [[BPR-0318]] each
flip that constant in their own slice and rerun their own course.

### What composition now owns

The production timer, `qt::event_loop_device_timer`: one `QTimer`, single-shot,
precise, restarted rather than accumulated, because the port promises at most
one live delay and two would race for the same poll. `cancel` stops it without
firing, which is what makes a cancelled flow stop polling immediately rather
than at the next interval boundary. Its expiry runs on the thread that owns it,
which composition makes the decision lane's thread, so the callback reaches the
host already serialized.

`unregistered_device_authorization_transport` satisfies the port's reference on
a build with nothing to talk to. It is not a fixture and not a stub for a real
transport: it is the honest shape of "no client to authenticate as", every
method is unreachable behind the capability gate, and each one counts being
reached and answers a typed error rather than succeeding quietly. The public
self-test reports that count, so a bypassed gate is visible rather than silent.

The device host is composed beside the GitHub host, sharing its vault, its
decision lane, and its enrollment boundary, and it joins the shutdown order and
the quiescence poll. No browser capability is composed in this slice: a build
that cannot start a flow has nothing to open, and the host reports `unavailable`
for the handoff rather than inventing an address.

### The three ledger selectors

`application_device_authorization_public_path`, `..._unavailable`, and
`..._release_contract` are registered exactly as the RFC-BPR-0016 final command
ledger allocates them, driving the shipped binary through a new
`--device-authorization-probe` alongside the existing `--self-test`. Three rows
rather than one, because "the choice exists", "GHES refuses it", and "the whole
shipped chain refuses it silently" fail for different reasons and a combined row
would hide which. The probe prints four separate facts —
`DEVICE_FLOW_HOST_COMPOSED`, `DEVICE_FLOW_PRESENTED`, `DEVICE_FLOW_STAGE`,
`DEVICE_FLOW_REQUESTS` — before its verdict, for the same reason.

Wiring the probe exposed a real defect in the run loop: the device wait
re-entered the CLI presentation check, which counts the profiles it passes, and
`CLI_UNAVAILABLE_COUNT` went from 1 to 2. The wait moved to the top of the
`connect` stage, ahead of the counting check. `application_public_path` and
`application_public_path_ghes_cli_unavailable` are unchanged and still green.

### Narrowing recorded rather than absorbed

Done [[BPR-0450]] shipped the production `device_authorization_transport_port`
implementation, because a partial port could not exist. That is recorded on both
cards. What remained here — the timer, the bridge's composition, and the
selection — is what landed.

### Evidence

17/17 across the complete RFC-BPR-0016 selector set plus `application_public_path`.
`cmake --build --preset app-release` exit 0. Release binary inspection: **zero**
`github_public_path_fixture` symbols against 6,746 in the dev binary, so the
fixture seam is genuinely absent rather than merely unused; 400
device-authorization symbols present, so the path is compiled in rather than
linked away; zero client-identifier-shaped strings, so no registered identity is
embedded. Format, `check_boundaries`, and `verify_documentation.py` clean.

Two falsification probes, each reverted: flipping
`shipped_device_authorization_capability()` to registered failed two of the
three selectors; skipping the host construction failed all three.

### Two digest pins and one card that is not this card's

`application_composition_contract` pins the LF-normalized digest of
`github_application.cpp` and a manifest digest over the whole production
`src/app/` source set, so that no composition edit lands without being noticed.
Both drifted, as they are designed to; both are re-pinned here in the same
commit as the edit that moved them, which is the only way the pin means
anything.

Running that suite also surfaced something that belongs to neither this card
nor [[BPR-0282]]. RFC-BPR-0017's guard,
`verify_rfc0017_test_inventory.py --release-binary`, fails on the shipped
artifact:

```
rfc0017-test-inventory:fail:symbol present in release binary: credential_free_request
```

That symbol reached Release at `11f4936` through [[BPR-0279]]'s own integrator
repair, and no RFC-BPR-0016 slice since has touched
`native_https_transport.cpp` — verified with `git log -S`. The guard predates
the product having a legitimate credential-free path, and it now cannot tell
RFC-BPR-0017's demonstration seam from RFC-BPR-0016's shipped device endpoints.
The CTest half (`rfc0017_test_inventory`) does not run the Release scan, so the
dev matrix stays green and only RFC-BPR-0017's closeout would have found it.
Carded as [[BPR-0451]] against RFC-BPR-0017, behind [[BPR-0318]], because
narrowing a guard while the capability it would permit is still unreachable is
the wrong order.
