---
schema: "implement-rfc.card.v1"
id: "BPR-2362"
title: "Scroll the code with a draft in Comments"
status: "In Progress"
rfcs: []
depends_on: ["BPR-2359"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","origin":"owner attended review, 2026-11-23","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-2279","BPR-2292","BPR-2354"],"reservation":"docs/issues/_reservations/BPR-2362--BPR-2362--f3ee1fd82a29.json"}
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
  verify the fix with Qt 6.12.1. Obtain a fresh owner verdict on the frozen
  correction.

## Plan

Extend the composed inline drawer course and retain the long-editor and
sideways-code coverage. Make the latch consider the drawer's forwarding
path for vertical gestures. Run the focused Qt, architecture, formatting
and documentation checks before handing off for attended review.
