---
schema: "implement-rfc.card.v1"
id: "BPR-1611"
title: "The Reviews tab"
status: "Done"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1609", "BPR-1610"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC07","outcome":"pass","revision":"f57e77c416c27b8465e11b312ad4d93025b44368","evidence":{"kind":"command","detail":"Seeded real store shows9 lifetime and6 thirty-day submissions, exact three ranges,26-week Monday calendar, five ending facts and five records. Host and SQLite cover DST overlap, spring gap and local Monday. Since-date and no-streak checks pass.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/integration-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/public-final-100.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/public-final-125.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/public-final-200.log"]}},{"criterion":"summary","outcome":"pass","revision":"f57e77c416c27b8465e11b312ad4d93025b44368","evidence":{"kind":"command","detail":"Both composed Summary activity tiles show the expected30-day counts:6 reviews and3 comments.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/integration-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/public-final-100.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/public-final-125.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/public-final-200.log"]}},{"criterion":"public-presentation","outcome":"pass","revision":"f57e77c416c27b8465e11b312ad4d93025b44368","evidence":{"kind":"command","detail":"All tabs/ranges, fixed and clamped sizes, passive refresh, plotted-data absence controls, hover/accessibility, Light/Dark, existing export/cache actions and shutdown pass. Public course passes at1x,1.25x,2x; renders inspected.","refs":["/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/integration-tests.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/public-final-100.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/public-final-125.log","/Users/jaeger/.mask/implement-rfc-runtime/bpr-insights-review-stats-r1-36e1d558/public-final-200.log"]}}]
---

# The Reviews tab

## Scope and acceptance

Implement the RFC-BPR-0086 Reviews presentation and AC07.

- Three ranges, the tiles, the 26-week calendar, "How reviews ended" and
  records. Local-time-zone days, including across a
  daylight-saving change.
- No streak (Q4).
- The two Summary review tiles.
- Load `frontend-design:frontend-design` first.
