---
schema: "implement-rfc.card.v1"
id: "BPR-2155"
title: "Verify and close RFC-BPR-0126"
status: "Backlog"
rfcs: ["RFC-BPR-0126"]
depends_on: ["BPR-2151","BPR-2152","BPR-2153","BPR-2154"]
closeout_for: ["RFC-BPR-0126"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2151--BPR-2155--573d6ab3248e.json","milestone_plan":{"id":"queue-and-steer-v1","path":"docs/rfcs/plans/queue-and-steer-v1/milestone-plan.json"}}
---

# BPR-2155: Verify and close RFC-BPR-0126

RFC base: `dbdecf9a32c9fc7cdd28122721dc61011236d60e`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **AC01:** Every in-scope chat accepts and displays multiple busy follow-ups with the defined Queue/Steer distinction, keyboard behavior and unsent editing controls; projections preserve the composer and captured subject. | Not run | No producing revision |
| **AC02:** Queue maintenance and restored paused queues produce no inference or workspace creation. Refused persistence/capacity leaves prior entries and user text intact, under the defined limits. | Not run | No producing revision |
| **AC03:** A successful originating run dispatches known-unsent entries in displayed order only after durable application completion and host retirement, with the preceding tour proposal represented correctly. Disposable first drafts gain neither implicit promotion nor these actions. | Not run | No producing revision |
| **AC04:** Stop, failure, usage limits, closure and restart produce the defined retained paused state. Changed authority, revision, configuration and context cannot retarget queued input or bypass admission; competing chats retain busy refusal and there is no automatic contention retry. Multiple windows agree on one queue; stale edits, closure and takeover preserve the defined ownership. | Not run | No producing revision |
| **AC05:** Qualified installed Claude and Codex App Server each accept a native correction on their existing conversation without transcript replay or a deliberate process restart. Unsupported configurations keep Queue and report Steer as unavailable without a paid discovery probe. | Not run | No producing revision |
| **AC06:** Queue and steering handoff survive crash/write/save races without losing or automatically duplicating input. Intermediate/duplicate results and dropped corrections cannot report premature success, retarget output, or hold a terminal run's lease indefinitely. Unknown input stays identifiable and never auto-resends. | Not run | No producing revision |
| **AC07:** All native parts of a steered run contribute once to the existing turn/session accounting and limits. Unreported costs remain unknown, and pending-input management adds no model request, summarizer or keep-alive call. | Not run | No producing revision |
| **AC08:** The composed Qt flow exposes pending, paused, sent and uncertain states and recovery with reachable Stop and keyboard/accessibility actions, while retaining the existing proposal-acceptance and publication boundaries. | Not run | No producing revision |

## Outcome and work

Reconcile the live documents this RFC changes: busy-send legality in RFC-BPR-0096, accepted steering in RFC-BPR-0103's turn boundary, cost attribution and composer actions in RFC-BPR-0101, and (if Q1 folds it in) RFC-BPR-0092's whole-tour feedback during the first draft; update `docs/ux/ai-experience.md` and the feature allocation in `docs/architecture/system-architecture.md`; record each owner answer to Q1 to Q18 on the RFC; run the documentation gate.
