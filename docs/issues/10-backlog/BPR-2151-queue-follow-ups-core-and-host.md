---
schema: "implement-rfc.card.v1"
id: "BPR-2151"
title: "BPR queues follow-ups and releases them only after real completion"
status: "Backlog"
rfcs: ["RFC-BPR-0126"]
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"},"criteria":["AC02","AC03","AC04"],"origin":"imported 2026-11-12 from queue-and-steer-v1 (RFC-DRAFT-0001); the draft defined no cards, they were cut at import"}
---

# BPR-2151: BPR queues follow-ups and releases them only after real completion

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md); milestone `queue-and-steer` of the [plan](../../rfcs/plans/queue-and-steer-v1/MILESTONES.md).

## Scope

Core: the bounded pending-input state of one conversation (stable entry IDs, displayed order, generation and queue revision, captured subject and attachments, Queued/Sending/Sent/Paused/Delivery unknown, limits), its legal transitions, and refusal on a fresh path (`session_mode_of`). Host: the profile-owned durable queue, the single dispatch owner and stale-revision refusal across windows, release of one next entry only after durable result handling, tour proposal processing, settled helpers and host retirement (no timer retry), and the pause rules: Stop, failure, usage limit, restart, unknown delivery, contention, changed target or configuration (not window closure: the queue is the session's, Q10); the edit hold (Q9); Stop all across sessions (AC09). Revalidation and admission before every release; nothing is retargeted. Queue maintenance makes no inference and no workspace.

## Acceptance

- **AC02:** Queue maintenance and restored paused queues produce no inference or workspace creation. Refused persistence/capacity leaves prior entries and user text intact, under the defined limits.
- **AC03:** A successful originating run dispatches known-unsent entries in displayed order only after durable application completion and host retirement, with the preceding tour proposal represented correctly. Disposable first drafts gain neither implicit promotion nor these actions.
- **AC04:** Stop, failure, usage limits, restart and unknown delivery produce the defined retained paused state; closing or switching a window neither pauses nor hands over a queue, which belongs to its session. Changed authority, revision, configuration and context cannot retarget queued input or bypass admission; competing chats retain busy refusal and there is no automatic contention retry. Every window showing a session shows and edits its one queue; stale edits are refused, and a queued message held while it is edited is never sent nor skipped, and its hold lifts on Save or Cancel.

## Notes

Owner answers 2026-11-13 (on the RFC): Q1 fold the BPR-1930 whole-tour feedback buffer into the queue (first queued message, sent with the Ask more seed; the timer poll goes); Q2 the local draft's discussion is in scope; Q9 a message being edited is held, not sent or skipped, and the queue continues on Save or Cancel; Q10 the queue belongs to the session and is drained by the host, closing or switching windows does not pause it; Stop all (AC09, host side: stop every active turn and pause every queue).
