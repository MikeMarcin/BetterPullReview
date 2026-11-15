---
schema: "implement-rfc.card.v1"
id: "BPR-2181"
title: "The session queue host owns and drains every session's queue; windows only present it"
status: "In Progress"
rfcs: ["RFC-BPR-0126"]
depends_on: []
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2181--BPR-2181--a919bdd13421.json", "origin": "owner, 2026-11-14 (relayed): \"Why does a queue have anything to do with a window. Queues should be managed by something like a session manager at a higher level and have nothing to do with the UI, the UI is just a presentation layer\"; critical architecture review of 03aae05cb (B1, B2, N1 to N7)", "criteria": ["AC01","AC02","AC03","AC04","AC05","AC06","AC07"]}
---

# BPR-2181: The session queue host owns and drains every session's queue; windows only present it

Part of [RFC-BPR-0126](../../rfcs/RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md).
A correction to [[BPR-2151]], whose AC04 evidence was overstated: the queue
there was decided and drained by windows (the host stopped when no window
showed a session), and only `queue_pause::none` was ever settled.

The owner, verbatim: "Why does a queue have anything to do with a window.
Queues should be managed by something like a session manager at a higher
level and have nothing to do with the UI, the UI is just a presentation
layer".

## Acceptance

- **AC01:** The application-layer session queue host owns every session's
  queue and alone decides when its next message goes. It starts it through a
  profile-reachable turn runner, only on the previous run's own typed finish
  and retirement on the decision lane; no window has to be open, free or
  loaded, and no timer watches for retirement. Windows subscribe to its
  semantic deltas and send it commands (add, edit-hold, reorder, remove,
  pause and resume, Steer, Stop all across the profile). A host test with
  zero windows drains a queue.
- **AC02:** A run's finish carries its outcome: failure, usage limit and Stop
  pause the queue with that reason (a core mapping from outcome to pause);
  a public-path course shows failure and usage limit.
- **AC03:** A release returns a typed admission with its run id, and that
  run's completion settles it; a turn that never starts (a refused read, a
  slash command) cannot leave a message Sending.
- **AC04:** After a crash a delivery-unknown message can be discarded (a core
  transition), and an unsent entry goes back to Queued by a core transition.
- **AC05:** A refused store write is returned as a refusal and the composer
  keeps its text.
- **AC06:** Sessions and queued inputs are typed in core (session identity,
  the threads an ask carries), not strings built in composition.
- **AC07:** A run that ends while a second window is still loading the pull
  request retires, and the session's queue goes on
  (`application_agent_queue_second_window_loading_public_path`). Turn output
  for a session no window shows is kept in its transcript.

## Plan (slices)

1. Workspace owners outlive cache sessions; the agent invocation host's
   retirement event; the queue host drains itself through a runner port and
   settles on retirement; the second-window-loading course (AC07 first half,
   part of AC01).
2. Typed finish and pause mapping in core; failure and usage-limit course
   (AC02); typed admission with run id (AC03).
3. Core transitions: discard uncertain, back to queued; store refusal (AC04,
   AC05).
4. Typed session identity and inputs (AC06); Stop all as a host operation.
5. The profile-level agent runtime: the turn runner reaches an agent
   invocation host without a window, transcripts kept for unshown sessions
   (AC01, AC07); the Agent tab stops being the send transport.
