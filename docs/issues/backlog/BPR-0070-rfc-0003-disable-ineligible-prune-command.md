---
id: BPR-0070
title: Disable prune for ineligible selected epoch availability
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0067]
---

# Disable prune for ineligible selected epoch availability

The BPR-0067 public maintenance path remains fail-closed because the existing
core transition and host catalog validation reject stale, `pruned`,
`cache_deleted`, and `never_observed` targets. Its command projection currently
checks authority, catalog, pending lanes, request capacity, accepted-pair
presence, and epoch-anchor kind, but not whether the selected epoch anchor's
availability can enter a prune operation. The public menu can therefore offer
and confirm an operation that is guaranteed to be rejected safely.

Make the core command projection disable prune unless at least one selected
epoch target has an availability admitted by the exact prune transition. Keep
the host/transition validation as the final authority. Cover mixed epoch/review
anchors, same-epoch deduplication, available/unavailable/corrupt targets, prior
prune, cache deletion, never-observed state, stale confirmation, disabled
reason/accessibility presentation, and zero dispatch while disabled.

This is a nonblocking UX/command-projection refinement discovered by the final
BPR-0067 critical review. It is not an authorization bypass, destructive-scope
bug, or reason to delay RFC-BPR-0004 implementation.
