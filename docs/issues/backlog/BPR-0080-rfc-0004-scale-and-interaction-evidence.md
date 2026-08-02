---
id: BPR-0080
title: Prove RFC-BPR-0004 scale and high-frequency interaction
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0079]
---

# Prove RFC-BPR-0004 scale and high-frequency interaction

Add representative large mostly-equal, many-small-change, very-large-
insertion, rapid-input, and resize fixtures. Instrument semantic recomputes,
axis rebuilds, comparison lookups, visible/near-visible paths, allocations or
equivalent bounded-work facts, repaint coalescing, final subpixel position,
and interaction latency with platform/hardware context.

## Required direct evidence

Ordinary scroll performs no semantic diff/group/pair recomputation or unchanged
axis rebuild, selects ribbons in `O(log S + V)`-equivalent bounded work, and
does not scan/paint all segments. Rapid input/resizes preserve motion and final
position. Record facts without overclaiming universal frame-rate guarantees.
