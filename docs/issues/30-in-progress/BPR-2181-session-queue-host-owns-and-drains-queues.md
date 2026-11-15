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
   Application layer done (`c1c80c1e6`, see Evidence). The epoch actor
   registry admits a profile-owned host (`9c7672579`), bounded by the queue
   host's session limit (`03a540b91`). The lent lease landed
   (`0aff5a7be`): `github_application_host` lends the runtime a review
   authority in its own name and ends it on identity invalidation, on a 404
   or refused permission for its pull request, on shutdown, or when the
   runtime lets go, and an ended authority pauses the queue with
   `lost_access` and starts no turn (`32fb55c83`). The private-run port is
   now free of Qt (`application::scoped_private_agent_runs`; the window's
   `private_review_workflow` delegates to it), the boundary fix the
   architecture asks for. Composition into the app is next.
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
- **AC01 (partial, slice 2, `9c7672579`), the composition's open risks
  checked:** (1) The epoch actor registry did refuse a context that is not
  a window. It admitted only the profile's live window observers, and every
  consumer took one of the profile's 32 window places. It now admits a
  profile-owned consumer (`attach_profile_owned`). That consumer is current
  until it retires or the registry closes, and it is bounded apart from the
  windows (`maximum_profile_owned_epoch_consumers`). `epoch_application_host`
  attaches as one through its own constructor. Course `application_epoch_host`
  (`profile_owned_host_holds_its_review_after_the_window_closes`): a
  profile-owned host accepts the review with no window, reads it after the
  window closed, and never takes a window's place. (2) Two ledger hosts on
  one review file do not clash. Admission is one `BEGIN IMMEDIATE`
  transaction over the scope's `agent_session_sequence` row, and every
  connection waits up to 2 s for a busy writer. (3) A third conflict the
  same course pins: the epoch host ingests only under a live lease, even
  for objects the profile cache already holds, and a lease is bound to the
  holder's application session. The window's lease is its read host's,
  bound to the window's session and revoked with it. The runtime therefore
  needs a lease lent in its own name by the GitHub host, and the profile
  must revoke it when the account or identity changes. That is the next
  step, at its owner (`github_window_read_host`, `github_application_host`).
  The profile-owned consumers take the queue host's session limit,
  `core::maximum_queued_sessions` (32), which `agent_queue_host::enqueue`
  now enforces (`03a540b91`, course `application_agent_queue_host`).
- **AC01 (partial, slice 2, `0aff5a7be` and `32fb55c83`), the runtime's
  own authority:** `github_application_host::lend_review_authority` lends an
  attached window's accepted pull request to a holder that is not a window.
  The handoff names the holder's application session, and its smart-Git
  lease is bound to that session. The lease is minted over the session's
  account transport, or over the credential-free one when signed out. The
  lease types moved to the internal `git_smart_authority_lease.hpp`, which
  the read host and the GitHub host share. The host ends a loan when the
  identity it was lent under is invalidated, when any attached window's read
  of that pull request under the same account answers 404 or refused
  permission, and at shutdown. It revokes every issued transport at once
  and then posts the holder's callback to the decision lane. The 404 signal
  is a new read-host observer (`observe_access_loss`), separate from
  `invalidate_epoch_authority`. Releasing the lease revokes it and tells
  nobody. The lender switching pull request or closing ends nothing. An
  account change, a generation change and a profile-revision change each
  invalidate the identity first, before the next session names its account
  or revision, and that is the moment the lease has to stop. So all three
  report one cause, `identity_changed`, and each has its own course. Course
  `application_github_host`: seven `test_a_lent_review_authority_*` cases
  (the holder's own lease and let-go, sign-out, account change, generation
  change, profile revision, pull request inaccessible, host closing).
  Runtime side: `review_agent_turn_port::authorized` gates every turn.
  `review_agent_runtimes::authority_ended` pauses the session's queue with
  the new `core::queue_pause::lost_access` ("Paused: this pull request can
  no longer be read with your account") and cancels the running turn.
  Course `application_review_agent_runtime`
  (`test_a_lost_authority_pauses_the_queue_and_starts_nothing`). Rerun on
  the dev tree at `32fb55c83` with the app built:
  `application_ask_menu_public_path`, `application_ask_queue_public_path`,
  `application_tour_feedback_queue_public_path`,
  `application_agent_queue_windows_public_path` and
  `application_agent_queue_second_window_loading_public_path` pass, along
  with `application_agent_queue_host`, `application_epoch_host`,
  `application_github_window_read_host` and `core_agent_queue`.
- **AC01 (partial, slice 2), the private-run boundary:**
  `application::scoped_private_agent_runs`
  (`private_review_application_host.hpp`) runs `admit`, `claim` and
  `finish` over a scope lookup under the ledger-file gate, with no Qt. A
  claim opens its own connection. `private_review_workflow` now delegates
  its `private_agent_run_port` to it, so the window keeps the panel and the
  run boundary is shared with the profile's runtime (coordinator,
  2026-11-14). Course `application_private_review_host`
  (`scoped_runs_without_a_panel`): a turn through it is admitted, claimed
  and finished durably, and a scope the lookup does not find is refused
  before any process starts.
- **AC05 (store, found under this card):** the agent queue store never
  saved a queue. Its entries cascaded from their queue, a cascade is a
  trigger program, and the hardened connection sets the trigger depth to 0,
  so every save failed with "triggers nested too deep". The store now
  deletes the entries explicitly, and the reference has no cascade. A file
  with the old tables can only hold them empty, so it is dropped and made
  again on open. Course `sqlite_agent_setup_storage`: every
  `core::queue_pause` value, `lost-access` included, saves and reloads as
  itself, and a file with the old cascading tables saves twice and reads
  back. Restored queues (AC04, AC11) were therefore never read from disk
  before this fix. Courses that claimed otherwise did not reach the store.
- **Rerun at this round's head (dev tree, app built):**
  `application_ask_menu_public_path`, `application_ask_addressed_public_path`,
  `application_ask_report_public_path`, `application_ask_queue_public_path`,
  `application_tour_feedback_queue_public_path`,
  `application_agent_queue_windows_public_path`,
  `application_agent_queue_second_window_loading_public_path`,
  `application_agent_queue_host`, `application_review_agent_runtime`,
  `application_private_review_host`, `sqlite_agent_setup_storage` and
  `agent_workspace_ownership` pass.
- **AC07:** `application_agent_queue_second_window_loading_public_path`
  passes (slice 1, `c65c59df0`). The runtime registry does not change it,
  because windows still run their own agent hosts until the composition
  lands. Rechecked 2026-11-14 at `9c7672579`: nothing composed changed, so
  that evidence stands.
