---
schema: "implement-rfc.card.v1"
id: "BPR-1666"
title: "Keep Review scrolling stable after opening a new thread"
status: "Ready"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner v0.3.0 report, 2026-10-19; v0.3.1 release blocker","reservation":"docs/issues/_reservations/BPR-1666--BPR-1666--2a12e1d9417b.json","milestone_plan":{"id":"bpr-1666-thread-scroll","path":"docs/rfcs/plans/bpr-1666-thread-scroll/milestone-plan.json"}}
---

# Keep Review scrolling stable after opening a new thread

The owner opens Overview's threads-with-new-posts status, enters New Threads,
and double-clicks a thread into Review. Scrolling long comments repeatedly snaps
upward. Clicking sometimes permits scrolling; at rest the left pane shifts or
flickers, and the horizontal scrollbar disappears and reappears as diff text moves.
Repeated reveal and scrollbar/layout feedback are hypotheses pending reproduction.

Repair the existing reading behavior without changing its visual design. Use
sanitized offscreen fixtures; never drive the owner's desktop or personal apps.
Baseline: `b433d515f`. Mandate: `thread-scroll-blocker-r1`.

## Acceptance

- AC1: Reproduce the failing interaction deterministically through the production
  Qt window, identify the cause, and show the regression detects the old behavior.
- AC2: Opening a thread still reveals its selected post; subsequent user scrolling
  reaches long content without delayed snap-back and rests at a stable position.
- AC3: Idle native scrollbar visibility and diff viewport geometry settle; retain
  ordinary native scrolling, selection, font/resize reflow and exact thread placement.
- AC4: Focused composition and adjacent Qt tests, touched-file coding checks and
  documentation gates pass; record platform limits and hand off a committed candidate.
