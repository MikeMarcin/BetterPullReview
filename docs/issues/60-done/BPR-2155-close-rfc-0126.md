---
schema: "implement-rfc.card.v1"
id: "BPR-2155"
title: "Verify and close RFC-BPR-0126"
status: "Done"
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
| **AC01:** Every in-scope chat accepts and displays multiple busy follow-ups with the defined Queue/Steer distinction, keyboard behavior and unsent editing controls; projections preserve the composer and captured subject. | Pass (dev): `application_ask_queue_public_path`, `application_agent_queue_windows_public_path`, `application_tour_chat_queue_{stop,whole_tour,local_draft}_public_path`; owner Accepts queue-stop-chat, queue-local-draft (queue-tour-v001) and steered-in-transcript (queue-tour-v003) | `4bda08895` |
| **AC02:** Queue maintenance and restored paused queues produce no inference or workspace creation. Refused persistence/capacity leaves prior entries and user text intact, under the defined limits. | Pass (dev): `application_agent_queue_host`, `core_agent_queue`, `application_ask_queue_public_path` | `d8f7fbf4b` |
| **AC03:** A successful originating run dispatches known-unsent entries in displayed order only after durable application completion and host retirement, with the preceding tour proposal represented correctly. Disposable first drafts gain neither implicit promotion nor these actions. | Pass (dev): `application_tour_feedback_queue_public_path`; fresh paths have no queued session kind and cannot be steered | `d8f7fbf4b` |
| **AC04:** Stop, failure, usage limits, restart and unknown delivery produce the defined retained paused state; closing or switching a window neither pauses nor hands over a queue, which belongs to its session. Changed authority, revision, configuration and context cannot retarget queued input or bypass admission; competing chats retain busy refusal and there is no automatic contention retry. Every window showing a session shows and edits its one queue; stale edits are refused, and a queued message held while it is edited is never sent nor skipped, and its hold lifts on Save or Cancel. | Pass (dev): `application_agent_queue_host`, `core_agent_queue`, `application_agent_queue_{windows,second_window_loading,close_runs_on,restart,finish_pauses}_public_path`; owner Accept queue-windows-v001 | `d8f7fbf4b`, `64365d031` |
| **AC05:** Qualified installed Claude and Codex App Server each accept a native correction on their existing conversation without transcript replay or a deliberate process restart. Unsupported configurations keep Queue and report Steer as unavailable without a paid discovery probe. | Pass: installed Claude Code 2.1.319 and Codex 0.167.0 App Server probes (2026-11-14), `application_agent_steer_unavailable_public_path`; owner Accepts steer-v002 | `98c2e9f30` |
| **AC06:** Queue and steering handoff survive crash/write/save races without losing or automatically duplicating input. Intermediate/duplicate results and dropped corrections cannot report premature success, retarget output, or hold a terminal run's lease indefinitely. Unknown input stays identifiable and never auto-resends. | Pass (dev): `application_agent_queue_handoff`, `application_agent_protocol_loop`, `application_agent_launch`, `application_agent_queue_tour_feedback_crash_public_path` | `64365d031`, `dd3b2664a` |
| **AC07:** All native parts of a steered run contribute once to the existing turn/session accounting and limits. Unreported costs remain unknown, and pending-input management adds no model request, summarizer or keep-alive call. | Pass (dev): `core_agent_steering`, `application_agent_protocol_loop`; the Claude probe's single cumulative figure | `64365d031` |
| **AC08:** The composed Qt flow exposes pending, paused, sent and uncertain states and recovery with reachable Stop and keyboard/accessibility actions, while retaining the existing proposal-acceptance and publication boundaries. | Pass (dev): `application_agent_steer_sent_public_path`, `qt_agent_steering_fixture`, the tour chat queue courses, `application_tour_feedback_queue_public_path`; owner Accept queue-sent-v001 | `4bda08895` |
| **AC09:** One Stop all control in the status bar's agent footer, beside the agent picker, in a fixed slot shown while any agent turn runs or any message is queued, stops every active turn across sessions and pauses every queue, so nothing starts next; it is reachable by keyboard and named to assistive technology. | Pass (dev): `qt_agent_footer`, `application_review_agent_runtime` (`test_stop_all_is_the_hosts`); Stop all accepted in prompts-v004 | `4bda08895` |

## Outcome and work

Reconcile the live documents this RFC changes: busy-send legality in RFC-BPR-0096, accepted steering in RFC-BPR-0103's turn boundary, cost attribution and composer actions in RFC-BPR-0101, and (if Q1 folds it in) RFC-BPR-0092's whole-tour feedback during the first draft; update `docs/ux/ai-experience.md` and the feature allocation in `docs/architecture/system-architecture.md`; the owner's answers to Q1 to Q18 and Stop all are recorded on the RFC (2026-11-13); reconcile RFC-BPR-0092 for Q1's fold-in; run the documentation gate.

## Closeout (2026-11-16)

Every criterion passes; the [verification record](../../verification/RFC-BPR-0126.md)
has the evidence and the integration pass (30 of 30 focused courses at lane
`76f59f17a`, Debug, macOS). Owner look queue-tour-v003 `steered-in-transcript`
Accepted (decision `a669056b-163a-43c6-a496-a277de4a1143`, read from session
`0f0bf2f5bd323f318366b52cf0e71dc0`), which completed [[BPR-2154]].

Reconciled: RFC-BPR-0096 (busy-send legality), RFC-BPR-0103 (accepted
steering in the turn boundary), RFC-BPR-0101 (composer actions, a steered
run's cost), RFC-BPR-0092 (Q1: first-draft feedback folded into the queue),
`docs/ux/ai-experience.md` (Follow-ups while an agent works) and
`docs/architecture/system-architecture.md` (session queue host, window
runtimes, delivery rule, the AI feature row). Owner answers Q1 to Q18 and
Stop all were already on the RFC (2026-11-13). RFC-BPR-0126 is
Auto-Verified; its index row is closed.

Known limits, recorded rather than carded (each is the behaviour the owner
saw and accepted, or a compatibility note, not a criterion miss): a steered
message keeps Sent to agent in the pending list until its turn ends;
whole-tour feedback during the first draft and a local draft's first
description draft queue but cannot be steered; a build older than
`4bda08895` cannot open a local draft saved with a steered message.
