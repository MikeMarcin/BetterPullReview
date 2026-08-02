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

Replace BPR-0073's conservative whole-one-sided-segment move scan with a
deterministic bounded/indexed path, and decide moved sub-block decomposition
without changing the monotonic delete-plus-insert primary alignment. Cover a
moved exact sub-block adjacent to unrelated inserted/deleted text, repeated
ambiguous candidates, and an explicit work-limit boundary. Do not introduce
permanent crossing move ribbons.

## Required direct evidence

Ordinary scroll performs no semantic diff/group/pair recomputation or unchanged
axis rebuild, selects ribbons in `O(log S + V)`-equivalent bounded work, and
does not scan/paint all segments. Rapid input/resizes preserve motion and final
position. Move detection is bounded under adversarial many-segment input and
either emits stable sub-block metadata or explicitly leaves ambiguous content
unrelated. Record facts without overclaiming universal frame-rate guarantees.

Index the BPR-0074 semantic lookup path so repeated line mapping, pair/group/
move resolution, detail projection, and change projection do not repeatedly
validate or linearly scan the complete immutable result. Preserve the current
fail-closed public boundary and exact semantic answers while proving ordinary
selection/navigation work scales with indexed or affected records.
