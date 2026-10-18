---
schema: "implement-rfc.card.v1"
id: "BPR-1606"
title: "Insights window shell at one fixed size"
status: "Ready"
rfcs: ["RFC-BPR-0086"]
depends_on: []
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
---

# Insights window shell at one fixed size

## Scope and acceptance

Implement RFC-BPR-0086 "The window" and AC01.

- A per-profile Insights window, owned like Settings, with the six tabs and the
  two header actions. The export action opens the existing export flow.
- One locked content size, clamped to the screen. A test switches every tab
  and range through three refreshes and asserts the frame never changes.
- Refresh runs every 5 s while active, every 30 s otherwise, and stops when
  hidden. A refresh issues no GitHub, Git or process effect.
- Summary tiles that have data today (Local data, Background work), with empty
  states for the rest.
- Menu entry: BetterPullReview › Insights… on macOS, Help › Insights…
  elsewhere, with ⌘⌥⇧I or Ctrl+Alt+Shift+I (Q1).
- Load `frontend-design:frontend-design` first. The owner judges the window
  in an attended review.
