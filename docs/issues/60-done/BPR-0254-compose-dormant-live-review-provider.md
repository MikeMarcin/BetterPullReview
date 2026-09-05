---
schema: "implement-rfc.card.v1"
id: "BPR-0254"
title: "Compose the dormant live review provider safely"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0252"]
---

# Compose the dormant live review provider safely

BPR-0252 linked the production provider and exact review-comment protocol, but
the shipped application still has only the fixture mutation composition. The
provider can now be selected in production without enabling publication, using
the complete designated target and the exact authenticated transport already
owned by the GitHub application host.

## Required final state

- The provider target binding and immediate re-observation include the complete
  designated repository/pull-request tuple: canonical host and owner/name,
  repository and pull-request database/node identities, number, open state,
  base-repository identity, base OID/ref, head OID/ref, and acting account
  database/node/login identity.
- `github_application_host` exposes only a narrow opaque mutation transport
  lease bound to its currently accepted profile/account/authority. Qt and the
  composition root never receive credential material or a network manager.
- The production composition root compares the accepted canonical host,
  repository owner/name and database/node identity, pull-request number and
  database/node identity, base-repository identity, base OID/ref, and head
  OID/ref with compiled-in `MikeMarcin/BPR-Test#1` target constants immediately
  before constructing the contracted provider. The acting account
  database/node/login identity is instead the exact dynamic identity observed
  from the authenticated session; it is cross-bound to the accepted snapshot
  and lease and is re-observed before use. No acting-account identity is a
  compiled constant. Any mismatch constructs neither the provider nor a
  coordinator and performs no network request.
- The opaque mutation lease has shared fail-closed revocation state. Profile,
  account, or accepted-identity invalidation and application-host shutdown
  revoke retained copies, so both later provider construction and transport use
  refuse the stale lease. Lease revocation is a denial boundary, not an
  asynchronous request-cancellation claim.
- This card keeps the provider dormant: it does not construct a production
  mutation coordinator, enable live Publish, or call provider observation or
  submission. The following slice owns asynchronous publication, stop
  authority, exact selected-line/path resolution, stale completion, and
  shutdown.
- Fixture runs continue to construct only the fixture provider and emit
  `REVIEW_MUTATION_PROVIDER:fixture`. Relevant shipped-executable contracts
  require that marker and refuse a live-selection marker.
- The composed-host verifier requires the live provider in the production
  application, forbids fixture mutation construction and reconciliation there,
  and continues to require fixture composition in the development-fixture
  profile. The standing isolation verifier still rejects the concrete live
  provider from every non-production executable.
- Tests-disabled Release and development-fixture binary evidence are
  non-vacuous, and exact/mismatched full-tuple selection probes prove the safe
  composition decision without credentials or network access.

## Non-goals

- Do not execute a live request or designated mutation lane.
- Do not acquire, log, persist, or use a write credential.
- Do not alter BPR-0235's fixed harmless canary/runner.
- Do not enable synchronous live publication on the Qt event path.
- Do not claim BPR-0142, provider cancellation, durable indeterminate retry,
  target-native Linux/Windows, or attended web-UI evidence.

## Verification

- Exact safe-target decision and opaque transport-lease host tests.
- Fixture public-path marker and live-selection cut/counterexample.
- Development-fixture and tests-disabled Release composed-host verification.
- `live_mutation_isolation`, `live_mutation_isolation_contract`, architecture
  boundaries, documentation validation, scoped format/tidy, and
  `git diff --check`.

## Implementation evidence

- Host and provider tests retain leases across identity invalidation and
  shutdown, then prove provider construction or use refuses them without a
  transport call.
- A changed head OID remains a valid target observation and reaches the
  session's `stale_head` decision before any POST; base OID/ref and head ref
  remain exact target constraints.
- Provider-independent binding validation and idempotency helpers live outside
  the coordinator translation unit. The fresh tests-disabled Release audit
  requires the live provider and factory while forbidding coordinator,
  fixture, and reconciliation symbols.
- The provider remains dormant: the production composition constructs no
  coordinator and performs no provider observation, submission, network, or
  credential access.

## Completion record

- The focused host, coordinator, mutation-contract, provider-admission,
  fixture-composition, isolation, architecture, and composed-host matrix passed
  10/10 on the final portable snapshot.
- A fresh tests-disabled Release build contains the contracted live provider and
  factory while excluding the fixture provider, reconciliation path, and review
  mutation coordinator constructor.
- The dormancy contract locks the complete production composition branch and
  inventories every concrete-provider lifecycle reference across the app
  translation unit. Hostile direct-member and generic-port helper injections
  both fail the same gate.
- The development-fixture executable emits the exact fixture marker, rejects all
  28 single-field designated-target substitutions, and remains free of the
  concrete live-provider symbol across the configured non-production binary
  manifest.
- Scoped format and SDK-aware tidy checks, documentation validation, and
  `git diff --check` passed. Independent critical rereview returned CLEAR with
  no blocking or high-risk findings.
- No live request, credential access, mutation, Linux replay, or Windows replay
  occurred. Asynchronous publication, cancellation, and selected-line binding
  remain assigned to the following slice.
