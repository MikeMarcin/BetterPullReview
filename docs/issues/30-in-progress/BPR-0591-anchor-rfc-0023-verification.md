---
schema: "implement-rfc.card.v1"
id: "BPR-0591"
title: "Anchor RFC-BPR-0023 verification and closeout"
status: "In Progress"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0590","BPR-0242","BPR-0245","BPR-0507"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
---

# Anchor RFC-BPR-0023 verification and closeout

## Objective

Perform capability-critical review of durable private data, agent provenance, and remote publication, then reconcile all live cards and RFC status.

## Required final state

- An independent reviewer traces private creation, agent dispatch, composition, draft acceptance, publish, server reconciliation, migration, deletion, and crash paths end to end.
- The review explicitly checks no provenance overclaim, no hidden GitHub metadata, no second agent/mutation path, no guessed anchor, no Qt/provider bypass, and no dual storage authority.
- All review findings are fixed or recorded as explicit blockers/deferred work with RFC-compatible rationale.
- Every required live card is Done before Auto-Verified closeout; pending work
  stays in its truthful lane and prevents closeout. Indexes and claim records agree.
- RFC status changes to `Auto-Verified` only if all dependencies are Auto-Verified and every required direct evidence row is satisfied. Otherwise available-target work remains complete with the RFC Proposed and closeout card parked/backlog as appropriate.

## Non-goals

- Waiving dependency order
- Closing from aggregate test success without claim inspection
- Treating unavailable live-service evidence as passed

## Completion evidence

- Independent review report
- Final claim and architecture-conformance matrices
- Issue-board audit
- Final documentation/structure/focused gate output
- RFC/index status diff

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

RFC base: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.

Requested RFC set: RFC-BPR-0022, RFC-BPR-0023; import position: 2 of 2.
Planned cards: BPR-0576, BPR-0577, BPR-0578, BPR-0579, BPR-0580, BPR-0581, BPR-0582, BPR-0583, BPR-0584, BPR-0585, BPR-0586, BPR-0587, BPR-0588, BPR-0589, BPR-0590, BPR-0591.

## Closeout checklist

| Criterion | Planned check | Result | Producing commit |
|---|---|---|---|
| A reviewer can create a PR-level or exact semantic-subject private thread, add reviewer turns, restart BetterPullReview, and recover the thread and its revision history exactly. | AC-01 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Side-chat prompts and complete assistant responses can be retained as turns without adding a second agent launch path, and the thread remains bound to the exact review revision it was created against. | AC-02 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Every turn revision has a typed actor. New reviewer text is `reviewer`, agent text names one exact `agent_run_id`, and migrated notes are `legacy_unknown`. | AC-03 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Editing agent-authored text creates a reviewer-authored successor with an immutable `edited_from` edge; the original bytes, actor, and run provenance remain inspectable. | AC-04 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Agent provenance retains requested, configured, protocol-reported, and content-self-reported model/effort facts separately. Missing facts remain unknown, disagreements are visible, and no value is inferred from a profile or executable name. | AC-05 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| One agent session can produce multiple uniquely identified runs and finding sets without collision; retained provenance remains readable after session catalog pruning. | AC-06 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Thread subjects and composition targets use exact semantic identities. Head/epoch movement produces visible stale or unplaceable state and never a nearest-line or silent reanchor. | AC-07 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| A reviewer can select exact revisions/findings, open a composition workspace, inspect sources and provenance, and produce a reviewer-authored, exact-copy, or explicitly agent-assisted candidate. | AC-08 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| `Squash to Draft` creates only local composition/draft state. Tests prove zero GitHub mutation, zero resolve operation, and zero review submission at this step. | AC-09 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Sources remain present and traceable after composition. Multiple exact targets require explicit target selection or Split into Drafts; the product does not guess. | AC-10 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Accepting a candidate records the exact composition revision on a durable RFC-BPR-0008 draft and survives restart. | AC-11 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Publishing requires a separate explicit gesture and traverses the existing target/head/acting-identity/idempotency/safe-target mutation path. | AC-12 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| A successful publication records local lineage to the returned server comment and reconciled server state. The GitHub body contains no hidden or mandatory BetterPullReview/AI marker and appears under the authenticated reviewer's identity. | AC-13 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Legacy local notes migrate byte-for-byte into one-turn private threads with unknown authorship, no invented provenance, no ID reuse, and no second private-note authority. | AC-14 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Private thread pages, provenance details, and composition inputs are bounded and cursor-loaded. Quota, oversize, and stale-source cases refuse whole and never silently truncate. | AC-15 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Private content reaches an agent only when exact source revisions are explicitly selected and successfully revalidated at dispatch. | AC-16 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| Deleting the local ledger or an explicitly selected private thread changes no GitHub state; the remaining ledger recovers transactionally after abrupt termination. | AC-17 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |
| The complete workflow is reachable through the real BetterPullReview composition root and is usable by keyboard and assistive technology at 100%, 125%, and 200% scale without clipping or whole-model resets for a bounded turn update. | AC-18 in RFC-BPR-0023 verification ledger | Not run; implementation authorized | None |

No acceptance criterion is satisfied by import. Reuse passing evidence only
while its inputs are unchanged. Continue the adopted live milestone plan under mandate r2; re-read the live
board and RFC dependencies. The old baseline above remains import provenance;
the immutable implementation RFC_BASE_SHA is recorded in the verification ledger.

## Import boundary

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.


## Owner amendment r2 — one independent review at final closeout

On 2026-09-26 the owner instructed implementation to continue and moved the
independent review from M2 to the end of RFC-BPR-0023. This card/M10 now owns one
capability-critical independent review of the **final integrated candidate**,
including the authority-boundary review previously allocated to BPR-0577. The
adopted plan preserves the prior review exit at M10; the timing change neither
waives review nor treats the M2 implementer's self-review as independent evidence.
The reviewer is authorized for the end, after implementation; no reviewer is
requested or dispatched during M2.

In addition to every existing review requirement above, explicitly challenge the
[live allocation](../../verification/RFC-BPR-0023-live-audit.md#m2--live-architecture-contract)
against the implemented source-use admission/edit/delete race, immutable lineage
and cleanup, v1–v6 migration/CAS, shared draft single authority, account/window
retirement, qualified runtime metadata, total-input/body bounds, and the
ledger/remote-journal publication crash join. Trace creation, agent dispatch,
composition, draft acceptance, separate publication, reconciliation, deletion,
and crash recovery through the public composition. Review is bound to an exact
final source revision and its evidence; material findings require scoped
correction and verification before closeout. The original final-state,
independence and eighteen-criterion requirements remain intact.

M2 architecture acceptance is not review acceptance. This review remains pending
until supplied at M10, and prevents RFC/index Auto-Verified closeout while open.
