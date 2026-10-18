---
schema: "implement-rfc.card.v1"
id: "BPR-1606"
title: "Insights window shell at one fixed size"
status: "Done"
rfcs: ["RFC-BPR-0086"]
depends_on: []
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1606:shell","outcome":"pass","revision":"ae0a402118e6560a9e1cb98ca9e6ef98c3b180ce","evidence":{"kind":"observation","detail":"Corrected shell follows the exact supplied HTML geometry and composition. Real menu/shortcut, six tabs/ranges, three refreshes, fixed frame/content/header, clamped page fit, byte data, export preview, cache controls, Escape, hidden timers and shutdown pass. All pages rendered Light/Dark at 1x, 1.25x and 2x."}},{"criterion":"BPR-1606:refresh","outcome":"pass","revision":"ae0a402118e6560a9e1cb98ca9e6ef98c3b180ce","evidence":{"kind":"observation","detail":"Unchanged passing host/lifecycle/throttle and capability evidence reused. Qt only changes presentation. Architecture scan and composed contract pass; no provider, Git, process, persistence or export policy changed."}},{"criterion":"BPR-1606:owner","outcome":"pass","revision":"ae0a402118e6560a9e1cb98ca9e6ef98c3b180ce","evidence":{"kind":"observation","detail":"Owner explicitly accepted BPR-1606-insights-shell-v002, revision ae0a40211-v002, fingerprint 649ab6f1674c77749d04c5695c2a1b2f3ce864869977ba766afd4619cf561d69 in session e51e8b870c12f3d41b3f243b2891758a, event 8 (decision b1070c71-f804-48f1-8210-80d373c6fd8a), empty note. Event 9 hands back; consumed cursor 9. Acceptance applies only to the corrected shell; earlier rejection remains recorded."}}]
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
