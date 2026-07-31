---
id: BPR-0040
title: Allocate RFC-BPR-0003 Git cache and epoch architecture
status: Ready
rfc: RFC-BPR-0003
depends_on: [BPR-0039]
---

# Allocate RFC-BPR-0003 Git cache and epoch architecture

Define the exact authority and typed boundary for app-owned Git repositories,
retained objects and refs, review-epoch persistence, deterministic cause and
selection policy, bounded catalog observations, asynchronous identity,
incremental Qt projection, composition, lifecycle, and explicit pruning.

Resolve before implementation:

- algorithm-tagged commit/object identities and exact ref-name validation;
- repository storage partitioning versus active account authorization;
- a nonreversible authority-scope identity that prevents retained-ref,
  ordinal, and catalog collisions when accounts share physical objects;
- credential-free native-HTTPS Git smart transport and installed-Git pack
  processing;
- persistence schema ownership and failure-atomic ref/catalog commit;
- epoch anchors, simultaneous base/head changes, unknown lineage, material
  equivalence, and `My last reviewed` ordering;
- hard quotas, paged history, destructive prune/delete semantics, crash
  recovery, and platform-specific process containment.

Add the real Git adapter scope and boundary checks without broadening the core,
Qt adapter, or RFC-BPR-0002 credential/network ports.
