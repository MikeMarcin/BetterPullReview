---
schema: "implement-rfc.card.v1"
id: "BPR-2359"
title: "Scroll the code from an empty Comments drawer"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner, 2026-11-23","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-1290","BPR-1542","BPR-2279"],"reservation":"docs/issues/_reservations/BPR-2359--BPR-2359--ccf4ea1de58e.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b543eda0fbdffc1e7df0d2cdeb02577cdf01b32f","evidence":{"kind":"command","detail":"qt_composed_inline_drawer_scale_{100,125,200} hit-test the empty drawer body and deliver spontaneous Qt platform wheel phases. Vertical updates scroll the code up and down, momentum continues each movement, and an unphased wheel notch moves the code. Baseline product code fails the downward update at 100%."}},{"criterion":"AC2","outcome":"pass","revision":"b543eda0fbdffc1e7df0d2cdeb02577cdf01b32f","evidence":{"kind":"command","detail":"The inline drawer courses move the code by gesture over a visible note and by wheel over open post text. qt_composed_long_thread_scale_{100,125,200} reach the last post and qt_nested_scroll_latch preserves nested ownership through momentum. The focused Release selector including general composed and architecture checks passed 11/11."}}]
---

# BPR-2359: Scroll the code from an empty Comments drawer

> THere's a bug in the comment drawer where I can't always scroll it. Seems to appear most often when I'm trying to scroll on an empty drawer screen.

The owner clarified: "Specifically i mean with a scroll gesture over the comment drawer".

## Acceptance

- AC1. With scrollable code beside an empty Comments drawer in Review, a
  vertical trackpad gesture over the drawer body moves the code in both
  directions, including momentum. A wheel over the same area also moves it.
- AC2. A gesture over the populated drawer still moves the code. Open
  threads can still scroll to their ends, and nested scrolling keeps its
  existing gesture ownership.

## Plan

Exercise the Review window offscreen with a scrollable comparison and
Comments visible. Drive platform-style phased input at the drawer body in
empty and populated states, reproduce the failure, and fix the missing
route while keeping content-specific scroll handling. Run the composed
drawer, long-thread and nested-scroll checks and the documentation gate.

## Resolution

The drawer shows a plain empty page when the pull request has no retained
conversations and there are no drafts. Only the cards view forwarded wheel
input to the comparison, so a gesture over the empty page reached neither
scroller. The Review Comments tab now forwards unhandled wheel events to
the visible comparison through the existing wheel path and accepts the
whole gesture. Cards continue to handle their own content and overflow.

With the product code at `ff1dd605e1d5`, the composed 100% check failed at
the first downward update over the empty page. The candidate passed 11
focused Release checks on macOS arm64 with Qt 6.12.0. The inline drawer,
long-thread and general composed courses each ran at 100%, 125% and 200%,
alongside nested scroll latching and the architecture boundary check.
The hidden Cocoa run passed the new scroll checks and rendered the empty
drawer. It later failed at the reply editor's keyboard focus assertion,
which the unmodified 0.5.2 course binary also fails in hidden Cocoa.
