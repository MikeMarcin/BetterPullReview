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
acceptance: [{"criterion":"AC1 A Qt test pins that after a modality change and after a host-republished layout the handle under a resting pointer still reports the split cursor, and fails at HEAD if the cause is one of 1, 2 or 4","outcome":"pass","revision":"65c47ec6","evidence":{"kind":"command","detail":"test_the_split_cursor_stays_over_a_resting_handle in tests/adapters_qt/workbench_window_tests.cpp: the conversations handle under a resting pointer stays under it with Qt::SplitHCursor at rest, after a Discuss/Read modality change, after the host republishes the layout and after an answered drag; the files handle the same. The pointer sequence passed at e22b495a before any product change, which is the finding: causes 1, 2 and 4 do not hold (present() only toggles visibility, the handles are created once, relayouts return the handle to the same column). The seam pins (the drawer's edge rule in Mid, Highlight under the pointer, rule at the drawer's edge of each handle) fail without the new handle. Diagnosis on the card; logs build/logs/bpr-0739-*.log. Corrected 2026-09-11 (d19): the 'cocoa probe held NSCursor.resizeLeftRightCursor for 1.1 s at rest' reading of build/logs/bpr-0739-cocoa-diag-1.log was wrong; that log records the real pointer 1164 pixels from the handle and prints FAILED, so it shows only that Qt calls [NSCursor set], not that the cursor holds under the pointer. See the correction section on this card."}},{"criterion":"AC2 qt_workbench_window_scale_{100,125,200} and qt_composed_github_scale_100 pass","outcome":"pass","revision":"65c47ec6","evidence":{"kind":"command","detail":"ctest: qt_workbench_window_scale_100/125/200, qt_composed_github_scale_100 and architecture_boundaries pass (build/logs/bpr-0739-focused-run.log, build/logs/bpr-0739-wb-run-9.log after the tidy rename); format_check and tidy --warnings-as-errors clean on the touched files (build/logs/bpr-0739-tidy-2.log). At 1.25 the resting-pointer sequence is read only at whole-number scales (offscreen pointer round trip lands a device pixel beside the handle after the Discuss round trip); the seam pins run at all three."}},{"criterion":"AC3 The owner resizes the conversations drawer on the signed bundle with the cursor visible throughout, in an attended session","outcome":"fail","revision":"35438244","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0739-splitter-seam, revision app-release-e4fdb0ea-2026-09-11.4 (bundle built at 35438244, executable sha256 e4fdb0ea8e250f42da2bfa3a61c501fbdbb6ffd6d92d929912f6b25b44b2757f), fingerprint 380b3fb203cc5831de9addeebad7d85e3e49665706c7220ff9a68573a9f9c128, decision a374890c-b692-4ada-afb9-6ccd278d7645, actor human, 2026-09-12T04:34:54Z, one screenshot: reject. Verbatim: 'still fails, same issue, at least it draws the seam now, cursor didnt show up in screenshot, but i promise its right over that blue vertical line and looks like a normal cursor not a resize cursor.' The second criterion, a visible seam taking the highlight under the pointer, is satisfied and is what the screenshot shows. The first is not: the cursor stays an arrow. The highlight is painted only from underMouse() (workbench_window.cpp:6309), so the handle held Qt's pointer state while the cursor was wrong, which refutes cause 3 as recorded in the d17 diagnosis; the defect is cursor delivery after Qt's enter. Carded as BPR-0746 with the discriminating check. The question whether the highlight state should go is unanswered."}}]
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

## Attended verdict, 2026-09-11 (v008): rejected

Session `cbd48b91…`, revision `app-release-e4fdb0ea-2026-09-11.4` (bundle
built at 35438244, executable sha256 `e4fdb0ea…`), actor human; handed back
at sequence 348 with all six candidates decided. Candidate
`BPR-0739-splitter-seam`, decision `a374890c`, 2026-09-12T04:34:54Z, one
screenshot
([the lit seam](../../verification/reviews/rfc-bpr-0028-attended-v008/captures/owner-splitter-seam-highlight-2026-09-11.png)):
**reject**. Verbatim:

> still fails, same issue, at least it draws the seam now, cursor didnt show up in screenshot, but i promise its right over that blue vertical line and looks like a normal cursor not a resize cursor.

Against the two criteria:

- The seam is visible and takes the highlight under the pointer: **satisfied**,
  and the capture shows it — a saturated one-pixel rule the height of the panes
  between the comparison's scroll bar and the drawer.
- The resize cursor shows and stays: **still fails**. macOS omits the pointer
  from a screen capture, so the arrow is the owner's report rather than the
  image; the image supplies the harder half of the evidence anyway.

