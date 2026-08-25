---
id: BPR-0277
title: Implement deterministic device-authorization policy
status: Done
rfc: RFC-BPR-0016
depends_on: [BPR-0276]
---

# Implement deterministic device-authorization policy

## Required final state

- Core owns compact typed states, intents, revision/request identity, semantic
  legality, stale-result rejection, terminal diagnostics, and cancellation.
- Poll timing, interval, backoff, and transport retry mechanics remain host
  effects owned by BPR-0279 and never enter core policy.
- Core and core-owned state cannot represent client identifiers, service
  endpoints, poll intervals, device/user codes, access tokens, or refresh
  tokens. A narrow host-owned display projection may carry only the user code,
  authority-validated verification URI, and bounded expiry needed by Qt.
- Replay/property tests cover every legal and illegal transition, duplicate and
  stale observations, cancellation, denial, expiry, unavailable, and success.
- No Qt, provider, filesystem, process, clock, timer, thread, network, database,
  credential, or hidden-global dependency enters `src/core/`.

## Why this waits on BPR-0276, written down — 2026-08-20

This card carried `depends_on: [BPR-0276]` with no prose anywhere explaining it,
which made the edge look like an unexamined claim when the backlog was audited.
It is not, but the real reason is not the one the edge names.

Nothing about this card's required final state needs BPR-0276's captured bytes.
The acceptance is spec-derived — typed states, legality, stale-result rejection,
replay and property tests — and the third bullet *forbids* real client
identifiers, endpoints, intervals, device and user codes, and tokens from ever
entering core. The file sets are disjoint too: this card adds `src/core/`, while
BPR-0276 works in capture and runner scripts.

**The actual blocker is that RFC-BPR-0016 is `Deferred`.** The board records it
plainly — "RFC-BPR-0016 remains Deferred … no partial device-flow mode exists" —
and this card is one of BPR-0277 through BPR-0285 in that program. Starting it
would begin implementing a deferred RFC, which is an owner decision rather than
a scheduling one.

So the edge is a proxy for a program-level deferral. It is left in place because
it produces the right behaviour, and this section exists so the next audit does
not mistake a missing justification for a missing dependency. Lifting the edge
would not make this card startable; only un-deferring the RFC would.

## The deferral reasoning above is superseded — 2026-08-24

The owner made device login the required product interaction before live
publication when cancelling BPR-0256 v006, and the reconciliation recorded in
`docs/verification/RFC-BPR-0016.md` establishes that the RFC's `Deferred`
front matter describes closeout eligibility, not implementation permission:
the validated available-target implementation exception authorizes
implementation once the first three eligibility gates close, and done
[[BPR-0276]] closed the last of them with its admitted attended capture. This
card is therefore genuinely startable in `20-ready/`. The paragraph above is
retained as the reasoning that was correct while the program-level deferral
stood; RFC closeout still waits on [[BPR-0318]] and RFC-BPR-0002.

## Implemented and verified — 2026-08-24

`src/core/include/bpr/core/device_authorization_model.hpp` and
`device_authorization_transition.hpp`, with their sources, model the flow in
the repository's established core idiom: one event variant mixing the two
intents with eight typed host observations, a
`device_authorization_request_context` carrying request, session, identity
generation, and a per-flow `device_authorization_generation`, a `transition()`
free function, and a `device_authorization_commands()` legality projection.
Stages are idle, requested, awaiting_authorization, cancelling, and the seven
terminals (authorized, denied, expired, disabled, unavailable, cancelled,
failed); `terminal()` names them and every terminal re-enables a fresh begin
under a strictly newer generation.

Two design decisions recorded for the successor cards: cancellation is
two-phase — a cancel intent enters `cancelling`, emits the cancel host
request, and refuses every completion observation deterministically until the
host confirms erasure with the cancelled observation — and the narrow display
projection (user code, validated verification URI, bounded expiry) is
host-owned per the RFC's allocation, so it belongs to [[BPR-0279]]'s file set
and deliberately does not exist in core. Every type in the new headers is
strong ids and closed enums only — no string field exists anywhere in the
model — so client identifiers, endpoints, poll intervals, device/user codes,
and token material are unrepresentable by construction; the scan-based
boundary proof of that claim stays allocated to [[BPR-0282]] with the claim
matrix.

Verification on this host: `core_device_authorization` registered
network-isolated and green; the fifteen-test replay/property suite covers the
successful course, every terminal observation, every failure-to-reason
mapping, whole-matrix stale/duplicate/orphan rejection over all eight
observations, cancel-versus-completion races, host-initiated cancellation,
restart-after-every-terminal with stale prior completions, per-stage command
legality, and rejected events emitting no host request. Full `core-only`
matrix 95/95 after a fresh configure; `dev` builds clean;
`core_dependency_boundary`, `core_boundary_contract`,
`architecture_boundaries`, and `architecture_boundary_contract` pass;
`coding-format-check`, strict `coding-tidy`, `verify_documentation.py`, and
`git diff --check` are clean.
