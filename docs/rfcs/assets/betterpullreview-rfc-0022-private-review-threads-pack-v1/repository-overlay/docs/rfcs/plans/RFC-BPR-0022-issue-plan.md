---
provisional_id: PACK-ISSUE-BPR-0022
rfc: RFC-BPR-0022
status: Backlog
live_id: allocate-on-import
milestone: Post-Pack v2
depends_on_rfc: [RFC-BPR-0007, RFC-BPR-0008, RFC-BPR-0011, RFC-BPR-0012]
---

# Issue Plan for RFC-BPR-0022

## Goal

Implement and Auto-Verify **Private Review Threads, Provenance, and Comment
Composition** from the live repository after pack import, without weakening the
private/shared boundary or the explicit RFC-BPR-0008 publication path.

## RFC prerequisites

- RFC-BPR-0007
- RFC-BPR-0008
- RFC-BPR-0011
- RFC-BPR-0012

Available-target implementation is explicitly authorized by the product-owner
direction recorded in the RFC. `Auto-Verified` closeout remains dependency
ordered.

## Minimum live issue-card slices

1. Pack import, live-ID allocation, source re-audit, architecture allocation,
   claim matrix, documentation ledger, and closeout card.
2. Qt-free private-thread, subject, actor, turn, immutable revision, staleness,
   and lifecycle model.
3. Qt-free agent-run provenance, typed fact-basis/conflict model, finding-source
   identity, derivation DAG, and composition state machine.
4. Forward-only SQLite schema, v1-through-current migration, legacy-note
   conversion, durable IDs, paged query ports, quotas, purge, and recovery.
5. Agent request/protocol/result metadata capture with one durable run per
   invocation and honest unknown behavior.
6. Private-thread application host, reviewer-prompt-before-dispatch ordering,
   side-chat/finding ingestion, exact source selection, cancellation, and stale
   completion handling.
7. Squash-to-Draft host path, deterministic/manual and explicit agent-assisted
   modes, target partitioning, durable draft correlation, and restart recovery.
8. Publication-lineage correlation through the existing RFC-BPR-0008 mutation
   and reconciliation path.
9. Private/shared Qt conversation navigation, incremental transcript, history,
   provenance details, source selection, composition workspace, and separate
   accept/publish controls.
10. Composition-root/public-path wiring and deterministic end-to-end workflow.
11. Focused core/host/adapter/Qt/security/migration/crash/performance evidence.
12. Rendered UX, keyboard, accessibility, scale, theme, and no-clipping evidence.
13. Product/architecture/developer/agent/operator documentation and
    simplification.
14. Capability-critical independent review and RFC closeout.

The supplied `issue-templates/` split these requirements more finely. The
integrator may merge adjacent templates only when the resulting card remains a
bounded, independently verifiable slice; it may not hide an acceptance
criterion inside a broad implementation card.

## Required planning artifacts

- Live issue dependencies and lane assignments
- Per-acceptance-criterion claim/evidence rows
- Architecture allocation for core, host, SQLite, agent adapters, Qt, and
  composition root
- Live schema allocation and migration matrix
- Durable ID/high-water allocation plan
- Bounded query, memory, database-size, and changed-set performance plan
- Agent protocol metadata capability matrix
- Private-context selection and secret-free evidence plan
- Designated safe GitHub mutation or signed recorded-fixture plan
- Qt information architecture and rendered-evidence plan
- Documentation impact ledger
- Explicit deferred work and target-native/owner/live-service blockers

## Closeout blocker examples

- Private threads or accepted compositions do not survive restart.
- Legacy notes are labeled as reviewer- or AI-authored without evidence.
- Model or effort is inferred from a profile, executable, output style, or
  provider name.
- Agent session identity is reused as run identity across turns.
- Editing rewrites agent-authored bytes or discards derivation.
- A stale or multi-anchor selection is silently attached to a neighboring line.
- `Squash to Draft` performs or can accidentally perform a GitHub mutation.
- A successful publication cannot be correlated to its local draft/composition
  and server comment identity.
- A hidden or mandatory provenance marker is added to the public body.
- Qt reads SQLite, launches an agent, or calls GitHub directly.
- Private text crosses the agent boundary without exact explicit selection and
  dispatch-time revalidation.
- Transcript rendering loads an entire review or resets the whole model for one
  changed turn.
- Session pruning deletes run provenance referenced by retained turns.
- The feature exists only in a helper, fixture, or mock rather than the public
  BetterPullReview composition.
- Migration, crash, quota, security, accessibility, documentation,
  simplification, or designated-mutation evidence remains open.
