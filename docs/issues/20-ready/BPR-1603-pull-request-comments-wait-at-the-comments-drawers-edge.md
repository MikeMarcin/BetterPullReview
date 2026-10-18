---
schema: "implement-rfc.card.v1"
id: "BPR-1603"
title: "Pull-request comments wait at the Comments drawer's edge"
status: "Ready"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1594"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner idea relayed by the coordinator during BPR-1593, 2026-10-18: \"The notes-on-the-pull-request thing might actually be a good UX for comments on the pull request too.\"", "reservation": "docs/issues/_reservations/BPR-1603--BPR-1603--a7b51ca1327a.json", "milestone": "conversation-chips", "criteria": ["AC10"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
---
# BPR-1603: Pull-request comments wait at the Comments drawer's edge

RFC-BPR-0085 D8 (amendment, 2026-10-18). Pull-request-level conversation
comments and review summaries, which are on no file, wait as a chip at the
Comments drawer's top edge, as [[BPR-1592]]'s notes on the pull request do in
Notes. The chip lists them; each opens in place, carries its unread mark from
[[BPR-1601]]'s read state, and offers Show in Discuss.

## Acceptance

- RFC-BPR-0085 AC10.
- A short design note with a mockup in Light and Dark comes first.
- Owner, attended: "Comments on the pull request wait at the drawer's edge and
  open in place."

## Boundaries

Qt presentation over the accepted conversation projection and BPR-1601's read
state; no new GitHub read, and Discuss keeps every comment as it does today.
Uses BPR-1592's edge-chip and list pattern, and the shared drawer parts.
