---
id: BPR-0078
title: Implement the DiffLinkMap divider
status: Done
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

## Implemented surface

- `diff_comparison_splitter` creates one production `diff_link_map` wide
  `QSplitterHandle` between two natural-layout panes.
- The handle retains changed measured intervals, stable segment/group identity,
  exact move-relation endpoints, cached visible paths, and no child per ribbon.
- Same-comparison shared-scroll frames place cubic ribbons and empty-side wedges
  in viewport coordinates; different-comparison and invalid frames fail closed.
- Pointer, double-click, group-aware keyboard navigation, polite accessible
  selection announcements, and native-threshold splitter dragging share the
  retained semantic identities. Scroll, resize, replacement, and clear paths
  reconcile stationary-pointer hover and publish downstream resets.
- Exact move markers and temporary overlays use the source-left and destination-
  right interval midpoints even when the primary ribbon is strongly displaced.
- Palette/style/font/theme events repaint inherited presentation without
  changing geometry or selection. Target-native System-theme composition and
  the planned presentation-polish pass remain BPR-0079/RFC-BPR-0005 work.

## Verification record

- `qt_diff_link_map_scale_{100,125,200}` passes through a live two-pane splitter,
  production handle, shared controller, and shared scrollbar.
- The fixture directly checks replacement/insertion/deletion topology, exact
  pane-edge attachment, hit identity after scroll/resize/remeasurement, click
  versus drag threshold, stationary-hover reconciliation and clear publication,
  hover/reveal, a captured polite accessibility announcement, displaced exact
  pane-edge move markers, temporary-only overlay, bounded visible candidates,
  and unchanged natural document block counts.
- `diff-link-map-{100,125,200}.png` were directly inspected at 1.0x, 1.25x, and
  2.0x; selected and neutral fills are clipped cleanly and meet both pane edges
  without a blank seam.
