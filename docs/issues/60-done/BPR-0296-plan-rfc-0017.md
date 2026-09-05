---
schema: "implement-rfc.card.v1"
id: "BPR-0296"
title: "Plan RFC-BPR-0017 implementation and verification"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: []
---

# Plan RFC-BPR-0017 implementation and verification

Establish the durable run anchor, complete issue graph, eligibility and product
decision ledger, architecture allocation, documentation impact, and direct
verification matrix for unauthenticated public review reading.

## Run anchor

- RFC base: `a45750f6bd84716240aa27a20f0734f33808ab8e`.
- Requested set position: second, after RFC-BPR-0016 and before remaining
  unattended macOS-ready RFC and issue work.
- Current RFC status: `Deferred`.
- Implementation authority: absent until real GitHub.com probes, a deliberate
  credential-free native-HTTPS contract, the product-mode decision, and the
  RFC-BPR-0002 dependency or an exact narrow exception are recorded.

## Planning requirements

- Treat RFC-BPR-0016 and RFC-BPR-0017 as alternative answers to the same
  authentication-friction problem; do not implement anonymous mode merely
  because RFC-BPR-0016 awaits external authority.
- Allocate real-service probe/product-decision work ahead of every
  implementation card and preserve a documented `do not build` outcome.
- Reuse the existing read contract, normalization, provider, and fixture
  architecture; allocate no parallel anonymous provider or read path.
- Model missing viewer identity, thread resolution, viewed state, actionability,
  private-repository disclosure, rate exhaustion, and GHES refusal explicitly.
- Prove zero vault lookup and zero authorization header as positive properties,
  not as consequences of a missing credential.

## Completion evidence

- The issue graph and immutable capability-critical closeout anchor are
  committed without changing RFC status.
- Every eligibility gate and acceptance criterion has a falsifiable direct
  verification row, exact owner, and durable evidence location.
- Pending external/attended gates grant no implementation, traffic, private
  repository, credential, or product-mode authority by implication.

## Result

- Recorded that the live tree contradicts two RFC findings: the transport and
  host require authenticated identity, and the current catalog has no REST
  review-comment operation from which anonymous threads could be reconstructed.
- Allocated BPR-0297 through BPR-0309 plus BPR-0312 through BPR-0317 for RFC
  correction, probes, non-product transport demonstration, the explicit build/
  no-build decision and positive gate, core/transport/normalization/host/diff/
  Qt/composition, platform evidence, verification, simplification,
  documentation, and critical closeout.
- Kept the RFC `Deferred`, the existing public observer outside product
  authority, and every network/product-mode gate explicit.
- Bound the issue graph, claim matrix, documentation ledger, and direct
  verification requirements to the immutable RFC base.
