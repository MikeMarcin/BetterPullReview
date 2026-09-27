---
schema: "implement-rfc.card.v1"
id: "BPR-0577"
title: "Allocate RFC-BPR-0023 across core, host, adapters, Qt, and composition root"
status: "Done"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0576"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
acceptance: [{"criterion":"architecture-state-and-algebra","outcome":"pass","revision":"8a11272be9eb629165113c58b4ab1e7d6cadb029","evidence":{"kind":"artifact","detail":"Current symbols and live allocation define authoritative state, bounded observations, typed intents/requests/terminals/identities/deltas and Qt projections.","path":"docs/verification/RFC-BPR-0023.md#m2--completed-architecture-under-amendment-r2"}},{"criterion":"singular-agent-and-mutation-paths","outcome":"pass","revision":"8a11272be9eb629165113c58b4ab1e7d6cadb029","evidence":{"kind":"artifact","detail":"Source inspection binds the existing chat/invocation/process route and shared-draft/mutation/coordinator route; no product source changed.","path":"docs/verification/RFC-BPR-0023.md#m2--completed-architecture-under-amendment-r2"}},{"criterion":"draft-authority-and-target-linkage","outcome":"pass","revision":"8a11272be9eb629165113c58b4ab1e7d6cadb029","evidence":{"kind":"artifact","detail":"Allocation extends the existing revisioned shared draft owner with exact local composition/draft transaction and separate RFC-0008/0022 publication.","path":"docs/verification/RFC-BPR-0023.md#m2--completed-architecture-under-amendment-r2"}},{"criterion":"ledger-migration-and-retention","outcome":"pass","revision":"8a11272be9eb629165113c58b4ab1e7d6cadb029","evidence":{"kind":"artifact","detail":"Allocation distinguishes ledger v6/recovery v5, preserves supported origins and v6 CAS/high water, retires flat-note authority, and defines reference-aware deletion.","path":"docs/verification/RFC-BPR-0023.md#m2--completed-architecture-under-amendment-r2"}},{"criterion":"navigation-and-bounded-projection","outcome":"pass","revision":"8a11272be9eb629165113c58b4ab1e7d6cadb029","evidence":{"kind":"artifact","detail":"Current modalities/tray/centre composition, private/shared roots, cursor/body/input bounds, stable model updates, focus and accessibility are allocated.","path":"docs/verification/RFC-BPR-0023.md#m2--completed-architecture-under-amendment-r2"}},{"criterion":"acceptance-evidence-ownership","outcome":"pass","revision":"8a11272be9eb629165113c58b4ab1e7d6cadb029","evidence":{"kind":"artifact","detail":"All eighteen unchanged RFC criteria retain direct surfaces, owning cards and refined falsifiers in the live audit/ledger.","path":"docs/verification/RFC-BPR-0023.md#m2--completed-architecture-under-amendment-r2"}},{"criterion":"r2-final-review-obligation","outcome":"pass","revision":"8a11272be9eb629165113c58b4ab1e7d6cadb029","evidence":{"kind":"artifact","detail":"Explicit owner amendment r2 moves the exact M2 review exit to M10/BPR-0591, retaining one capability-critical independent review of the final integrated candidate before closeout. That review remains pending.","path":"docs/verification/RFC-BPR-0023.md#m2--completed-architecture-under-amendment-r2"}},{"criterion":"focused-documentation-and-plan-checks","outcome":"pass","revision":"8a11272be9eb629165113c58b4ab1e7d6cadb029","evidence":{"kind":"artifact","detail":"DOCUMENTATION_OK; 64 documentation tests; scoped board validation; plan amendment/discovery/r2 binding; exact criteria/dependency/26-exit/26-pack-hash comparisons; diff whitespace checks pass. Candidate-bound checkpoint check permits fresh M3 context.","path":"docs/verification/RFC-BPR-0023.md#m2--completed-architecture-under-amendment-r2"}}]
---

# Allocate RFC-BPR-0023 across core, host, adapters, Qt, and composition root

## Objective

Turn the RFC into a live-tree-specific contract before types, schema, and UI diverge.

## Required final state

- One recorded allocation names authoritative state, compact core observations, intents/events/requests/deltas/diagnostics, effect ports, async identities/generations, and Qt projections.
- The allocation identifies the one existing agent launch path and one existing GitHub mutation path that must be reused.
- The current RFC-BPR-0008 draft authority and target vocabulary are audited; durable draft/composition linkage has exactly one owner.
- The current ledger migration frontier, supported prior schemas, sequence strategy, and no-dual-authority plan are settled.
- The private/shared drawer information architecture and bounded query/projection strategy are sketched against current widgets/models.
- Every acceptance criterion has a planned direct surface and evidence owner.

## Non-goals

- Provider-specific implementation in core
- Final visual polish
- Broad refactoring unrelated to this RFC

## Completion evidence

- Architecture allocation document or card attachment
- Current-symbol/path inventory
- Acceptance-to-card matrix
- Documentation/board/plan checks and explicit carry-forward of the independent authority-boundary review to M10/BPR-0591 under owner amendment r2

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

Resolve and test the live RFC's 1 MiB body versus 512 KiB page contract with
explicit immutable body continuations, progress, and no silent truncation.
Recheck total invocation-input bounds for selected compositions. Ledger schema
6 and recovery schema 5 are now separate live authorities; recheck each frontier
before allocation. Preserve exact v5 migration cases plus v6 note revision/CAS
and high-water behavior.


## M2 architecture acceptance under amendment r2

Mandate r2; milestone `m2-architecture`; implementer `/root/rfc0023_m2`;
M1 integration baseline `cb36559911d6cd4def1582a0121b527ed2f9edce`.
Claim commit `20ff2732e4e6d11df333ccc2fffbf3069029f7e3`.

The [live allocation](../../verification/RFC-BPR-0023-live-audit.md#m2--live-architecture-contract)
now names the current symbols and single owners, core algebra, bounded ports,
transaction and cancellation boundaries, schema/migration/CAS/retention,
per-invocation provenance, exact selected context, encoded input limits,
publication crash join and current modality/focus/accessibility allocation.
It refines every AC's direct evidence owner in the existing ledger. No product
source or imported source-only plan is changed and no feature acceptance is claimed.

The [verification and durable handoff](../../verification/RFC-BPR-0023.md#m2--architecture-candidate)
records the exact candidate, checks, command state and continuation. On
2026-09-26 the owner amended review timing: continue implementation now and
perform the independent review at the end of the RFC. The adopted plan moves
that obligation to M10/BPR-0591, whose one capability-critical independent review
must inspect the final integrated candidate and these architecture boundaries
before RFC closeout. The review is still required and has not occurred.

M2 acceptance covers the six architecture final states above, their current
source inventory, evidence ownership and focused documentation/board/plan checks.
It does not accept the final product or satisfy the final independent review.
The original pending candidate `b98ddfd477962c4768ab291e646a3fecd4e1600e` and
handoff `ba8cb88e942981ee925ef77a2d05d689fe8ae13f` remain historical evidence.
Completion under r2 permits the coordinator to land M2 and begin M3.
