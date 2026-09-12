---
schema: "implement-rfc.card.v1"
id: "BPR-0728"
title: "The conversations drawer goes narrower than 375 pixels in Read"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"205ac8b2-30fd-4339-8d08-51c621ab7fc8"}
acceptance: [{"criterion":"AC1 qt_workbench_window_scale_{100,125,200} pin a handle drag of the conversations drawer to 200 pixels in Read, its persistence across Home and back, and no clipped control at that width","outcome":"pass","revision":"working tree over a067a29f","evidence":{"kind":"command","detail":"test_the_conversations_drawer_narrows_below_its_default_in_read: Read opens at 375, a drag lands at 200 and the host hears 200, every visible control is reachable (in the drawer or within the horizontal scroll range), a drag to 100 stops at the shared panel floor, Home and back present the republished 200; the three scales pass"}},{"criterion":"AC2 qt_composed_github_scale_100, application_composition_contract and the UIforETW contract pass with any digest repinned","outcome":"pass","revision":"working tree over a067a29f","evidence":{"kind":"command","detail":"qt_composed_github_scale_100 (BPR-0527 pin restated: no sideways scroll at the default width), application_composition_contract and application_uiforetw_selected_diff_contract pass; no digest names workbench_window.cpp, none repinned"}},{"criterion":"AC3 the owner reads code in Read on the signed bundle with the drawer narrowed and accepts it in an attended session","outcome":"pass","revision":"2cdc0e57","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0728-conversations-drawer-width, revision app-release-64d9e46a-2026-09-11.3 (bundle built at 2cdc0e57, executable sha256 64d9e46a0fa7cff7e37d778d329a8e71bcf6ff13547bbc82eadb547ebb252827), fingerprint f61c6e2f549a2b87de6823d61b36533cde4fd3216a87de9ba9eea47db8dc7d66, decision 8a907ecd-4aef-4596-a035-320365d12b40, actor human, 2026-09-11T23:47:54Z: accept. The handle takes the drawer below 375 in Read, controls stay reachable and the width survives Home and back: accepted. The note names a separate defect, the horizontal-resize cursor showing only for a moment over the handle, carded as BPR-0739; it is not this criterion."}}]
---

# The conversations drawer goes narrower than 375 pixels in Read

The owner chose, on the options candidate `BPR-0724-read-width` in the
v006 attended session (decision `205ac8b2`, 18:42:22Z), "Keep the
conversations drawer in Read but let it go narrower than 375 pixels"
(with the single-pane layout,
[BPR-0729](../10-backlog/BPR-0729-a-single-pane-read-layout-in-the-manner-of-p4merge.md),
as a separate slice). The context is
[BPR-0724](BPR-0724-read-mode-leaves-too-little-horizontal-room-for-the-code.md):
"it was difficult to read the code because of a lack of horizontal area
for the text display".

## Scope

- The conversations drawer's minimum width
  (`conversation_drawer_default_width`, 375, in `src/adapters/qt`) is a
  default, not a floor: the splitter handle drags it down to the presented
  floor the other drawers use (160 pixels, BPR-0662), and the drawer's
  contents reflow or scroll at that width rather than clipping (the thread
  cards, the filter row, the review editor).
- The dragged width persists as any drawer width does (BPR-0662) and is
  not lifted back to 375 on the next launch.
- Owned paths: `src/adapters/qt` conversations drawer, drawer panel and
  workbench window sizing, their tests and the UIforETW probe if it pins
  the 375 figure.

## Acceptance

- `qt_workbench_window_scale_{100,125,200}` pin a handle drag of the
  conversations drawer to 200 pixels in Read, its persistence across Home
  and back, and no clipped control at that width.
- `qt_composed_github_scale_100`, `application_composition_contract` and
  the UIforETW contract pass with any digest repinned.
- The owner reads code in Read on the signed bundle with the drawer
  narrowed and accepts it in an attended session.

## Verification record, 2026-09-11 (d11)

The 375 floor was the conversation scroll area's explicit minimum width
(`src/adapters/qt/src/workbench_window.cpp`, the `workbenchConversationDrawerScroll`
setup); the drawer panel itself already stopped at the shared host floor
(`application::minimum_workbench_drawer_width`, 64) like the other drawers,
and `minimum_presented_drawer_width` (160) is not a drag stop for any drawer
but the BPR-0662 rule that lifts a persisted squeeze back to the design
width on the next launch. The scroll area now claims no minimum, so the
handle drags the drawer to the same floor the other drawers share, the host
hears the dragged width (`dispatch_workbench_width`) and republishes it, and
`central_sizes` presents any persisted width of 160 or more as it is;
375 stays the design width a squeeze is lifted to.

The column's own minimum is about 290 pixels (the widest row is the
*Classify selected conversation as defect* button); beneath that the
content scrolls sideways (`Qt::ScrollBarAsNeeded`) rather than clipping, so
every control stays reachable. At the default width no horizontal bar
appears, which the composed BPR-0527 pin now states in those terms.

Pinned by `test_the_conversations_drawer_narrows_below_its_default_in_read`
(`tests/adapters_qt/workbench_window_tests.cpp`): Read opens at 375, a
handle drag lands at 200 and the host hears 200, every visible control is
reachable at 200 (inside the drawer or within the scroll range), a drag to
100 stops where the other drawers stop and is heard, and after the host
republishes 200, Home and back present 200 rather than 375.
The UIforETW contract pins no 375 figure and no digest names
`workbench_window.cpp`.

Landed in the BPR-0727 finish commit, which carries the shared
`src/adapters/qt/src/workbench_window.cpp`, `tests/adapters_qt/workbench_window_tests.cpp`
and `tests/adapters_qt/composed_github_tests.cpp` edits of both drawer cards.

## Attended candidate, 2026-09-11 (v007)

The pending criterion above is candidate `BPR-0728-conversations-drawer-width` in
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md)
on the bundle built from the source at 2cdc0e57 (executable sha256
`64d9e46a…`), candidate revision `app-release-64d9e46a-2026-09-11.3`;
prepared and protected at sequence 311, undecided, no host running until
the owner asks for the session. The ask: Read's conversations drawer narrows below 375 by the handle, controls stay reachable, the width survives Home and back.

## Attended verdict, 2026-09-11 (v007): accepted

Session `cbd48b91…`, revision `app-release-64d9e46a-2026-09-11.3` (bundle built at 2cdc0e57, executable sha256 `64d9e46a…`), actor human; the session was handed back at sequence 322 with all ten candidates decided. Candidate `BPR-0728-conversations-drawer-width`, decision `8a907ecd`, 23:47:54Z: **accept**.

Verbatim:

> It does, but the mouse cursor for the horizontal resize flickers only for a brief moment and the returns back to the normal cursor so it's difficult to resize properly since you have no visual indicator you're cursor is over the border element.

Accepted; the note is a new defect, carded as [BPR-0739](BPR-0739-the-resize-cursor-over-the-conversations-drawer-handle-does-not-stay.md): the resize cursor flickers and returns to the arrow over the handle. Done.
