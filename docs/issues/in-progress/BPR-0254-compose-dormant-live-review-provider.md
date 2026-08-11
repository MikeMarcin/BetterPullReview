---
id: BPR-0254
title: Compose the dormant live review provider safely
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0252]
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
- The production composition root compares every accepted handoff/snapshot
  field with compiled-in `MikeMarcin/BPR-Test#1` constants immediately before
  constructing the contracted provider. Any mismatch constructs neither the
  provider nor a coordinator and performs no network request.
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

