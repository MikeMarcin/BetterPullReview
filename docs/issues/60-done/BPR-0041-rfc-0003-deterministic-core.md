---
id: BPR-0041
title: Implement deterministic review-epoch core policy
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0040]
completed: 2026-07-30
---

# Implement deterministic review-epoch core policy

Add compact, Qt/Git/filesystem/process/clock/persistence-free state and strong
types for active pull-request revision, review epochs, exact comparison
anchors, catalog and comparison revisions, generations, requests, and
diagnostics.

Implement deterministic policy for initial observation, author upload,
force-push/rebase, base update, base-branch change, historical import,
material equivalence, the default semantic `Whole PR` alias resolving the
latest live base/head pair, other named aliases, arbitrary exact pairs,
unavailable anchors, idempotence, and stale or duplicate completion
rejection. Explicit time and bounded ancestry/material facts arrive as
observations; the core never discovers them.

Prove full-context acceptance, identity-switch invalidation, generation
overflow failure, deterministic replay, compact state, alias legality, and
the absence of Git, Qt, provider, I/O, clock, thread, and persistence
dependencies.

## Result

- Added algorithm-tagged binary SHA-1/SHA-256 commit and tree identities,
  distinct authority/catalog/review/operation identities, bounded revision
  and generation types, compact state, typed events, typed host requests,
  deltas, diagnostics, and command projections.
- Implemented deterministic initial, historical, descendant-upload,
  non-fast-forward, unknown-lineage, base-update, base-branch, material,
  association-only, semantic-alias, arbitrary-pair, availability,
  cancellation, retry, and exact completion policy.
- Made authority loss and catalog conflict explicitly clear catalog,
  selection, and comparison projections. A replacement catalog incarnation
  cannot inherit or combine with an old comparison, and an invalid
  application session cannot enter a host request.
- Consolidated comparison retirement and ingestion startup without changing
  the core/host boundary. The finished state remains trivially copyable,
  trivially destructible, and below the 2 KiB limit.

## Evidence

- `cmake --preset core-only`, `cmake --build --preset core-only`, and
  `ctest --preset core-only --output-on-failure` pass 20/20.
- The focused `core_epoch_policy` and
  `architecture_boundary_contract` run passes 2/2, including exhaustive
  cause/alias/pair tables, replay, overflow, malformed-terminal,
  mixed-incarnation, projection-clear, and zero-session cases.
- An independent adversarial API/delta review found and then re-reviewed the
  catalog-incarnation, projection-clear, and session-admission repairs; its
  final verdict reports no remaining BPR-0041 blocker.
- Core dependency scans find no Qt, Git process, filesystem, persistence,
  network, clock, or thread dependency. The documentation validator and
  `git diff --check` pass.
