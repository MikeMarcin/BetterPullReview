---
schema: "implement-rfc.card.v1"
id: "BPR-1609"
title: "Shared charts, the GitHub tab and the Summary budget bar"
status: "Done"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1608"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1609:shared-chart","outcome":"pass","revision":"173e69f7afaaaa21b233ee56347bd3621d63b2f4","evidence":{"kind":"observation","detail":"Exact frozen public course and inspected renders cover independent framed lanes/maxima, interval event bars, stacks/gaps/reset rules, midpoint/edge hover and accessible descriptions. Isolated-to-continuous brush regression fails without the reset and passes after restoration. Calendar remains later scope."}},{"criterion":"BPR-1609:github-summary","outcome":"pass","revision":"173e69f7afaaaa21b233ee56347bd3621d63b2f4","evidence":{"kind":"observation","detail":"Unchanged accounting/privacy/retention evidence is reused. Frozen real SQLite/public fixture proves split, budgets, categories, processes, limits and no-effect refresh/tabs/ranges."}},{"criterion":"BPR-1609:theme-shell","outcome":"pass","revision":"173e69f7afaaaa21b233ee56347bd3621d63b2f4","evidence":{"kind":"observation","detail":"Frontend-design and ATT reference were read. All 22 developer variants and final frozen graphs were inspected. Expanded native caption flags, fixed/clamped shell and profile roles pass; actual native appearance is accepted separately by the owner-titlebar record."}},{"criterion":"BPR-1609:engineering","outcome":"pass","revision":"173e69f7afaaaa21b233ee56347bd3621d63b2f4","evidence":{"kind":"observation","detail":"Final focused integration 4/4; format22/tidy, 67 documentation regressions and final documentation gate. Completed clean main RE2 work merged, combined composition pins reviewed, no publication authority change. Exact final frozen launcher/public course pass."}},{"criterion":"BPR-1609:owner-titlebar","outcome":"pass","revision":"173e69f7afaaaa21b233ee56347bd3621d63b2f4","evidence":{"kind":"observation","detail":"Owner explicit Accept, empty note, session 1b7f1eff8b9c5b203d35ae30890884f8, event 8, decision a8156396-0d49-4da5-8515-57b84932f1d8, candidate BPR-1609-insights-titlebar-v002, revision 173e69f7a-v002, fingerprint 9ac52a892f6f30e32310b5482a24d9ec316de1a2cb92bfcd205871f9d0f027a5. Handback event9; consumed cursor11 after scoped releases. Exact frozen source and build retained; both verdicts preserved."}},{"criterion":"BPR-1609:owner-chart-comparison","outcome":"pass","revision":"173e69f7afaaaa21b233ee56347bd3621d63b2f4","evidence":{"kind":"observation","detail":"Owner explicit Accept, empty note, session 1b7f1eff8b9c5b203d35ae30890884f8, event 7, decision 3b03a520-366a-4543-85d2-d04c8842bee3, candidate BPR-1609-insights-charts-v002, revision 173e69f7a-v002, fingerprint 9e8f16d1ead05319b19107c71c124b608493628ba838608a1fac09fa06a1a069. Handback event9; consumed cursor11 after scoped releases. Exact frozen source and build retained; both verdicts preserved."}}]
---

# Shared charts, the GitHub tab and the Summary budget bar

## Scope and acceptance

Implement RFC-BPR-0086 "Charts" (AC02) as a shared Qt chart component,
then the GitHub tab (AC04) and the Summary budget bar and Problems tile (AC03).

- Lanes, maximum labels, the foreground/background stack, gaps, hover with
  hysteresis, edge flipping, theme roles and accessible descriptions. The
  calendar mode is added by whichever of BPR-1611 and BPR-1613 lands
  first.
- Load `frontend-design:frontend-design` first. The owner compares the charts
  with AllTheThings' Energy chart in an attended review.
