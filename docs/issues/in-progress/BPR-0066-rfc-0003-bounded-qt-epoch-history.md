---
id: BPR-0066
title: Bound the Qt epoch-history working set
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0046, BPR-0047, BPR-0056]
---

# Bound the Qt epoch-history working set

The RFC-BPR-0003 Qt selector currently appends every accepted 100-epoch page.
A valid catalog may retain 100,000 epochs, allowing the presentation model to
grow to 200,000 anchor rows and making stable-ID operations repeatedly scan the
resident history. The full history must remain host/catalog-owned and paged;
Qt may retain only a fixed working set independent of the durable catalog
limit.

Select one bounded or virtual page-window design before implementation. It
must preserve exact revision-bound cursor acceptance, stable epoch and anchor
identity, independent left/right selection, complete selected OIDs even when a
selected anchor is outside the visible window, named comparison behavior,
explicit availability, incremental Qt signals, persistent-index legality,
catalog-revision/authority replacement, and access to older retained history.
Do not move catalog queries or semantic paging policy into Qt, copy the catalog
into core state, reset the model for ordinary page movement, or silently cap
the user's reachable history.

Direct tests must drive more pages than the resident limit and prove the row
cap, deterministic eviction/window movement, continued older-history access,
selection pinning or exact out-of-window presentation, forward/restart access
to recent history, stable signals/indexes, stale-page rejection, and bounded
lookup behavior. Re-run the real public composition/render and architecture
boundaries. Track any required core/host paging extension explicitly rather
than weakening these claims.
