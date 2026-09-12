---
schema: "implement-rfc.card.v1"
id: "BPR-0739"
title: "The resize cursor over the conversations drawer handle does not stay"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"8a907ecd-4aef-4596-a035-320365d12b40"}
---

# The resize cursor over the conversations drawer handle does not stay

The owner, 2026-09-11, accepting `BPR-0728-conversations-drawer-width` in
the v007 attended session (decision `8a907ecd`, 23:47:54Z), verbatim:

> It does, but the mouse cursor for the horizontal resize flickers only
> for a brief moment and the returns back to the normal cursor so it's
> difficult to resize properly since you have no visual indicator you're
> cursor is over the border element.

The width criterion is accepted and
[BPR-0728](../60-done/BPR-0728-the-conversations-drawer-goes-narrower-than-375-pixels-in-read.md)
is Done; the cursor is this card.

## What the code says

Nothing in `src/adapters/qt/src` sets a cursor on the central splitter or
overrides its handle: `workbench_central_splitter`
(`src/adapters/qt/src/workbench_window.cpp:6258-6266`) overrides only
`move_handle`, there is no `createHandle`, `setHandleWidth` or style sheet,
and the only `setCursor` calls are the rail's pointing hand
(`workbench_drawer.cpp:280`) and `usage_ring.cpp:162`. The only custom
`QSplitterHandle` is the comparison-internal `diff_comparison_splitter`
(`diff_link_map.cpp:83`, `:992`). Candidate causes, strongest first, none
confirmed:

1. `apply_drawer_widths()` (`:6220-6232`) calls `setSizes(central_sizes())`
   on modality change and from `apply_drawer_layout` (`:6217`); a relayout
   that moves the handle out from under the pointer leaves the arrow until
   the next move.
2. `splitterMoved` feeds `observe_handle_drag()` (`:6234-6255`), which
   reports widths to the host; if the host republishes a layout, the window
   re-sets sizes mid-hover (guarded by `ApplyingLayout`, not by a
   drag-in-progress flag).
3. `handleWidth()` is the platform default (never set; read at `:6322`), so
   the hot zone may be narrower than the visible seam on macOS.
4. `workbench_drawer_panel::present()` (`:527-552`) takes widgets out of and
   back into the column; a re-created handle drops its hover state.

## Scope

- With the pointer resting over the handle between the comparison and the
  conversations drawer (and the other drawer handles), the horizontal-resize
  cursor shows and stays until the pointer leaves the handle, in every
  modality, with and without a drag.
- Diagnose the cause above (or another) before changing anything; a wider
  handle is acceptable only if the seam it draws stays within the accepted
  asset.
- Owned paths: `src/adapters/qt/src/workbench_window.cpp` (the central
  splitter), `workbench_drawer.cpp` if the panel re-parenting is the cause,
  `tests/adapters_qt/workbench_window_tests.cpp`.

## Acceptance

- A Qt test pins that after a modality change and after a host-republished
  layout the handle under a resting pointer still reports the split cursor
  (`QSplitterHandle::cursor()` or the widget under the pointer), and fails
  at HEAD if the cause is one of 1, 2 or 4.
- `qt_workbench_window_scale_{100,125,200}` and `qt_composed_github_scale_100`
  pass.
- The owner resizes the conversations drawer on the signed bundle with the
  cursor visible throughout, in an attended session.