The highlight is the finding. It is painted only from `underMouse()` —
`Qt::WA_UnderMouse`, set by Qt's enter dispatch (`workbench_window.cpp:6303-6312`,
the pen at `:6309`) — so at the moment of the capture the handle held Qt's
pointer state and the effective cursor was still an arrow. **Cause 3 as
recorded in the d17 diagnosis is refuted**: the hot zone is not too narrow to
hit, it was hit. So are causes 1, 2 and 4 (relayout hypotheses, excluded in
d17). The defect is cursor *delivery* after Qt's enter, which none of the four
candidates covers, and it is not visible to the offscreen test: that test
reads Qt's own bookkeeping under a platform with no cursor at all.

Carded as [BPR-0746](BPR-0746-the-resize-cursor-is-not-delivered-over-the-seam-on-the-signed-bundle.md),
with three ranked hypotheses about the macOS cursor path and the logging check
that discriminates them on the real windowed application before any product
change.

The open question this card carried — the `Highlight` hover state is a state
the design asset does not draw, say if it should go — is **unanswered**. "At
least it draws the seam now" is not a verdict on the hover colour. It is asked
again.

This card stays parked on human-review; AC3 is asked again on a bundle
carrying BPR-0746.

## Correction to AC1's evidence, 2026-09-11 (d19)

AC1's evidence said the "cocoa probe held `NSCursor.resizeLeftRightCursor` for
1.1 s at rest". `build/logs/bpr-0739-cocoa-diag-1.log` does not support that.
The log records `pointer at local x=-1164` — the real pointer 1164 pixels left
of the handle, over the files region — while reporting `underMouse=1` and
`nscursor=resizeLeftRight` at t=403 ms, and the run printed `FAILED`. The
Enter came from an event sent directly to the window, and the cursor value from
Qt's own synchronous `[NSCursor set]` with the pointer somewhere else
entirely. The log therefore shows that Qt calls `[NSCursor set]`, which nobody
disputed, and shows nothing about what AppKit does on the next real pointer
motion with the pointer actually on the handle.

The AC1 test itself is unaffected and still passes; only the probe sentence was
wrong. It is corrected in the record rather than removed, because the d17
diagnosis leaned on it and BPR-0746 needs to know why the evidence looked
green.

## Owner answer, 2026-09-12 (mandate r11): the hover highlight, provisionally

Asked whether the `Highlight` hover state should go, since the design asset
does not draw it, the owner answered verbatim:

> I'm not sure what this is, but leaning towards keep it.

That is a leaning, not a verdict, and the first clause is the part that
matters: the owner does not know which element the question was about. It is
not treated as settled, and the hover state is **kept unchanged** in the
meantime — a leaning towards keep and no change are the same action, so
nothing is being decided on his behalf by leaving it alone.

The question is asked once more in v009, and differently. Re-asking "should
the hover highlight go?" would get the same answer for the same reason. The
v009 ask shows him the element instead: the one-pixel rule at the seam in its
two states side by side, `QPalette::Mid` at rest and `QPalette::Highlight`
under the pointer (`workbench_window.cpp:6303-6312`, the pen chosen at
`:6309`), named as the thing his "at least it draws the seam now" was about.
Then the question is answerable: the rule is accepted work either way, and
what is being asked is only whether it should brighten under the pointer when
the accepted asset draws it at one constant weight.

AC3, the resize cursor, is unchanged and still fails. [[BPR-0746]] carries
it; see that card for why no product change was attempted this round.

## Attended candidate, 2026-09-12 (v009)

The hover question is candidate `BPR-0739-seam-hover` in
[rfc-bpr-0028-attended-v009](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md),
on the bundle built from the source at aff9fc64 (executable sha256
`c2381f32…`, tests disabled, codesign clean), candidate revision
`app-release-c2381f32-2026-09-12.5`; prepared and protected at sequence 357,
undecided, no host running until the owner says so. It carries the owner's own
v008 capture cropped to the seam
([the crop](../../verification/reviews/rfc-bpr-0028-attended-v009/captures/seam-highlight-crop-from-v008.png))
and offers two options rather than a free-text question, because the v008 ask
failed on the owner not knowing which element was meant.

**AC3 is not asked in v009.** Nothing about cursor delivery changed this round,
so asking whether the cursor stays would be asking the owner to confirm a
failure already recorded. What is asked instead, as the separate candidate
`BPR-0746-cursor-probe`, is the logging run that [[BPR-0746]] needs; AC3 comes
back on a bundle carrying whatever that log leads to.
