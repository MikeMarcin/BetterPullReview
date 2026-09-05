---
schema: "implement-rfc.card.v1"
id: "BPR-0089"
title: "Plan RFC-BPR-0005 architecture and verification"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0088"]
---

# Plan RFC-BPR-0005 architecture and verification

Allocate the RFC-BPR-0005 task set, choose the architecture allocation across core, host, and Qt, and build the verification matrix before implementation rather than at closeout.

Record which acceptance criteria are provable unattended on this macOS host and which are deferred. RFC-BPR-0005's own text names no owner-attended gate, but the RFC-BPR-0003 and RFC-BPR-0004 ledgers both defer a presentation-polish and native visual-acceptance pass to this RFC; that inherited obligation is a real attended gate and gets its own card.

This RFC ends at a closeout-preparation card in `backlog/` and stays `Proposed`. Closure order is unchanged.

## Direct verification

- Twelve implementation, documentation, simplification, and closeout cards are
  allocated as BPR-0090 through BPR-0101, with dependencies encoded so the
  chain runs core to host to Qt to composition. `docs/issues/NEXT_ID` advanced
  to `BPR-0102`.
- The RFC-BPR-0005 verification ledger records the architecture allocation, the
  claim matrix with one row per acceptance criterion, the documentation ledger,
  and the closeout blockers, from immutable base
  `55d365f16847d2ffc2c8fab83a5ce62eb695e2a0`.
- The ledger names the RFC-BPR-0004 surfaces this RFC composes rather than
  rebuilds, and states that a second mapping, presentation-side regrouping,
  follower-scroll coupling, or synthetic alignment rows would be a defect.
- Gate classification: no RFC-BPR-0005 requirement needs Windows execution, a
  GitHub mutation, a credential, or a network service; its non-goals exclude
  full GitHub mutations. Exactly one owner-attended gate exists and it is
  inherited rather than native to this RFC, so it is recorded as such.
- `DOCUMENTATION_OK`, and `documentation_issue_graph_contract` passes.

## Result

RFC-BPR-0005 is planned with its verification matrix built before
implementation rather than at closeout. The single attended gate is isolated on
BPR-0100 so it blocks nothing else. BPR-0089 is complete.
