---
schema: "implement-rfc.card.v1"
id: "BPR-2330"
title: "Another author's draft waiting in Following sends no notification"
status: "Ready"
rfcs: []
depends_on: ["BPR-2326"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","found_by":"owner, 2026-11-20, relayed by the coordinator, answering whether a draft in Following should still notify: \"Yes, stay quiet.\"","reservation":"docs/issues/_reservations/BPR-2330--BPR-2330--a64322720637.json","related_cards":["BPR-2326","BPR-1683"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2330: Another author's draft waiting in Following sends no notification

## Asked

[[BPR-2326]] moved another author's draft from Needs me to Following but
left its notification eligibility alone, so a review request on a draft
still interrupted. The owner, 2026-11-20: "Yes, stay quiet." A follow-up
card because BPR-2326 is Done.

## Acceptance

- AC1 While another author's pull request is read as a draft and Settings ›
  Review › Home › Others' drafts is Following (the default), it sends no
  notification: not for a review request, comments, commits since my review
  or watched activity.
- AC2 When a read says it is ready for review, the usual rules apply: a
  pending review request (or other notifying state) notifies then, once, and
  not again on later refreshes while it stays in the same state.
- AC3 With Others' drafts on Needs me, notifications are as before. My own
  drafts and ignored rows are unchanged.
- AC4 Tests: core/application policy (a draft's request does not notify;
  ready notifies once; Needs me notifies), architecture boundaries, format
  and the documentation gate.
