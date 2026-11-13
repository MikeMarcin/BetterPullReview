---
schema: "implement-rfc.card.v1"
id: "BPR-2152"
title: "Queued and steered input survives crashes and races without loss or automatic resend"
status: "Backlog"
rfcs: ["RFC-BPR-0126"]
depends_on: ["BPR-2151"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"},"criteria":["AC06"],"origin":"imported 2026-11-12 from queue-and-steer-v1 (RFC-DRAFT-0001); the draft defined no cards, they were cut at import"}
---

# BPR-2152: Queued and steered input survives crashes and races without loss or automatic resend

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md); milestone `queue-and-steer` of the [plan](../../rfcs/plans/queue-and-steer-v1/MILESTONES.md).

## Scope

Atomic claim of the exact entry and version, durably marked Sending and bound to its BPR run and native session or thread (and expected turn for Steer) before any write; failure before a write leaves it unsent, a possible write without an outcome is Delivery unknown; receipts and terminal disposition persisted before the queue advances; reconciliation only from qualified native status or history; bounded transport shutdown so a terminal run never holds the workspace lease. Fault-injection courses for crash, write and save races, duplicate and intermediate results, and dropped corrections.

## Acceptance

- **AC06:** Queue and steering handoff survive crash/write/save races without losing or automatically duplicating input. Intermediate/duplicate results and dropped corrections cannot report premature success, retarget output, or hold a terminal run's lease indefinitely. Unknown input stays identifiable and never auto-resends.

## Notes

Owner questions: Q16.
