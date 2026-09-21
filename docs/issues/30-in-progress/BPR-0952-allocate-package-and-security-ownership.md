---
schema: "implement-rfc.card.v1"
id: "BPR-0952"
title: "Allocate package and security ownership"
status: "In Progress"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0951"]
milestone: "M2 Architecture allocation"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Allocate package and security ownership

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Inventory the real build/install/runtime graph, existing release tools, retention/migration/diagnostic surfaces and all RFC acceptance gaps at the exact candidate.
- AC2: Record one authority and typed intent/event/observation/request/delta path for each affected feature, including request/session/profile/revision/generation, effect ports, cancellation, shutdown and evidence-backed no-impact regions.
- AC3: Bind concrete package, hardening, provenance, native-fixture and documentation work to the adopted plan; preserve the macOS updater exception, inherited evidence limits and all remaining platform criteria.

## Verification and execution record

Assigned child `/root/rfc0014_m2`, milestone `m2-architecture-allocation`,
mandate r3, adopted clean entry `a9a1e3cd1b897bf1242a3dc46ccd986a9dfe0323`.
The immutable RFC base remains `d0a8a6dda005af12b3818234f4878a9f48a44146`.
Claim `303ba3b5e2f457f5b0f9385ea999e46c058b84d5` changes only this card's lane.

The [release-readiness allocation](../../architecture/release-readiness.md)
records exact existing owners, typed paths/identities/lifecycles, build/runtime
and static dependency graph, release-tool reuse, G1–G9 gaps and all nine public/
native criterion checks. Its bounded follow-on notes are copied to the owning
cards. No acceptance bullet, stage exit or platform obligation is removed.
Linux baselines are declared qualification targets, not observed native support;
the existing local activation IPC versus the no-local-server criterion is
explicitly pending reconciliation in M9.

The real development app builds. Its maintenance course passes seed/prune/
restart/delete/restart verification, and composed-host linkage passes. The
checkout architecture scan finds an unrelated ignored nested worktree's second
adapter declaration; an exact committed tracked-source archive passes the same
check. See the [M2 ledger](../../verification/RFC-BPR-0014.md#milestone-2-architecture-allocation)
for exact commands, executable hash, limits and final candidate evidence.
M2 changes no product/build/tool source and requires no attended verdict.

Owner amendment r2 explicitly requires reuse of the existing project
`publish-release` skill, `tools/release.sh`, `tools/publish-release.sh` and
`macos-release` preset. The [M1 source inventory](../../verification/RFC-BPR-0014.md#owner-amendment-r2-existing-macos-release-workflow)
maps their existing universal build/deploy/sign/notarize/verify/package/publish
mechanisms and exact fingerprints to remaining acceptance. Inventory and extend
demonstrated gaps; do not duplicate those mechanisms or treat their existence
as clean-target, trust or publication evidence.

Owner amendment r3 permits source-backed revisions to obsolete implementation
prescriptions in the RFC/plan while preserving intended outcomes and security
obligations. M1 has reconciled the third-party runtime inventory wording and
removed an inferred standalone-core-SDK/general-embedding deliverable. The nine
RFC acceptance bullets are unchanged. Record any later material scope or
acceptance change explicitly, retain unavailable native/human evidence, and
reuse existing core/host/Qt behavior with direct evidence where no new code is needed.
