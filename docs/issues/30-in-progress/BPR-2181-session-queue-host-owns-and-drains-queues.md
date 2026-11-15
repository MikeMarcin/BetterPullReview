---
schema: "implement-rfc.card.v1"
id: "BPR-2181"
title: "The session queue host owns and drains every session's queue; windows only present it"
status: "In Progress"
rfcs: ["RFC-BPR-0126"]
depends_on: []
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2181--BPR-2181--a919bdd13421.json", "origin": "owner, 2026-11-14 (relayed): \"Why does a queue have anything to do with a window. Queues should be managed by something like a session manager at a higher level and have nothing to do with the UI, the UI is just a presentation layer\"; first critical review of the queue at 03aae05cb (B1, B2, N1 to N7); second critical review of the queue (2026-11-14); Prompts settings review B1", "criteria": ["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08","AC09","AC10","AC11","AC12","AC13","AC14","AC15"]}
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

Origin: the owner's direction above; the first critical review of the queue
at `03aae05cb` (B1, B2, N1 to N7); the second critical review of the queue
(2026-11-14); and the Prompts settings review's B1. Each criterion has its
own course.

- **AC01 (first review B1):** The application-layer session queue host owns
  every session's queue and alone decides when its next message goes. It
  starts it through a profile-reachable turn runner, only on the previous
  run's typed finish and retirement on the decision lane; no window has to be
  open, free or loaded, no timer watches for retirement, and turn output for a
  session no window shows is kept in its transcript. Windows subscribe to its
  semantic deltas and send it commands. Course: a host test with zero windows
  drains a queue; a public path with the session's window closed.
- **AC02 (first review B2):** A run's finish carries its outcome; failure,
  usage limit and Stop pause the queue with that reason (a core mapping from
  outcome to pause). Course: a failing fixture agent leaves `Paused ==
  failed` with the second entry still queued; a usage limit pauses as usage
  limit. BPR-2151's AC04 evidence cited only a helper test for these and was
  overstated.
- **AC03 (first review N1):** A release returns a typed admission with its
  run id, and that run's completion settles it; a refused material read, a
  slash command or a slow retirement cannot leave a message Sending. Course.
- **AC04 (first review N2, N5):** A delivery-unknown message after a crash can
  be discarded, and an unsent entry goes back to Queued, both by core
  transitions. Course.
- **AC05 (first review N3; second review):** A refused store write is returned
  as a refusal and the composer keeps its text; queue store writes run on the
  host's effect path, not synchronously on the GUI thread. Course.
- **AC06 (first review N7):** Sessions and queued inputs are typed in core
  (session identity, the threads an ask carries), not strings built in
  composition. Course.
- **AC07:** A run that ends while a second window is still loading the pull
  request retires, and the session's queue goes on. Course:
  `application_agent_queue_second_window_loading_public_path`.
- **AC08 (second review):** Switching pull request mid-turn does not wedge
  the old session: its Sending entry and runner state settle from that run's
  retirement, per session, not per window. Course: switch mid-turn.
- **AC09 (second review; Q10):** Closing a window neither pauses a queue nor
  hands it over. Once the agent runtime is the profile's (slice 6), a close
  does not touch a running turn at all. Until then, a turn the close stops
  returns its message to the front, queued (core `return_unsent`), and the
  queue goes on only once that run has really retired; no other window starts
  the next turn before then. Stop pauses only when the reviewer presses Stop
  or Stop all. Course: close mid-turn
  (`application_agent_queue_windows_public_path`).
- **AC10 (second review):** Queue-versus-send is a core decision over the
  queue and the run state, not the view's; a Return in the gap before a turn
  shows working is queued and never credited to a queued entry. Course.
- **AC11 (second review):** Tour feedback never waits forever: shown and
  released by the host, returned to the chat as not sent after a crash (and
  its queue resumable), and the notice says only what will happen. Course.
- **AC12 (second review):** A stale refusal note is never read as a release
  verdict; Resume cannot send a message twice. Course.
- **AC13 (Prompts review B1):** Prompt facts are assembled once, outside the
  widget; the tab and the tour side chat are handed decided offers, and a
  queued release carries its decided expansion, so the tab never decides
  again. Course: a prompt conditioned on pushes since the review, pressed,
  reaches the agent.
- **AC14:** `agent_queue_host::steer(session, id)`: holds the message, offers
  it to the running turn; sent, it leaves the queue; otherwise it goes next
  and the reason is returned. Course.
- **AC15 (first review N6):** Stop all is a host operation across the
  profile's sessions and runs, not the footer's over one window. Course.

## Plan (slices)

Approved by the coordinator 2026-11-14, with slice 6 moved first after
slice 1.

1. Done (`c65c59df0`, AC09 corrected in the next commit): AC07, AC09, part
   of AC01 and AC14.
2. AC01 rest: a profile-level agent runtime, so a session no window shows
   still runs and keeps its transcript, a window close does not touch a
   running turn, and the Agent tab stops being the send transport.
   Application layer done (`c1c80c1e6`, see Evidence); composition into
   the app not started.
3. AC02, AC03, AC12: typed finish and admission with run id; core
   outcome-to-pause mapping; failure and usage-limit courses.
4. AC04, AC05: discard uncertain (back-to-queued done as `return_unsent`);
   store refusal; store writes on the effect path.
5. AC06, AC08, AC10, AC15: typed session identity and inputs; per-session
   runner state; core queue-versus-send; Stop all on the host.
6. AC13, AC11: facts assembled outside widgets, decided expansions carried by
   releases; tour feedback through the host. With it, [[BPR-2184]]'s N4
   (`quick_ask_facts` sets each thread's `Changed` and `CommitsSince` from
   `core::thread_baseline` and `core::thread_progress_since`) and N5 (report
   identity: a stable turn identity and structured byline, reports keyed by
   it, dropped when their tab closes), recorded on BPR-2184 when done.

## Evidence

- **AC01 (partial, slice 2 application layer, `c1c80c1e6`):**
  `application::review_agent_runtime` and the `review_agent_runtimes`
  registry (`src/application/include/bpr/application/review_agent_runtime.hpp`).
  There is one runtime per review session, owned by the profile. The
  registry offers it to `agent_session_runner`, whose
  `agent_turn_runner_port::steer` is unchanged. A runtime runs its turns on
  machinery the wiring composes for it (`review_agent_turn_port`). It
  builds a released typed message's request from the review snapshot it
  holds, and it keeps every turn no window saw end (`unshown_turns`). It
  lives while its queue can drain or a turn runs, and the registry lets it
  go off its own callback once neither holds and no window shows it.
  Course: `application_review_agent_runtime`. A queue with zero windows
  drains in order, each request comes from the snapshot, and the transcript
  keeps all three turns. A window detaching neither cancels nor shuts down
  the running turn. A quick ask with no window stays queued and unpaused
  until AC13 gives releases a decided expansion. Still open for AC01: the
  app composition. Each runtime gets its own invocation stack (an epoch
  host holding the review's accepted handoff for workspaces, a ledger for
  session admission, and the process chain), windows attach and adopt
  snapshots, the Agent tab sends through `review_agent_runtime::dispatch`,
  and a close stops touching a running turn. The public path is to queue
  two messages, close the window, let the turns run, then reopen and see
  them.
- **AC07:** `application_agent_queue_second_window_loading_public_path`
  passes (slice 1, `c65c59df0`). The runtime registry does not change it,
  because windows still run their own agent hosts until the composition
  lands.
