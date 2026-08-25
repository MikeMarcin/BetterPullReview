---
id: BPR-0281
title: Compose the production device-authorization path
status: In Progress
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
