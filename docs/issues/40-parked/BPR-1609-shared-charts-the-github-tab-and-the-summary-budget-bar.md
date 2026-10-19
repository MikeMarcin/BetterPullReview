---
schema: "implement-rfc.card.v1"
id: "BPR-1609"
title: "Shared charts, the GitHub tab and the Summary budget bar"
status: "Parked"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1608"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1609:shared-chart","outcome":"pass","revision":"43ec9904a1a688bf9fd4472e113ec29b17b07b04","evidence":{"kind":"observation","detail":"Rendered public contract covers independent lanes and maxima, stack fill plus a missing-series negative control, gaps/reset marks, midpoint hover, edge-inset card flipping, range clearing and accessible descriptions. Calendar is assigned to the later Reviews/Energy milestone by this card."}},{"criterion":"BPR-1609:github-summary","outcome":"pass","revision":"43ec9904a1a688bf9fd4472e113ec29b17b07b04","evidence":{"kind":"observation","detail":"The real seeded SQLite public fixture proves Summary own/other/remaining, GraphQL without a split, Search, Problems, categories, process failures and secondary-limit facts. Normal fixture connection adds requests; repeated refresh/tabs/ranges add none."}},{"criterion":"BPR-1609:theme-shell","outcome":"pass","revision":"43ec9904a1a688bf9fd4472e113ec29b17b07b04","evidence":{"kind":"observation","detail":"Frontend-design was read before UI work. Supplied exact HTML and AllTheThings Energy source reviewed; inspected Light/Dark offscreen Summary and GitHub 1h/1d renders preserve the accepted fixed shell. Native profile roles drive chart colors."}},{"criterion":"BPR-1609:engineering","outcome":"pass","revision":"43ec9904a1a688bf9fd4472e113ec29b17b07b04","evidence":{"kind":"observation","detail":"LLVM22 format/tidy, core-only Insights, production syntax and focused eight-test integration pass. Completed-main integration rebuilt the two public targets and its three affected courses pass in 27.22s. Source self-review retains passive observation, private bounded storage and existing publication authority."}},{"criterion":"BPR-1609:owner-chart-comparison","outcome":"pending","revision":"43ec9904a1a688bf9fd4472e113ec29b17b07b04","evidence":{"kind":"artifact","detail":"Ready protected attended session 12f141ab8f2268fd7a22c6973bfd0921, candidate BPR-1609-insights-github-v001 revision 43ec9904a-v001 fingerprint 3d608c3246f118f3937254653d3603021f4d03e80cead3f5f024ee4d1623c656. Consumed preparation cursor4; owner Accept/Reject against AllTheThings Energy chart remains pending for the next engineering round."}}]
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
