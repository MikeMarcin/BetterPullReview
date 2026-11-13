---
schema: "implement-rfc.card.v1"
id: "BPR-2154"
title: "The composer queues and steers, and shows every pending, sent and uncertain input"
status: "Backlog"
rfcs: ["RFC-BPR-0126"]
depends_on: ["BPR-2151", "BPR-2153"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"},"criteria":["AC01","AC08"],"origin":"imported 2026-11-12 from queue-and-steer-v1 (RFC-DRAFT-0001); the draft defined no cards, they were cut at import"}
---

# BPR-2154: The composer queues and steers, and shows every pending, sent and uncertain input

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md); milestone `queue-and-steer` of the [plan](../../rfcs/plans/queue-and-steer-v1/MILESTONES.md).

## Scope

Load `frontend-design:frontend-design` first. Send labelled Queue while the conversation runs or its queue is not empty; a separate Steer action and keyboard command on a steerable turn; Stop always reachable; the compact pending list with Edit, Remove and reorder (also by keyboard), Pause queue and Resume queue; states in words with one polite announcement per action; projections keep the draft, focus, scroll and selection; existing palette, accessibility and agent-accent rules. Rendered offscreen and driven through the composed application.

## Acceptance

- **AC01:** Every in-scope chat accepts and displays multiple busy follow-ups with the defined Queue/Steer distinction, keyboard behavior and unsent editing controls; projections preserve the composer and captured subject.
- **AC08:** The composed Qt flow exposes pending, paused, sent and uncertain states and recovery with reachable Stop and keyboard/accessibility actions, while retaining the existing proposal-acceptance and publication boundaries.

## Notes

Owner questions: Q4, Q5, Q9. An owner look on a frozen build is not an acceptance criterion of the RFC as drafted; a look can be filed and not waited on.
