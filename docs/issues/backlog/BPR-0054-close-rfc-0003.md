---
id: BPR-0054
title: Verify and close RFC-BPR-0003
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0035, BPR-0048, BPR-0049, BPR-0050, BPR-0051, BPR-0052, BPR-0053, BPR-0055, BPR-0056, BPR-0059]
---

# Verify and close RFC-BPR-0003

Replay the complete claim and architecture-conformance matrices from clean
state. Directly inspect core policy, host staging and persistence, real
installed-Git and native smart transport, cache/ref/catalog recovery,
force-push retention, exact aliases/selections, rendered Qt behavior,
security boundaries, package/runtime dependencies, and platform-native
evidence.

Resolve every documentation-ledger row, perform a skeptical final diff review,
move every RFC card to `done/`, and mark RFC-BPR-0003 Auto-Verified only when
all claims agree and RFC-BPR-0002 has already closed in canonical order.

Closeout requires fresh `dev`, `core-only`, sanitizer, tests-disabled Release,
macOS, Linux, and real Windows matrices; safe fixture and hostile-canary
evidence; architecture/runtime/binary-dependency gates; immutable-base
documentation review; original-pack validation; workflow absence; and
`git diff --check`.
