---
id: BPR-0076
title: Implement Qt measured alignment axis
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0074]
---

# Implement Qt measured alignment axis

Measure live left/right line-boundary document coordinates and build one
cached piecewise alignment axis. Each positive segment extent is the maximum
of measured left/right extents; empty sides project to a constant boundary and
unequal replacements converge at one comparison endpoint. Add indexed lookup,
legal end caps, clamping, and explicit invalidation.

Measured pixels remain Qt-local. Rebuild only for accepted diff/file/epoch,
font, zoom, line spacing, tabs, DPI/screen, viewport/divider, wrap/fold, or
document-layout changes—not ordinary vertical scroll.

## Required direct evidence

Prove continuity, zero-extent safety, insertion/deletion pinning, unequal
ratios, different viewport heights, start/end reachability, DPI/zoom/resize
invalidation, semantic-ID retention, and no Qt type or pixel state in core or
durable host state.
