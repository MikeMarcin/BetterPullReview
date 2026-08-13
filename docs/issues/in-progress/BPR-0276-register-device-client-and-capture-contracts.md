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
occurred under attended observation. The immutable attended runner captured and
locally validated the real `expired_token` course and the token-success course,
including all ten exact read-only Metadata, Contents smart-HTTP, Checks,
pull-request REST, and review-thread GraphQL probes. Those two owner-private
course roots remain transient until the complete five-course set is assembled,
reviewed, and recorded; they do not yet satisfy the real-contract gate.

The denial, disabled-setting, and `slow_down` courses remain pending explicit
operator direction because they respectively revoke the current BPR-Test user
authorization, temporarily disable and restore Device Flow, and spend one
replacement early poll. The original early poll observed `slow_down` but its
interrupted exploratory course retained no admissible fixture. No replacement
poll is authorized yet.

The hermetic sanitizer, attended runner, and exact external-attestation
verifier are committed at `3b6d470`, `100f578`, and `910176c`. The verifier
requires every final fixture byte, all five closed course trees, the exact
runner bytes, the disabled-setting transition, the replacement-poll authority
and spent marker, deterministic reassembly, and a genuine accepted
BetterPullReview attended-review export. Its claim is deliberately limited to
human acceptance of those exact sanitized bytes as attended run evidence; it
does not claim cryptographic user, GitHub, or transport authenticity.

The verification app is private and owner-only under
`rfc0016-private-owner-only-2026-08-13`. This narrows the accepted registration
authority; it does not establish an any-account production identity or permit
production selection for other GitHub accounts.
