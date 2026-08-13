---
id: BPR-0274
title: Plan RFC-BPR-0016 implementation and verification
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# Plan RFC-BPR-0016 implementation and verification

Establish the durable run anchor, complete issue graph, eligibility ledger,
architecture allocation, documentation ledger, and direct verification matrix
for the GitHub device-authorization RFC before any implementation begins.

## Run anchor

- RFC base: `b969554fb45d838b2a4ea11811363bd7e158e47e`
- Requested set and order: RFC-BPR-0016, then RFC-BPR-0017, then remaining
  unattended macOS-ready RFC and issue work.
- Current RFC status: `Deferred`.
- Implementation authority: absent until every live eligibility gate is
  recorded as satisfied or the product owner records an exact narrow exception.

## Planning requirements

- Allocate one external-authority and evidence card before every implementation
  card.
- Preserve manual token entry as a first-class path and the existing native
  vault enrollment boundary.
- Allocate functional-core, GitHub-adapter, application-host, Qt/composition,
  direct verification, documentation, simplification, and capability-critical
  closeout slices.
- Record the open RFC-BPR-0002 Windows x64 dependency without treating macOS,
  cross-compilation, mocks, or hosted workflows as a substitute.
- Prepare digest-bound operator decisions for any application-kind,
  registration, permission, external traffic, or dependency exception.
- Add no client identifier, device endpoint, device code, token, fixture, or
  partial product mode while an eligibility gate is unsatisfied.

## Completion evidence

- The complete RFC-BPR-0016 issue graph and closeout anchor are committed.
- Every acceptance criterion has a direct failing-if-wrong verification row.
- The documentation ledger covers authentication, credentials, native HTTPS,
  lifecycle, cancellation, Qt presentation, browser handoff, composition,
  build selection, and operator guidance.
- Pending attended and external gates are explicit, digest-bound, and grant no
  implementation or remote authority by implication.

## Result

- Recorded the immutable RFC base and the product-owner-requested RFC order in
  the RFC-BPR-0016 issue plan.
- Allocated BPR-0275 through BPR-0285 plus BPR-0310/BPR-0311 for
  eligibility, real-service contracts, core policy, GitHub transport,
  application orchestration, Qt presentation, composition, security
  verification, documentation, simplification, platform evidence, and
  capability-critical closeout.
- Kept the RFC `Deferred`: neither the unresolved RFC-BPR-0002 closeout nor the
  application-registration and real-service-fixture gates was inferred away.
- Mapped every acceptance criterion to a falsifiable final-head row on
  BPR-0285 and recorded affected documentation surfaces.
- Recorded the scheduler-capacity defect encountered during planning: the
  runtime advertised ten child slots but rejected a third live child both at
  the root and through a running worker. Two read-only lanes ran concurrently;
  remaining planning work continued locally without widening write ownership.
