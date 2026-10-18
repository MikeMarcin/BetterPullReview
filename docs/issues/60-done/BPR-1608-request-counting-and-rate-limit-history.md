---
schema: "implement-rfc.card.v1"
id: "BPR-1608"
title: "Request counting and rate-limit history"
status: "Done"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1607"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1608:requests","outcome":"pass","revision":"7fdcc9cd1b2e3c08e84c19d244116bd058ebb756","evidence":{"kind":"observation","detail":"Native API hop creation and actual gh API process start emit content-free resource/category facts. Headerless requests count, Core keeps the lowest remaining per host/account/reset, and GraphQL has no split. core_insights, native_https_transport_fixture, gh_cli_transport pass; typed category mapping reviewed."}},{"criterion":"BPR-1608:persistence-retention","outcome":"pass","revision":"7fdcc9cd1b2e3c08e84c19d244116bd058ebb756","evidence":{"kind":"observation","detail":"SQLite schema 2 migrates the store and preserves own request counts through restart inside the same reset. Headerless, reset crossing and no-current-reading states pass core_insights and sqlite_insights_storage. Pure retention boundaries and effect-lane compaction retain raw 2h, five-minute 48h and daily 93d."}},{"criterion":"BPR-1608:processes","outcome":"pass","revision":"7fdcc9cd1b2e3c08e84c19d244116bd058ebb756","evidence":{"kind":"observation","detail":"Real github_cli_process_runner course passes exact launches, timeout, cancellation, output limit, crash, nonzero exit, reap failure, one quarantine, one abandoned child and slowest duration. Shared supervision ordering guard and its focused negative control pass."}},{"criterion":"BPR-1608:problems","outcome":"pass","revision":"7fdcc9cd1b2e3c08e84c19d244116bd058ebb756","evidence":{"kind":"observation","detail":"The diagnostic decorator tallies failed outcomes before the logger preferences, including no logger, on the Insights effect lane. application_insights_host passes; closed operation/category mapping reviewed."}},{"criterion":"BPR-1608:passive","outcome":"pass","revision":"7fdcc9cd1b2e3c08e84c19d244116bd058ebb756","evidence":{"kind":"observation","detail":"Insights refresh owns only its local storage port; no provider, Git or process capability is reachable there. application_insights_public_path passes through the actual menu/window/lifecycle. architecture_boundaries passes in 15.54s. No live call was issued for this work."}}]
---

# Request counting and rate-limit history

## Scope and acceptance

Implement the measurement half of RFC-BPR-0086 "Summary" and "GitHub"
(D5, D7) for AC03 and AC04.

- Count BetterPullReview's requests by resource and operation category,
  persisted per reset window so a restart keeps them.
- Keep every rate-limit reading, deduplicated, at the History retention.
- Derive other clients for Core REST in the core, with clamping and the
  stated states for a reset crossing and for no reading.
- Count `gh` process outcomes, quarantined lanes, abandoned children and the
  slowest call from the shared process supervision.
- Tally Problems: `failed` diagnostic outcomes by operation category.
- Never issue a request for Insights.
