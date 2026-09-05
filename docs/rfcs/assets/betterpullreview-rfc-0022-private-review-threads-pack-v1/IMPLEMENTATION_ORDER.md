# Implementation Order

This file explains the intended graph inside the pack. After import, live issue
card dependencies are the only mutable tracker.

## Critical path

```text
01 import/re-audit
  -> 02 architecture allocation
     -> 03 private-thread core
     -> 04 provenance/lineage/composition core
        -> 05 SQLite/migration/paging
        -> 06 runtime metadata capture
           -> 07 private-thread host
              -> 08 Squash-to-Draft host
                 -> 09 publication lineage

03/04/05/07 -> 10 Qt private/shared transcript
08/09/10    -> 11 Qt composition/provenance UI
07-11       -> 12 composition-root public path
05-12       -> 13 focused/security/performance evidence
10-12       -> 14 rendered UX/accessibility evidence
13/14       -> 15 docs/simplification
15          -> 16 independent review/closeout
```

## Phase 0 — Import and bind the work

- Reconcile RFC ID, live schema, issue IDs, and current dependency status.
- Retain this pack under `docs/rfcs/assets/` with a `LIVE_IMPORT` mapping.
- Re-audit the source facts before editing.
- Establish one architecture allocation and acceptance/evidence ledger.

No implementation lane starts from provisional IDs or an unimported pack.

## Phase 1 — Establish exact semantics

Implement Qt-free types and transitions first:

- private thread subjects and lifecycle;
- turn/revision/actor model;
- agent run identity and provenance facts;
- immutable derivation DAG;
- finding source identity;
- composition modes, source snapshots, target partitioning, duplicate advisory;
- the hard prohibition on composition-to-publication transitions.

This phase is the contract that prevents storage and UI convenience from
inventing policy.

## Phase 2 — Make the history durable

Allocate the next live ledger schema and implement:

- high-water sequences;
- normalized private thread/turn/revision/provenance/lineage/composition tables;
- durable draft correlation;
- legacy-note migration as `legacy_unknown`;
- cursor paging, quotas, purge, restart, WAL and process-kill recovery;
- compatibility facade only where needed, with no lasting dual authority.

Run schema and migration tests before building UI on the new store.

## Phase 3 — Capture provenance at the boundary

Extend invocation settings, protocol events, and terminal results to retain:

- one run identity per invocation;
- requested and configured model/effort;
- protocol-reported model/effort/version/remote IDs when genuinely available;
- unknown and conflict states;
- exact selected private source IDs/digests.

Protocol work and SQLite implementation can progress in parallel after core
semantics are fixed.

## Phase 4 — Host private conversation and composition

Wire the application host so:

- reviewer prompts commit before dispatch;
- agent completion correlates exactly and persists atomically;
- side chat and finding queue feed one durable private-thread authority;
- context selection is exact and revalidated;
- manual/copy/agent-assisted compositions all create immutable candidates;
- accepting a candidate creates a durable draft but no mutation;
- successful RFC-BPR-0008 mutation and reconciliation add publication lineage.

## Phase 5 — Present one coherent workflow

Implement a private/shared conversation navigation model, not another unrelated
panel. Add incremental transcript, provenance details, source selection,
composition workspace, target/split controls, Accept as Draft, and the separate
existing Publish action.

The UI must remain useful when runtime model/effort is unknown and when a thread
is stale or unplaceable.

## Phase 6 — Prove the boundary

Evidence must include:

- restart and crash recovery;
- v1-through-current migration and exact v5 note conversion;
- multiple runs per session and session pruning;
- full/partial/conflicting/unknown runtime metadata;
- private-source opt-in canaries;
- Squash-to-Draft mutation canary (zero calls);
- separate designated publish and server lineage;
- independent verification that public bytes contain no injected marker;
- quota, paging, database growth, active-set/changed-set, and model reset counts;
- keyboard, accessibility, scale, theme, focus, clipping, and public executable
  reachability.

## Parallel work

After cards 01-04:

- card 05 (SQLite) and card 06 (protocol metadata) may run in parallel;
- card 10 (Qt transcript shell) may begin against fake projections after core
  types and paged observation contracts stabilize;
- documentation can be drafted alongside implementation but is not complete
  until actual paths, limits, commands, and evidence exist.

Do not parallelize competing edits to the draft state machine, application
composition root, or review-ledger schema without explicit ownership and an
integration card.
