---
id: "RFC-DRAFT-0029"
title: "Keep diff line numbers aligned and Find stationary while scrolling"
status: "Proposed"
metadata: {"origin":"owner report, 2026-11-08","inspected_revision":"31559e616707","authoring":"author-rfc; frontend-design:frontend-design and desktop guidance loaded"}
---

# Keep diff line numbers aligned and Find stationary while scrolling

## Problem and grounded current behavior

The owner reports that, in the single-pane diff, line numbers shift relative
to their text while scrolling. The Find widget also shifts relative to the
window while the diff scrolls. These are owner observations; authoring has
not independently reproduced the running application's symptoms.

The inspected implementation provides two concrete explanations:

- In [source_pane](../../../../src/adapters/qt/src/diff_workbench_support.cpp),
  `set_within_line_offset` moves the native viewport upward by `AppliedOffset`
  and masks its overscan. For unified rows, `gutter_header` derives its height
  from that displaced viewport top. `paint_line_numbers` then adds this
  already-displaced height and subtracts `AppliedOffset` again. The number
  position therefore includes the residual twice while the text moves once;
  the heading and clipping boundary also depend on the residual.
- [place_source_find](../../../../src/adapters/qt/src/diff_workbench_find_and_editing.cpp)
  positions the floating Find widget from the raw viewport origin and size.
  Those describe the displaced backing widget, including overscan, rather
  than the stationary displayed source area. Repositioning Find at different
  residuals can therefore move its anchor. This placement is shared by split
  and unified views.

These are source-based diagnoses, not runtime verification. They conflict with
the existing [fractional scrolling contract](../../RFC-BPR-0073-smooth-diff-touchpad-panning.md#render-fractional-line-vertical-positions)
and the [accepted floating Find design](../../../issues/60-done/BPR-1503-find-in-pane-becomes-a-floating-vs-code-style-find-widget.md#design-accepted-2026-10-14).
This proposal restores those behaviors without changing their contracts.

## Proposed behavior and boundaries

### Line numbers follow source rows

In single-pane (unified) comparison, each base/head number stays vertically
aligned with its own rendered source row throughout continuous scrolling,
including a partially clipped first row. Row backgrounds, number-cell selection
and thread indications follow the same applied origin. The base/head heading
and its clipping boundary stay at the pane's fixed upper edge; source rows
scroll beneath them. Crossing a whole-row boundary or reversing direction
introduces no extra displacement between gutter and text.

### Find stays attached to the displayed pane

Keep the floating Find widget at the upper-right of its target pane's
stationary displayed source area, excluding backing-widget overscan. With
window and pane layout unchanged, scrolling either axis or revealing another
match leaves the widget's rectangle in window coordinates unchanged. This
applies in unified view and either split pane, including after a later repaint
or placement refresh.

Retain the existing insets, narrow-width fitting, focus and search behavior.
Actual window/pane resizing, explicit retargeting and presentation changes may
reposition it to the new target area; a taller query may change its height
while keeping its upper-right anchor. Opening or closing Find continues to
leave the displayed source extent unchanged.

### Scope and ownership

Correct the local Qt geometry and its regression coverage under the existing
[presentation ownership](../../../architecture/system-architecture.md#build-target-and-dependency-direction).
Preserve fractional scrolling, gesture filtering, keyboard reveal, source
identities and split-view shared vertical authority. Reuse measured geometry;
ordinary scrolling must not add whole-document scans, semantic recomputation
or unchanged-axis rebuilds. Search matching, host effects, persistence and
visual redesign are outside this correction. No material product choice is
open; the coordinate representation is an implementation decision.

## Acceptance

- **AC01:** Rendered unified comparisons containing unchanged, removed and
  added rows demonstrate the defined gutter alignment and fixed heading
  through several within-row offsets, whole-row crossings and reversal.
  Number and text row placement agree within one device pixel, including at
  clipped edges; clicking a visible number resolves its displayed source row.
- **AC02:** An open Find widget retains exactly the same window-relative
  rectangle through vertical and horizontal scrolling and match reveals in
  unified and both split panes. Refreshing placement at a nonzero residual
  produces no jump. The oracle uses the stationary displayed area independently
  of the raw backing-widget rectangle.
- **AC03:** After resize, code zoom, line-number visibility changes, Find query
  height changes and split/unified switching, the defined anchors are correct
  and subsequent scrolling retains them. Verify rendered alignment at 100%,
  125% and 200% scale with existing narrow-width Find fitting preserved.
- **AC04:** A composed production-workbench interaction and full-window render
  through the installed Git/cache path demonstrate both corrections. Existing
  fractional navigation and source Find behavior remain valid, and scroll
  observations retain the bounded-work rules under Scope and ownership.

Extend the existing [diff workbench geometry courses](../../../../tests/adapters_qt/diff_workbench_tests.cpp)
and [source Find public path](../../../../tests/end_to_end/source_find_public_path_tests.cpp).
The latter's current `expect_floating` helper compares Find with the same raw
viewport rectangle used by production placement; that comparison alone cannot
establish a stationary anchor. Compare actual painted gutter rows with native
text placement and observe Find across scroll events in window coordinates.
Draft validation is separate from implementation acceptance.
