---
schema: "implement-rfc.card.v1"
id: "BPR-1887"
title: "Keep conversation navigation clear of line-aligned comments"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-1887--BPR-1887--9c514ed2528a.json","related_cards":["BPR-1781","BPR-1886"],"milestone_plan":{"id":"responsiveness-latency-v4","path":"docs/rfcs/plans/responsiveness-latency-v4/milestone-plan.json"}}
---

# BPR-1887: Keep conversation navigation clear of line-aligned comments

The owner identified the pull-request conversation link overlapping a comment
in the responsiveness course's rendered application. The same overlap is in
the saved BPR-1885 render, before BPR-1886. The navigation link is a child of
the viewport with fixed top coordinates, where a line-aligned card can also
be painted. The existing tests checked the link's text/action but not its
separation from comments. A passing functional course did not make that
render acceptable.

## Acceptance

- AC1: Give the link layout space outside the composed comment viewport;
  preserve its label, unread count, keyboard access and Overview destination.
- AC2: Preserve code-line alignment, card order, refresh loading feedback and
  the current comparison; verify navigation and narrow composed geometry.
- AC3: Add a regression that fails against the overlapping version, inspect
  corrected application renders and run focused source/documentation checks.
- AC4: Record inherited gate failures, add one journal line and land on main.

## Evidence

Pending implementation and verification. The frontend-design skill is loaded;
this is a layout correction within the existing native presentation.
