---
schema: "implement-rfc.card.v1"
id: "BPR-2152"
title: "Queued and steered input survives crashes and races without loss or automatic resend"
status: "In Progress"
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

- From BPR-2153's review (2026-11-14): a correction's own reply is bounded
  (an unanswered App Server `turn/steer` is unknown after 30 s, and a run
  with an unknown correction sets `agent_run_result::SteerDeliveryUnknown`).
  The bound on the whole run under delivery uncertainty (AC06: a terminal
  run must not hold its lease indefinitely) is this card's, not built there.

## Design as built (2026-11-15)

Coordinator assignment 2026-11-15: BPR-2152, then BPR-2154. Built on
[[BPR-2181]]'s profile queue host and runtimes and [[BPR-2153]]'s native
steering.

- **Claim before any write.** `core::claim` takes the exact entry at the
  revision shown: it becomes Sending, bound in a `core::queued_claim` (route
  release or steer, the BPR run, BPR's agent session of the native
  conversation, and later the input identity), and is no longer eligible for
  release, edit or removal. The queue host stores the claim and starts the
  turn (or offers the correction) only when the store's verdict comes back on
  the decision lane (`agent_queue_host::commit_then`); the store keeps the
  claim in a new `claim` column of `agent_queue_entries`.
- **Failure before a write leaves it unsent.** A claim the store refused, a
  Stop all while it was being stored, a turn that never started
  (`failed_to_start`, `workspace_unavailable`, input refused as too large) or
  a correction the turn never wrote goes back first among the unsent with its
  identity, and the queue pauses where it would otherwise repeat. Nothing is
  written to the agent.
- **A possible write without an outcome is Delivery unknown.** A release
  whose run failed after launch with no sign at all that the agent took it
  (no output, no tool step, no answer) is `queued_finish::unknown`; a
  correction written and never named by its run's end stays Delivery unknown;
  a crash restores every Sending or Sent entry as Delivery unknown with its
  claim. The queue pauses, nothing is resent, and only Discard removes it.
- **Receipts before the queue advances.** A release's claim names its run
  once admitted (`core::bind_run`); a correction the runtime wrote is Sent
  under its input identity (`core::acknowledge`). The run's end settles each
  correction claimed for that run by receipt (`core::settle_steers`, from
  `agent_run_result::SteeredInputs` in `review_agent_runtime::finish`):
  consumed leaves, not taken goes back first, any other pauses as Delivery
  unknown. The next release is itself a claim stored whole with the queue,
  so it starts only after the settlement is durable. Late, repeated or stray
  answers change only the record they name, once.
- **Bounded run under uncertainty.** A Claude answer that leaves a correction
  open with `queued_turn_count` above zero holds the run for its successor
  turn for at most `agent_launch_request::SteerSuccessorMilliseconds`
  (30 s) until the successor's first frame; then the correction is unknown,
  the turn ends on the answer it gave, and the connection is retired and its
  process ended (`agent_protocol_decoder::abandon_successor`). With
  [[BPR-2153]]'s 30 s bound on an unanswered `turn/steer`, a dropped
  correction cannot hold a run, or its workspace lease, indefinitely.
- **Steer is asynchronous.** `agent_queue_host::steer` returns only the
  press's refusal; the window is told the outcome once the claim is stored
  (`agent_queue_steer_done`). A correction handed to the turn shows Sending,
  then Sent to agent, until its run settles it.
- **A sign the agent took it.** Delivery unknown applies only when the run
  failed (`nonzero_exit`, `protocol_failure`, `timeout`) and the runtime saw
  nothing of it: no stream event (a heartbeat counts), no answer, no tool
  step and no correction. A run that showed anything failed as its own, and
  its message went (unchanged from [[BPR-2181]] AC02).
- **Pause queue and a claim in flight.** A claim whose store write is still
  out is recalled (unsent, first) by Stop all, a lost access or an agent that
  became busy; Pause queue stops what comes after the message already shown
  as Sending, not that message.

## Evidence (2026-11-15, dev Debug, macOS)

- AC06 course `application_agent_queue_handoff` (new): claim stored before
  the turn starts, receipts stored, a repeated retirement settles nothing
  twice; a crash after the claim restores Delivery unknown with its binding,
  Resume refused, Discard then only the waiting message goes; a refused claim
  and Stop all during the claim write nothing; Pause queue does not recall
  the message shown Sending; corrections claimed and stored before they are
  offered, settled by receipt (taken leaves, not taken back first with its
  identity, unnamed uncertain and paused), late and stray answers change
  nothing; a release that died silent is uncertain; an intermediate or
  repeated result never completes a steered turn, a dropped correction is
  abandoned and its connection never reused; claims survive the store's
  encoding.
- `application_agent_protocol_loop`: a dropped correction ends the run on
  its answer with the correction unknown, the connection retired and the
  process ended (`test_a_dropped_correction_cannot_hold_the_run`).
- `core_agent_queue` (`test_claims_and_settled_corrections`),
  `application_agent_queue_host` (`test_steer`, asynchronous),
  `application_agent_invocation_host` (a runtime Steer's unwritten
  correction comes back as the same message, a written one is Sent under its
  input identity), `sqlite_agent_setup_storage`.
- Public paths, passing with the change: `application_agent_steer_sent_public_path`
  (the steered message shows Sent to agent while its turn runs and leaves the
  pending list when its run names it), `application_agent_steer_unavailable_public_path`,
  `application_ask_queue_public_path` (the Steer outcome is waited for),
  `application_tour_feedback_queue_public_path`,
  `application_agent_queue_windows_public_path`,
  `application_agent_queue_second_window_loading_public_path`,
  `application_agent_queue_close_runs_on_public_path`,
  `application_agent_queue_restart_public_path`,
  `application_agent_queue_tour_feedback_crash_public_path`,
  `application_agent_queue_finish_pauses_public_path`,
  `application_review_agent_runtime`, `qt_drawer_surfaces_fit_scale_100`.
