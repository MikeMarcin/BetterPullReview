---
id: BPR-0080
title: Prove RFC-BPR-0004 scale and high-frequency interaction
status: Done
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

## Implemented surface

- Replaced whole-segment nested move scanning with normalized-line indexing,
  unique anchors, maximal exact moved-sub-block refinement, and independent
  deterministic work/segment ceilings. Budget exhaustion retains the original
  primary alignment and publishes no partial move metadata.
- Added a semantic index bound to one validated immutable result and indexed
  overloads for mapping, moves, detail, projection, navigation, selection,
  reconciliation, and interaction transitions. The Qt workbench retains and
  reuses one index for each accepted publication.
- Removed pane-wide text measurement from ordinary frame application. Explicit
  layout invalidations still coalesce and rebuild the measured axis.
- Replaced per-frame candidate concatenation/sort/deduplication with an ordered
  merge and added counters for range searches, visited candidates, built and
  painted paths, peak active records, semantic work, pane measurements, and
  capacity growth.
- Added deterministic large mostly-equal, many-small-change, very-large-
  insertion, 1,000-input, 100-resize, ambiguity, exact/one-below work-limit,
  segment-limit, and fail-closed validation fixtures.

## Verification record

- Selected core computation, core semantics, and application-host executables
  pass directly.
- The workbench and link-map executables pass serially and offscreen at 1.0x,
  1.25x, and 2.0x. Generated rasters in
  `build/dev/tests/adapters_qt/` were directly inspected.
- macOS Sequoia 15.7.7 arm64, Qt 6.11.1, 14 ideal threads: the 2,000/2,000-line,
  500-segment, 250-group workbench fixture recorded publish/input/event-drain
  observations of 59/8/1 ms at 1.0x, 59/5/1 ms at 1.25x, and 59/22/1 ms at
  2.0x. These are local facts, not universal latency or frame-rate promises.
- One thousand rapid inputs preserve their exact summed/clamped subpixel
  position, apply 1,000 frames, and coalesce repaint delivery once without a
  semantic query, index rebuild, unchanged-axis rebuild, or pane scan. A
  100-resize storm performs one axis rebuild and two pane measurements without
  a semantic query or index rebuild. Peak and cumulative counters constrain
  every candidate lookup, path build, and paint in the trace. A move-enabled
  selected group resolves six visible overlays through retained visibility
  marks without a per-frame sort or endpoint search. Visible-path selection
  remains below retained changed entries and grows no hot-path capacity after
  setup.
- The full developer build, `coding-format-check`, touched-unit strict tidy,
  both architecture tests, documentation verifier, `git diff --check`, and
  immutable-RFC check pass. No GitHub Actions workflows exist. Independent
  closeout review reports no blocker, nonblocking risk, or verification gap
  within the BPR-0080 scale/indexed-interaction slice.

Target-native Windows verification remains open and nonblocking. Offscreen
forced-scale evidence is not a target-native System-theme claim and does not
close RFC-BPR-0005's planned presentation-polish pass.
