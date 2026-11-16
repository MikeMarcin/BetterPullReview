---
schema: "implement-rfc.card.v1"
id: "BPR-2154"
title: "The composer queues and steers, and shows every pending, sent and uncertain input"
status: "In Progress"
rfcs: ["RFC-BPR-0126"]
depends_on: ["BPR-2151", "BPR-2153"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"},"criteria":["AC01","AC08","AC09"],"origin":"imported 2026-11-12 from queue-and-steer-v1 (RFC-DRAFT-0001); the draft defined no cards, they were cut at import"}
---

# BPR-2154: The composer queues and steers, and shows every pending, sent and uncertain input

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md); milestone `queue-and-steer` of the [plan](../../rfcs/plans/queue-and-steer-v1/MILESTONES.md).

## Scope

Load `frontend-design:frontend-design` first. Follow-up behaviour as a Settings choice, Queue or Steer, default Queue, and Cmd+Return (Ctrl+Return off macOS) for the opposite once; Send labelled by what it will do while the conversation runs or its queue is not empty; Stop always reachable; the compact pending list with in-place Edit (held while edited), Remove and reorder (also by keyboard), Pause queue and Resume queue; Stop all in the status bar's agent footer; states in words with one polite announcement per action; projections keep the draft, focus, scroll and selection; existing palette, accessibility and agent-accent rules. Rendered offscreen and driven through the composed application.

## Acceptance

- **AC01:** Every in-scope chat accepts and displays multiple busy follow-ups with the defined Queue/Steer distinction, keyboard behavior and unsent editing controls; projections preserve the composer and captured subject.
- **AC08:** The composed Qt flow exposes pending, paused, sent and uncertain states and recovery with reachable Stop and keyboard/accessibility actions, while retaining the existing proposal-acceptance and publication boundaries.
- **AC09:** One Stop all control in the status bar's agent footer, beside the agent picker, in a fixed slot shown while any agent turn runs or any message is queued, stops every active turn across sessions and pauses every queue, so nothing starts next; it is reachable by keyboard and named to assistive technology.

## Notes

Owner answers 2026-11-13 (on the RFC): Q4/Q5 follow-up behaviour is a Queue or Steer setting, default Queue, with Cmd+Return doing the opposite for one message (Codex's Follow-up behavior setting as inspiration); Q9 queued messages are edited in place and held while edited; Stop all in the agent footer (AC09). An owner look on a frozen build is not an acceptance criterion of the RFC as drafted; a look can be filed and not waited on.
