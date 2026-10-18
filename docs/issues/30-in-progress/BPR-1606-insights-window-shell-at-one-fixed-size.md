---
schema: "implement-rfc.card.v1"
id: "BPR-1606"
title: "Insights window shell at one fixed size"
status: "In Progress"
rfcs: ["RFC-BPR-0086"]
depends_on: []
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1606:shell","outcome":"pass","revision":"5a02177b30250494811033052b673c20a9b5a7b0","evidence":{"kind":"observation","detail":"Real profile menu and central shortcut open one fixed-size Insights window. Public-path test exercises all six tabs, every range and three refreshes, populated local Summary/Storage, header anonymized export to existing frozen preview, Manage cached repositories to existing controls, Escape and shutdown."}},{"criterion":"BPR-1606:refresh","outcome":"pass","revision":"5a02177b30250494811033052b673c20a9b5a7b0","evidence":{"kind":"observation","detail":"Application host tests prove effect/completion lanes, in-flight skip and minute storage throttle. Hidden window timers stop; storage port and shared Git inventory have no provider, Git operation or process capability. Visible cadence is five seconds active and thirty seconds inactive."}},{"criterion":"BPR-1606:owner","outcome":"fail","revision":"5a02177b30250494811033052b673c20a9b5a7b0","evidence":{"kind":"observation","detail":"Owner rejected the frozen shell at session a25204cd201da1ca5dbff4439c52bda4 event 6: “that looks almost nothing like the mockups /Users/jaeger/Downloads/bpr_insights_window_fixed_size_mockup.html”. Event 7 handed back. Revision remains rejected; correction is required."}}]
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

## Owner correction, 2026-10-18

The first shell was rejected against the exact HTML mockup at
`/Users/jaeger/Downloads/bpr_insights_window_fixed_size_mockup.html`.
Fresh engineering round released only this candidate through
`resume-development` (event 8); its rejection and frozen build are retained.
The correction follows that reference’s compact toolbar, title/status header,
card grid and storage rows while preserving the authored milestone boundary.
