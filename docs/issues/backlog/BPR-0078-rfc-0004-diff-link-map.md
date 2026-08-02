---
id: BPR-0078
title: Implement the DiffLinkMap divider
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0074, BPR-0076, BPR-0077]
---

# Implement the DiffLinkMap divider

Implement one custom-painted center component, preferably a wide
`QSplitterHandle`, using filled cubic interval ribbons and empty-span wedges.
Use retained geometry, ordered visible-run culling, pane-edge attachment,
theme/selection/hover states, stable-ID hit testing, click selection, reveal,
and platform drag-threshold splitter behavior.

Do not instantiate one widget per ribbon. Ordinary correspondence remains
monotonic and noncrossing; moved relations use markers or a temporary
selected/hover overlay.

## Required direct evidence

Render replacement/insertion/deletion geometry at 1x, fractional, and 2x
scales; inspect seams/clipping; hit-test after scroll/resize; prove selection
identity, move-marker topology, bounded visible paths, accessible navigation,
and no synthetic top-pane rows.
