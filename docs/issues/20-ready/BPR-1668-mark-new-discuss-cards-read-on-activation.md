---
schema: "implement-rfc.card.v1"
id: "BPR-1668"
title: "Mark a new Discuss card read when it opens"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner amendment, 2026-10-19: auto mark new as read when clicking a card with the new badge","reservation":"docs/issues/_reservations/BPR-1668--BPR-1668--30d95b0fbb41.json","milestone_plan":{"id":"bpr-1667-discuss-body","path":"docs/rfcs/plans/bpr-1667-discuss-body/milestone-plan.json"}}
---

# Mark a new Discuss card read when it opens

Owner amendment to `/root/discuss_body_fix`, mandate
`discuss-body-blocker-r2-auto-read`: “While we're at it can we auto mark new as
read when we click on a card with the new badge”. Preserve BPR-1667's body and
empty-summary obligations. Use the existing semantic read command and durable
ledger; Qt performs no persistence or provider effects.

## Acceptance

- AC1: Clicking a New-badged Discuss card requests reading that exact card;
  keyboard activation follows the same opening path. Passive projection refresh
  and selection alone do not mark unrelated cards read.
- AC2: The ordinary application publishes the cleared badge only after the
  existing durable read path succeeds. Restart preserves the result; another
  unread summary, PR comment and inline thread stay new until activated.
- AC3: A deterministic old-code failure and passing focused Qt/public-composition
  regressions cover the behavior; coding/documentation checks pass and the
  combined milestone is committed and quiescent.
