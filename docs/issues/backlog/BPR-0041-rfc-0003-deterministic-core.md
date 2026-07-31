---
id: BPR-0041
title: Implement deterministic review-epoch core policy
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0040]
---

# Implement deterministic review-epoch core policy

Add compact, Qt/Git/filesystem/process/clock/persistence-free state and strong
types for active pull-request revision, review epochs, exact comparison
anchors, catalog and comparison revisions, generations, requests, and
diagnostics.

Implement deterministic policy for initial observation, author upload,
force-push/rebase, base update, base-branch change, historical import,
material equivalence, default Branch-base-to-Latest selection, named aliases,
arbitrary exact pairs, unavailable anchors, idempotence, and stale or
duplicate completion rejection. Explicit time and bounded ancestry/material
facts arrive as observations; the core never discovers them.

Prove full-context acceptance, identity-switch invalidation, generation
overflow failure, deterministic replay, compact state, alias legality, and
the absence of Git, Qt, provider, I/O, clock, thread, and persistence
dependencies.
