---
schema: "implement-rfc.card.v1"
id: "BPR-2359"
title: "Scroll the code from an empty Comments drawer"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner, 2026-11-23","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-1290","BPR-1542","BPR-2279"],"reservation":"docs/issues/_reservations/BPR-2359--BPR-2359--ccf4ea1de58e.json"}
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
