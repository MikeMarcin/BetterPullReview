---
schema: "implement-rfc.card.v1"
id: "BPR-0739"
title: "The resize cursor over the conversations drawer handle does not stay"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"8a907ecd-4aef-4596-a035-320365d12b40"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1 A Qt test pins that after a modality change and after a host-republished layout the handle under a resting pointer still reports the split cursor, and fails at HEAD if the cause is one of 1, 2 or 4","outcome":"pass","revision":"65c47ec6","evidence":{"kind":"command","detail":"test_the_split_cursor_stays_over_a_resting_handle in tests/adapters_qt/workbench_window_tests.cpp: the conversations handle under a resting pointer stays under it with Qt::SplitHCursor at rest, after a Discuss/Read modality change, after the host republishes the layout and after an answered drag; the files handle the same. The pointer sequence passed at e22b495a before any product change, which is the finding: causes 1, 2 and 4 do not hold (present() only toggles visibility, the handles are created once, relayouts return the handle to the same column). The seam pins (the drawer's edge rule in Mid, Highlight under the pointer, rule at the drawer's edge of each handle) fail without the new handle. Diagnosis on the card; logs build/logs/bpr-0739-*.log; cocoa probe held NSCursor.resizeLeftRightCursor for 1.1 s at rest (build/logs/bpr-0739-cocoa-diag-1.log)."}},{"criterion":"AC2 qt_workbench_window_scale_{100,125,200} and qt_composed_github_scale_100 pass","outcome":"pass","revision":"65c47ec6","evidence":{"kind":"command","detail":"ctest: qt_workbench_window_scale_100/125/200, qt_composed_github_scale_100 and architecture_boundaries pass (build/logs/bpr-0739-focused-run.log, build/logs/bpr-0739-wb-run-9.log after the tidy rename); format_check and tidy --warnings-as-errors clean on the touched files (build/logs/bpr-0739-tidy-2.log). At 1.25 the resting-pointer sequence is read only at whole-number scales (offscreen pointer round trip lands a device pixel beside the handle after the Discuss round trip); the seam pins run at all three."}},{"criterion":"AC3 The owner resizes the conversations drawer on the signed bundle with the cursor visible throughout, in an attended session","outcome":"pending","revision":"65c47ec6","evidence":{"kind":"observation","detail":"Asked in the v008 attended packet on the rebuilt tests-disabled bundle, with the hover highlight (a state the asset does not draw) shown for the owner's verdict."}}]
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

## Diagnosis, 2026-09-11 (d17)

Causes 1, 2 and 4 do not hold. `workbench_drawer_panel::present()` only
toggles visibility; the splitter's four children are added once and never
re-parented, so no handle is re-created (4). A Qt test that rests the
pointer on the conversations handle, changes modality (Discuss widens the
drawer and Read returns it), has the host republish the layout, and drags
the handle with the host answering, finds the handle still under the
pointer with `Qt::SplitHCursor` at HEAD (1, 2): the relayouts return the
handle to the same column and Qt's tracking follows it. Under the cocoa
platform the same window held `NSCursor.resizeLeftRightCursor` for the
1.1 seconds sampled with the pointer resting; a plain `QSplitter` probe
held it as long as the pointer stayed.

The cause is 3. `handleWidth()` is the macOS default, 7 pixels, and the
style paints the handle in the window colour: in the owner's v007 capture
(`owner-epoch-read-2026-09-11.png`, columns 1908-1921 at 2x) the handle is
7 logical pixels of exactly the background between the comparison's
18-pixel scroll-bar trough and the drawer. There is nothing to aim at, and
the trough's edge reads as the border; the cursor changes while the
pointer crosses the seven pixels and the arrow returns over the trough or
the drawer. The design asset draws the drawer's edge (`.files`
`border-right`, `.cmts` `border-left`, one pixel of `--rule`), the rule
the collapsed rail already paints in `QPalette::Mid`.

## Change

`workbench_central_splitter` now creates `workbench_splitter_handle`,
which keeps the platform width and paints the drawer's edge rule on its
drawer side (left edge for the files and repositories handles, right edge
for the conversations handle) in `QPalette::Mid`, and in
`QPalette::Highlight` while the pointer is over it, so the seam is visible
and the hot zone announces itself before the cursor changes. The width and
the widths the host persists are untouched. The hover colour is a state the
asset does not draw; it is shown for the owner's verdict in v008.

The test also records that at a fractional scale (1.25) the offscreen
round trip of the pointer position lands a device pixel beside the handle
after the Discuss round trip, so the resting-pointer sequence is read at
whole-number scales and the seam pins run at all three; whether Windows
fractional scaling drops the hover after a relayout on a real display is
the Windows lane's to observe.
