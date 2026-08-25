---
id: BPR-0276
title: Register the device client and capture real wire contracts
status: Done
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

## Operator direction — one recovery early poll authorized — 2026-08-20

Recorded verbatim:

> Granted

This is the plain operator direction this card's own text required:

> After a new plain operator direction authorizes exactly one recovery early
> poll, all five courses must be recaptured under the repaired runner and one
> runner digest; mixed-runner assembly stays forbidden.

### What this authorizes, stated narrowly

Exactly **one** recovery early poll, against the device-authorization endpoint,
under the repaired runner, using the private owner-only `BPR-Test` identity. It
carries its own pre-traffic `O_EXCL` spent marker, and no further retry is
admitted — the authorization is consumed whether or not the capture succeeds.

It authorizes nothing else. Not repository mutation, not release, not GHES, not
production selection for any other GitHub account, and not a second attempt if
this one fails the way the last one did.

### What is not yet established

The last poll reached the wire, GitHub returned the documented `slow_down` with
its new interval, and the fail-closed runner rejected the body and retained
nothing — the authority was spent for no evidence. The parser and sanitizer
repair landed at `69acab3` and `386176f`, and their focused gates pass 39/39,
40/40, and 2/2 hermetically.

Before spending this authorization, the repair must be shown to accept the exact
shape that was rejected, hermetically, against a recorded response rather than a
live one. Spending a single-use grant to discover a second parser gap would waste
the thing just granted. That check is unattended and comes first.

## The precondition is discharged, and the poll is still attended — 2026-08-20

### The repair admits the shape that was rejected

This card recorded that spending the grant to discover a second parser gap would
waste it, so the repair was proven hermetically first. Same body, same
registration provenance built by each version's own functions, no network:

| parser | result |
|---|---|
| before the repair, `69acab3^` | **REJECTED** — `ContractError: response schema mismatch` |
| at HEAD | **ACCEPTED** — `interval=10`, projection values `['error', 'interval', 'official_error_uri']` |

The body is the documented one: `error=slow_down`, its description, the
`docs.github.com` error URI, and `interval`. The cause is visible in one line of
the exact-key table, `capture_rfc0016_device_contracts.py:234`:

```
before:  "slow_down": ["error", "error_description", "error_uri"]
at HEAD: "slow_down": ["error", "error_description", "error_uri", "interval"]
```

Exact-key enforcement is what discarded the real response and left the first
authorization spent for nothing. It no longer does. Both hermetic suites also
pass at HEAD: 39/39 and 40/40.

### The grant authorizes the poll; it does not make it unattended

The owner granted one recovery early poll and asked for unattended work from that
point. Those do not compose here, and it is a property of the protocol rather
than of the authorization.

Device authorization requires a person. `run_rfc0016_device_capture_attended.py`
has `show_device_code(user_code)` at `:805` and `present(grant.user_code)` at
`:1034`, and the capture's own provenance records
`observation_method: attended_github_settings_page` at `:1374`. Someone must read
the user code, open the verification URL, enter it, and authorize. No unattended
session can supply that, and a run that reaches the poll without it consumes the
single-use marker and captures nothing — exactly the failure this card is
recovering from.

So the card stays in `40-parked/` under `awaiting-owner-verdict`. What changed is
that the last unattended obstacle is gone: the authorization is granted, the
parser is proven against the shape that defeated it, and the next action is one
attended session that recaptures all five courses under one runner digest.

## Recaptured, admitted, and closed — 2026-08-24

The owner sat the attended session. All five courses were recaptured in one
sitting under the single repaired runner digest
`sha256:d00583abcb052c72e11c9f14e97e1dd69575455e26e81b5db32455c98c1b5c3c`:
real expiry, real denial, a fresh token success whose ten designated read-only
probes ran and restored the `BPR-Test` user authorization, the
disabled-setting response inside an attended enabled→disabled→restored
settings transition (02:20:50Z → 02:21:14Z → 02:23:34Z), and the authorized
recovery early poll. The recovery consumed its distinct `O_EXCL` pre-traffic
spent marker, GitHub returned the documented `slow_down` body, and the
repaired parser admitted it with its bounded interval — the evidence the two
prior spent authorities never obtained. The registration observation was
re-confirmed by the owner against the live settings page before any course
ran, and `RFC0016_ATTENDED_CAPTURE_BUNDLE_OK` assembled the five course roots
deterministically.

The evidence is repository-recorded: the sanitized fixture tree at
`tests/fixtures/github/device-authorization/v1/` (replacing the
`pending_real_capture` placeholder), the five course trees and all four
original/recovery authority/spent records at
`docs/verification/rfc0016-live-capture-v001/courses/`, and the live-run
attestation beside them. The owner reviewed the digest-bound offline page at
`docs/verification/reviews/rfc0016-attended-live-capture-v001/` and accepted
batch `bpr:/attended-review/betterpullreview/42fd8114a6c2bbeb0cbf6f54`,
subject digest
`sha256:a72c61372e2a3da00c4746f400da6bf983121b55e6be2651a9a5abb0ae67afac`,
with every downstream claim declared false. Complete admission passes:
`RFC0016_DEVICE_CAPTURE_VALID:attended_human_accepted_capture`. The claim
remains deliberately bounded to human acceptance of these exact sanitized
bytes as attended run evidence; production identity and platform enablement
remain BPR-0318's, and this card's done move promotes exactly BPR-0277 and
BPR-0278.
