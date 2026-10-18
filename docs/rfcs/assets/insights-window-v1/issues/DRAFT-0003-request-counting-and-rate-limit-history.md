---
schema: "implement-rfc.card.v1"
id: "DRAFT-0003"
title: "Request counting and rate-limit history"
status: "Backlog"
rfcs: ["RFC-DRAFT-0024"]
depends_on: ["DRAFT-0002"]
platforms: ["mac", "win", "linux"]
---

# Request counting and rate-limit history

## Scope and acceptance

Implement the measurement half of RFC-DRAFT-0024 "Summary" and "GitHub"
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
