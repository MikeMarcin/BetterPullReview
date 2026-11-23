---
schema: "implement-rfc.card.v1"
id: "BPR-2362"
title: "Scroll the code with a draft in Comments"
status: "Done"
rfcs: []
depends_on: ["BPR-2359"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","origin":"owner attended review, 2026-11-23","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-2279","BPR-2292","BPR-2354"],"reservation":"docs/issues/_reservations/BPR-2362--BPR-2362--f3ee1fd82a29.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"766d0e65e54c2ce5f3d7b9a5191bfda70776cfc2","evidence":{"kind":"command","detail":"qt_composed_inline_drawer_scale_100/125/200 deliver phased wheel events to the composed window. Upward movement and momentum reach the scrollable comparison over the open draft editor, collapsed draft and adjacent blank cards area."}},{"criterion":"AC2","outcome":"pass","revision":"766d0e65e54c2ce5f3d7b9a5191bfda70776cfc2","evidence":{"kind":"command","detail":"The same course holds a long draft editor's gesture at the top through momentum before and after ScrollEnd, then gives a fresh upward gesture at the top to the comparison. qt_conversation_cards_view and qt_nested_scroll_latch preserve code-box and nested vertical scrolling."}},{"criterion":"AC3","outcome":"pass","revision":"766d0e65e54c2ce5f3d7b9a5191bfda70776cfc2","evidence":{"kind":"observation","detail":"macOS arm64 Qt 6.12.1 Debug (-g0): original product failed the upward update over the open draft. Thirteen focused CTest rows and documentation gates pass on the candidate. Hidden Cocoa passes the draft-scroll segment and later reaches the previously recorded reply-focus failure."}},{"criterion":"AC4","outcome":"pass","revision":"157a4487e14e3d73c4b8014099fa25c4278b18a2","evidence":{"kind":"observation","detail":"Owner accept in attended session 484776b1fd455a800012d47912774f04, decision 11b185bb-5d93-452a-978e-a87dabf6cdfb, candidate fingerprint 682dcf9d87833fd159d3e2e113d786e287630a332359232e189823a097d66669. Owner note: Something kind of weird when you collapse an empty draft it just disappears."}}]
---

# BPR-2362: Scroll the code with a draft in Comments

A new upward trackpad gesture over the Comments drawer can stall when the
drawer has a draft, whether the draft is open or collapsed. The comparison
still has room to scroll.

## Acceptance

- AC1. In Review, a new vertical gesture over an open or collapsed draft,
  or the surrounding Comments area, scrolls the comparison when the card
  view is at its own limit and the comparison can move. Momentum continues
  the movement.
- AC2. A long draft editor keeps the gesture through momentum when it
  takes the first movement. A new gesture at its limit can reach the
  comparison. Horizontal gestures can still operate nested code boxes.
- AC3. Reproduce the failure with platform-style phased wheel events and
  verify the fix with Qt 6.12.1.
- AC4. Obtain a fresh owner verdict on the frozen correction.

## Plan

Extend the composed inline drawer course and retain the long-editor and
sideways-code coverage. Make the latch consider the drawer's forwarding
path for vertical gestures. Run the focused Qt, architecture, formatting
and documentation checks before handing off for attended review.

## Result

The nested latch now offers the cards the first vertical movement even
when their own bar is at its limit. The cards can forward that movement
to the comparison. The long editor can still own its vertical gesture
through momentum, and fenced code boxes can own a horizontal gesture.

On macOS arm64, the composed 100% course reproduced the stall in the
original product with Qt 6.12.1. The candidate passed the composed inline
drawer, long-thread and general GitHub courses at 100%, 125% and 200%,
plus the conversation cards, nested scroll, qualification inventory and
architecture checks. Format and the documentation gate passed. Clang-tidy
reported the existing `row_rect` naming violation in the cards view.
Hidden Cocoa also reached the draft-scroll pass marker. That run later
hit the previously documented hidden-Cocoa reply-focus assertion
([[BPR-2359]]).

The owner accepted physical trackpad scrolling on the integrated freeze.
The owner also observed that collapsing an empty draft makes it disappear.
That observation concerns draft behavior outside the scrolling criterion.
