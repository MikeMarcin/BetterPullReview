---
schema: "implement-rfc.card.v1"
id: "BPR-1614"
title: "Work timings and the Work tab"
status: "Done"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1609"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC10-event-loop","outcome":"pass","revision":"9334f0b12ce6d73bcb1a9c9a9214304755a2d040","evidence":{"kind":"observation","detail":"Public real QEventLoop250ms block becomes exactly one persisted stall251177us, with p95 against100ms; independent production-monitor probe253081us excludes800ms idle. Removing event-filter installation yields0stalls and exit1.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/integration.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/metrics-probe-positive.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/metrics-probe-negative.log"]}},{"criterion":"AC10-diff","outcome":"pass","revision":"9334f0b12ce6d73bcb1a9c9a9214304755a2d040","evidence":{"kind":"observation","detail":"Actual application-host computations independently reach move work, move segment and approximate-alignment limits. Each emits its own flag, one duration/local path; cached comparison emits nothing.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/test-correction.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/integration.log"]}},{"criterion":"AC10-host-facts","outcome":"pass","revision":"9334f0b12ce6d73bcb1a9c9a9214304755a2d040","evidence":{"kind":"observation","detail":"Core weighted p95/even+odd median, hour/day selection and five slowest ordering pass. Real syntax hit/miss/eviction facts match host counters; every executed search includes acquisition+matching even on failure, without query data. Public seeded7files,5paths,75%cache hits,2evictions and20ms median match.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/integration.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/search-final-check.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/reserve-check.log"]}},{"criterion":"Work-lifetime-privacy-presentation","outcome":"pass","revision":"9334f0b12ce6d73bcb1a9c9a9214304755a2d040","evidence":{"kind":"observation","detail":"Schema5 migration/retention/restart preserve48h five-minute and93d daily history; Work path canary absent from byte-only export. Public no-effect refresh/fixed/clamped layout/keyboard/hidden timer checks pass. Light/Dark1x,1.25x,2x Work inspected. Existing maintenance/reset classification reused unchanged.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/integration.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/public-1.25.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/public-2.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-work-r1-de2hcgoq/docs-integrated.log"]}}]
---

# Work timings and the Work tab

## Scope and acceptance

Implement RFC-BPR-0086 "Work" (D10) and AC10.

- Main-thread stall detection in the Qt adapter, with the 100 ms threshold,
  count, longest and p95.
- Per-file diff wall time in the application host, and work-limit counts.
- Five slowest files, syntax cache hit rate and evictions, and median source
  search time.
- Load `frontend-design:frontend-design` first.
