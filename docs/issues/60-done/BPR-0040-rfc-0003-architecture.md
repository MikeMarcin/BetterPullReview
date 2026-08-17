---
id: BPR-0040
title: Allocate RFC-BPR-0003 Git cache and epoch architecture
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0039]
completed: 2026-07-30
---

# Allocate RFC-BPR-0003 Git cache and epoch architecture

Define the exact authority and typed boundary for app-owned Git repositories,
retained objects and refs, review-epoch persistence, deterministic cause and
selection policy, bounded catalog observations, asynchronous identity,
incremental Qt projection, composition, lifecycle, and explicit pruning.

Resolve before implementation:

- algorithm-tagged commit/object identities and exact ref-name validation;
- repository storage partitioning versus active account authorization;
- nonreversible cache and PR authority-scope identities that prevent
  retained-ref, ordinal, and catalog collisions without sharing physical
  objects across accounts or profile revisions;
- credential-free native-HTTPS Git smart transport and installed-Git pack
  processing;
- persistence schema ownership and failure-atomic ref/catalog commit;
- epoch anchors, simultaneous base/head changes, unknown lineage, material
  equivalence, and `My last reviewed` ordering;
- hard quotas, paged history, destructive prune/delete semantics, crash
  recovery, and platform-specific process containment.

Add the real Git adapter scope and boundary checks without broadening the core,
Qt adapter, or RFC-BPR-0002 credential/network ports.

## Result

- Allocated a dedicated compact epoch core and application host downstream of
  an exact post-acceptance GitHub handoff and invalidate-before-clear message.
  Qt projections are not an internal command bus.
- Selected algorithm-tagged binary Git identities, random cache/PR scopes, a
  random catalog incarnation, checked never-reused ordinals, composite cause
  facts, exact material identity, revision-bound paging, and explicit
  no-fallback alias/pair semantics.
- Isolated every profile-revision/account/repository object database. Native
  HTTPS remains the sole credential-bearing network path; a bounded app-owned
  protocol-v2 client supplies only raw pack bytes to fixed, credential-free
  `git index-pack` and local object/ref commands.
- Defined versioned private refs and a dedicated BPEC v1 epoch catalog with
  SQLite visibility authority, point-of-no-cancellation versus
  durable-success semantics, typed recovery, explicit prune/cache deletion,
  hard quotas, and no automatic retention loss.
- Preserved native System palette/style/font inheritance for the interim
  control, RFC-BPR-0005's distinct presentation-polish/native visual gate,
  RFC-BPR-0006's selectable appearance ownership and fresh-install System
  default, the nonblocking Windows hardware backlog, and zero GitHub Actions
  involvement.

## Evidence

- Three independent architecture reviews covered core policy and identity,
  Git/protocol/cache security, and catalog/Qt/composition behavior; the
  corrected documents agree on authority, ref grammar, causes, cancellation,
  catalog linearization, and lifecycle.
- Apple Git 2.39.5 directly accepted the selected isolated
  `index-pack --stdin --strict --check-self-contained-and-connected
  --max-input-size --threads=1 --no-rev-index` shape, produced one validated
  pack result, and exposed the expected commit from the target bare
  repository.
- The documentation dependency/status gate passes directly and in the focused
  `dev` and `core-only` CTest matrices (3/3 each). The original source-pack
  validator reports 14 RFCs and 17 normative documents, the official
  Git/GitHub/SQLite reference audit is recorded, tracked and on-disk workflow
  counts are both zero, and `git diff --check` passes.
