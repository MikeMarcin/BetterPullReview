---
id: BPR-0276
title: Register the device client and capture real wire contracts
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0275]
---

# Register the device client and capture real wire contracts

Use only the authority recorded by BPR-0275 to create the selected public
client and capture sanitized contracts from the endpoints the product will call.

## Required final state

- The registered owner, public client identifier, exact permission/scope set,
  installation policy, device-flow setting, and refresh policy match the
  approved subject as narrowed by the recorded private/owner-only plain
  operator amendment; no client secret is shipped or committed.
- Sanitized real-service fixtures cover device-code success, token success,
  `authorization_pending`, one deliberately early bounded poll producing
  `slow_down`, denied, expired, and disabled responses. `slow_down` is the
  service's rate-limit contract; no separate rate-limit body is invented.
- Malformed, truncated, duplicated, and oversized bodies are explicitly
  synthetic hostile parser fixtures, not falsely attributed to GitHub.
- Capture tooling and evidence prove tokens, refresh tokens, device codes, user
  codes, cookies, and credentials are absent from repository artifacts and logs.
- The public identifier and fixture provenance are recorded without compiling
  the private verification identifier into any product artifact or granting
  repository-mutation authority. BPR-0281 owns the client-free unavailable
  composition; BPR-0318 separately owns a distributable identity and compiled
  production selection.
- Read-only app-token probes prove Metadata, Contents, Checks, pull-request REST,
  and the exact review-thread GraphQL document. Pull requests write is verified
  as configured in the registration but no POST is sent; actual review-comment
  publication belongs to the separate identity-bound designated-live packet and
  cannot be inferred from this card.

## Current course

The private GitHub App registration and selected-repository installation have
occurred under attended observation. The attended runner captured and locally
validated real expiry, denial, a fresh token-success course that restored the
BPR-Test user authorization and repeated all ten designated read-only probes,
and a disabled-setting response with a verified disable/restore transition.
The earlier token-success root is also preserved as intermediate evidence.

The operator authorized one replacement early poll. Its durable spent marker
was created before traffic and GitHub returned the documented `slow_down`
response, including its new interval, but the fail-closed runner admitted only
the three generic error fields and rejected the body. No raw response or course
root was retained. The parser and sanitizer now require a bounded repair that
admits the interval only for `slow_down` and adversarially rejects every other
schema/type variant. The original and replacement authority/spent records must
remain immutable and be bound into final evidence.

Changing the runner invalidates every existing final course identity. After a
new plain operator direction authorizes exactly one recovery early poll, all
five courses must be recaptured under the repaired runner and one runner digest;
mixed-runner assembly stays forbidden. The recovery uses a distinct O_EXCL
pre-traffic spent marker and no further retry is admitted. Every existing
owner-private course root remains intermediate provenance and is never silently
rewritten or represented as a repaired-runner capture.

The initial hermetic sanitizer, attended runner, and exact external-attestation
verifier landed at `3b6d470`, `100f578`, and `910176c`; the exact
`slow_down.interval` and two-chain parser-recovery contract is repaired at
`69acab3`, with final replay-time interval binding at `386176f`. Its focused
gates pass 39/39 sanitizer tests, 40/40 runner tests, and 2/2
repository-network-isolated CTests. The verifier requires every final
fixture byte, all five closed course trees, the exact repaired runner bytes,
the disabled-setting transition, every early-poll authority and spent marker,
deterministic reassembly, and a genuine accepted
BetterPullReview attended-review export. Its claim is deliberately limited to
human acceptance of those exact sanitized bytes as attended run evidence; it
does not claim cryptographic user, GitHub, or transport authenticity.

The verification app is private and owner-only under
`rfc0016-private-owner-only-2026-08-13`. This narrows the accepted registration
authority; it does not establish an any-account production identity or permit
production selection for other GitHub accounts.
