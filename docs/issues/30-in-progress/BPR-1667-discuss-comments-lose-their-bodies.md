---
schema: "implement-rfc.card.v1"
id: "BPR-1667"
title: "Show the accepted comment bodies in Discuss"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner v0.3.0 report, 2026-10-19; v0.3.1 release blocker","reservation":"docs/issues/_reservations/BPR-1667--BPR-1667--599178c63b5c.json","milestone_plan":{"id":"bpr-1667-discuss-body","path":"docs/rfcs/plans/bpr-1667-discuss-body/milestone-plan.json"}}
---

# Show the accepted comment bodies in Discuss

Mandate `discuss-body-blocker-r1`, assignment `/root/discuss_body_fix`.
Baseline `818de796b1f06b02cc5dda24e0ed86363c413154`.
The owner opens Overview -> new comments -> Discuss and sees many comments
saying “No body provided.” Opening the same comment in Review shows its body.
Preserve the existing design, exact content identity and actual empty-body state.
Use sanitized deterministic fixtures and offscreen product paths; no live GitHub,
private data, owner desktop, release version or publication actions.

## Acceptance

- AC1: Reproduce the mismatch through the production Overview new-comments route
  and show a deterministic regression fails against the old code.
- AC2: Discuss and Review show the same accepted body for the same comment;
  changed content and reused identities update without stale or cross-body text.
- AC3: Actual empty bodies retain their empty state; independent bodies and
  original Markdown source remain exact.
- AC4: Focused public-path and adjacent checks, touched-file coding checks and
  documentation gates pass; commit the result and hand off quiescent commands.
