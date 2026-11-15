---
schema: "implement-rfc.card.v1"
id: "BPR-2153"
title: "Claude and Codex App Server accept a native correction on the running turn, counted once"
status: "In Progress"
rfcs: ["RFC-BPR-0126"]
depends_on: ["BPR-2152"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"},"criteria":["AC05","AC07"],"origin":"imported 2026-11-12 from queue-and-steer-v1 (RFC-DRAFT-0001); the draft defined no cards, they were cut at import"}
---

# BPR-2153: Claude and Codex App Server accept a native correction on the running turn, counted once

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md); milestone `queue-and-steer` of the [plan](../../rfcs/plans/queue-and-steer-v1/MILESTONES.md).

## Scope

Codex App Server `turn/steer` with `threadId` and `expectedTurnId`; Claude bidirectional stream JSON with a UUID-bearing user input, priority `next`, correlated by input acknowledgment and `user_message_uuid(s)` separately from the result `uuid` fence. Completion waits for every accepted input's terminal disposition and identified continuations or helpers. Capability from local protocol and version observations, never a paid probe; an unsupported driver keeps Queue and names why Steer is unavailable; no kill, restart or replay fallback. Accounting: every native part of a steered run counts once against the run's turn and session totals and limits; unknown stays unknown. Installed-agent qualification with exact versions, smallest bounded conversations, on the owner's installed Claude and Codex.

## Acceptance

- **AC05:** Qualified installed Claude and Codex App Server each accept a native correction on their existing conversation without transcript replay or a deliberate process restart. Unsupported configurations keep Queue and report Steer as unavailable without a paid discovery probe.
- **AC07:** All native parts of a steered run contribute once to the existing turn/session accounting and limits. Unreported costs remain unknown, and pending-input management adds no model request, summarizer or keep-alive call.

## Notes

Owner questions: Q3, Q14, Q15, Q17, Q18.

## Progress

- 2026-11-14: started by coordinator assignment on branch
  `claude/native-steering`, ahead of BPR-2152 (durable handoff, Backlog).
  This card builds the native mechanism, the typed capability and the
  accounting; the durable Sending/Delivery-unknown records stay BPR-2152's.
